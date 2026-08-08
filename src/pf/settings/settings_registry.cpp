#include <pf/settings/settings_registry.hpp>

#include <libxml/parser.h>

#include <cstdlib>
#include <iostream>
#include <sstream>

namespace pf
{

namespace
{
std::string trim(const std::string& s)
{
    const auto begin = s.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos)
        return {};
    const auto end = s.find_last_not_of(" \t\r\n");
    return s.substr(begin, end - begin + 1);
}

std::vector<std::string> splitWhitespace(const std::string& s)
{
    std::vector<std::string> tokens;
    std::istringstream       iss(s);
    std::string              tok;
    while (iss >> tok)
        tokens.push_back(tok);
    return tokens;
}

xmlNode* findChildElement(xmlNode* parent, const std::string& name)
{
    if (!parent)
        return nullptr;
    for (xmlNode* child = parent->children; child; child = child->next)
        if (child->type == XML_ELEMENT_NODE && name == reinterpret_cast<const char*>(child->name))
            return child;
    return nullptr;
}

bool findAttribute(xmlNode* node, const std::string& name, std::string& out)
{
    xmlChar* val = xmlGetProp(node, reinterpret_cast<const xmlChar*>(name.c_str()));
    if (!val)
        return false;
    out = reinterpret_cast<const char*>(val);
    xmlFree(val);
    return true;
}

std::string elementText(xmlNode* node)
{
    xmlChar*    content = xmlNodeGetContent(node);
    std::string text     = content ? reinterpret_cast<const char*>(content) : std::string{};
    if (content)
        xmlFree(content);
    return trim(text);
}

// Coerce plain text (from an attribute or an element's text content) into a
// JSON value shaped like `schema` — the default-constructed value tells us
// whether this field is a bool/number/string/array so we know how to read it
// back (e.g. splitting whitespace-separated vector text into array elements).
nlohmann::json coerceLeaf(const std::string& text, const nlohmann::json& schema)
{
    try
    {
        if (schema.is_boolean())
            return text == "true" || text == "1";
        if (schema.is_number_integer())
            return static_cast<int64_t>(std::stoll(text));
        if (schema.is_number_float())
            return std::stod(text);
        if (schema.is_array())
        {
            const nlohmann::json elemSchema = schema.empty() ? nlohmann::json(0.0) : schema.at(0);
            nlohmann::json        arr        = nlohmann::json::array();
            for (const auto& tok : splitWhitespace(text))
                arr.push_back(coerceLeaf(tok, elemSchema));
            return arr;
        }
        return text; // string, or an enum serialized as a string
    }
    catch (const std::exception&)
    {
        return schema; // malformed text — fall back to the default value
    }
}

std::string formatScalar(const nlohmann::json& v)
{
    if (v.is_string())
        return v.get<std::string>();
    return v.dump(); // numbers/bool: reuse nlohmann's round-trip-safe formatting
}
} // namespace

SettingsRegistry& SettingsRegistry::instance()
{
    static SettingsRegistry s_instance;
    return s_instance;
}

// Recursively rebuild a JSON value from an XML element, guided by `schema`
// (a default-constructed instance's JSON shape). For object fields, a member
// is read from a same-named child element if present, otherwise from a
// same-named attribute — child elements win when both exist, since elements
// are the canonical form this registry writes.
nlohmann::json SettingsRegistry::xmlToJson(xmlNode* node, const nlohmann::json& schema)
{
    if (!node)
        return schema;

    if (schema.is_object())
    {
        nlohmann::json result = nlohmann::json::object();
        for (auto it = schema.begin(); it != schema.end(); ++it)
        {
            const std::string& key = it.key();
            if (xmlNode* child = findChildElement(node, key))
            {
                result[key] = xmlToJson(child, it.value());
                continue;
            }
            std::string attrVal;
            if (findAttribute(node, key, attrVal))
                result[key] = coerceLeaf(attrVal, it.value());
        }
        return result;
    }

    return coerceLeaf(elementText(node), schema);
}

