#include "settings/settings_registry.hpp"

#include <fstream>
#include <iostream>

SettingsRegistry& SettingsRegistry::instance()
{
    static SettingsRegistry s_instance;
    return s_instance;
}

void SettingsRegistry::loadJson(const std::string& path)
{
    std::ifstream f(path);
    if (!f.is_open())
    {
        std::cerr << "[SettingsRegistry] Cannot open: \"" << path << "\"\n";
        return;
    }

    nlohmann::json root;
    try
    {
        root = nlohmann::json::parse(f, nullptr, true, /*ignore_comments=*/true);
    }
    catch (const nlohmann::json::parse_error& e)
    {
        std::cerr << "[SettingsRegistry] JSON parse error in \"" << path << "\": " << e.what() << "\n";
        return;
    }

    for (auto& [key, entry] : m_items)
    {
        if (!root.contains(key))
            continue;
        try
        {
            entry.loader(entry.ptr, root[key]);
            if (entry.onLoaded)
                entry.onLoaded(entry.ptr);
        }
        catch (const nlohmann::json::exception& e)
        {
            std::cerr << "[SettingsRegistry] Error loading \"" << key << "\": " << e.what() << "\n";
        }
    }
}

void SettingsRegistry::saveJson(const std::string& path) const
{
    nlohmann::json root;
    for (const auto& [key, entry] : m_items)
        root[key] = entry.saver(entry.ptr);

    std::ofstream f(path);
    if (!f.is_open())
    {
        std::cerr << "[SettingsRegistry] Cannot write: \"" << path << "\"\n";
        return;
    }
    f << root.dump(4) << '\n';
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