// Mirrors xmlToJson: always writes child elements (never attributes), so
// re-saved files are uniform regardless of how the source file was authored.
void SettingsRegistry::jsonToXml(xmlNode* parent, const std::string& name, const nlohmann::json& value)
{
    if (value.is_object())
    {
        xmlNode* elem = xmlNewChild(parent, nullptr, reinterpret_cast<const xmlChar*>(name.c_str()), nullptr);
        for (auto it = value.begin(); it != value.end(); ++it)
            jsonToXml(elem, it.key(), it.value());
        return;
    }

    std::string text;
    if (value.is_array())
    {
        for (std::size_t i = 0; i < value.size(); ++i)
        {
            if (i != 0)
                text += ' ';
            text += formatScalar(value[i]);
        }
    }
    else
    {
        text = formatScalar(value);
    }

    xmlChar* encoded = xmlEncodeEntitiesReentrant(parent->doc, reinterpret_cast<const xmlChar*>(text.c_str()));
    xmlNewTextChild(parent, nullptr, reinterpret_cast<const xmlChar*>(name.c_str()), encoded);
    xmlFree(encoded);
}

void SettingsRegistry::loadXml(const std::string& path)
{
    xmlDocPtr doc = xmlReadFile(path.c_str(), nullptr, XML_PARSE_NOBLANKS);
    if (!doc)
    {
        std::cerr << "[SettingsRegistry] \"" << path << "\" not found — writing defaults.\n";
        saveXml(path);
        return;
    }

    xmlNode* root = xmlDocGetRootElement(doc);
    if (!root)
    {
        std::cerr << "[SettingsRegistry] \"" << path << "\" has no root element.\n";
        xmlFreeDoc(doc);
        return;
    }

    for (auto& [key, entry] : m_items)
    {
        xmlNode* itemNode = findChildElement(root, key);
        if (!itemNode)
            continue;
        try
        {
            const nlohmann::json schema = entry.defaulter();
            const nlohmann::json j      = xmlToJson(itemNode, schema);
            entry.loader(entry.ptr, j);
            if (entry.onLoaded)
                entry.onLoaded(entry.ptr);
        }
        catch (const std::exception& e)
        {
            std::cerr << "[SettingsRegistry] Error loading \"" << key << "\": " << e.what() << "\n";
        }
    }

    xmlFreeDoc(doc);
}

void SettingsRegistry::saveXml(const std::string& path) const
{
    xmlDocPtr doc  = xmlNewDoc(reinterpret_cast<const xmlChar*>("1.0"));
    xmlNode*  root = xmlNewNode(nullptr, reinterpret_cast<const xmlChar*>("Settings"));
    xmlDocSetRootElement(doc, root);

    for (const auto& [key, entry] : m_items)
        jsonToXml(root, key, entry.saver(entry.ptr));

    if (xmlSaveFormatFileEnc(path.c_str(), doc, "UTF-8", 1) == -1)
        std::cerr << "[SettingsRegistry] Cannot write: \"" << path << "\"\n";

    xmlFreeDoc(doc);
}

nlohmann::json SettingsRegistry::getDefaultJson(const std::string& itemName) const
{
    auto it = m_items.find(itemName);
    if (it == m_items.end() || !it->second.defaulter)
        return nlohmann::json{};
    return it->second.defaulter();
}

bool SettingsRegistry::resetItem(const std::string& itemName)
{
    auto it = m_items.find(itemName);
    if (it == m_items.end() || !it->second.defaulter)
        return false;
    it->second.loader(it->second.ptr, it->second.defaulter());
    if (it->second.onLoaded)
        it->second.onLoaded(it->second.ptr);
    return true;
}

bool SettingsRegistry::resetItemValue(const std::string& itemName, const std::string& memberName)
{
    auto it = m_items.find(itemName);
    if (it == m_items.end() || !it->second.defaulter)
        return false;
    nlohmann::json def = it->second.defaulter();
    if (!def.contains(memberName))
        return false;
    nlohmann::json j = it->second.saver(it->second.ptr);
    j[memberName] = def[memberName];
    it->second.loader(it->second.ptr, j);
    if (it->second.onLoaded)
        it->second.onLoaded(it->second.ptr);
    return true;
}

void SettingsRegistry::registerEnumOptions(const std::string& itemName, const std::string& fieldName,
                                           std::vector<std::string> options)
{
    m_enumOptions[itemName][fieldName] = std::move(options);
}

const std::vector<std::string>* SettingsRegistry::getEnumOptions(const std::string& itemName,
                                                                  const std::string& fieldName) const
{
    auto it = m_enumOptions.find(itemName);
    if (it == m_enumOptions.end()) return nullptr;
    auto it2 = it->second.find(fieldName);
    if (it2 == it->second.end()) return nullptr;
    return &it2->second;
}

} // namespace pf
