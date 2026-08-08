#pragma once

#include <pf/settings/dirty_tracker.hpp>

#include <nlohmann/json.hpp>
#include <libxml/tree.h>

#include <cstdint>
#include <functional>
#include <iostream>
#include <map>
#include <string>
#include <type_traits>
#include <vector>

/// ---------------------------------------------------------------------------
/// SettingsRegistry
///
/// Singleton global registry mapping string keys → typed settings objects.
///
/// Items register themselves at static-init time via SettingsItem<T> globals,
/// matching the original Item<T> / CodingManager::addItem() pattern.
/// ---------------------------------------------------------------------------
namespace pf
{

class SettingsRegistry
{
public:
    struct Entry
    {
        void*                                              ptr;
        std::function<void(void*, const nlohmann::json&)> loader;
        std::function<nlohmann::json(const void*)>        saver;
        std::function<void(void*)>                        onLoaded;
        std::function<nlohmann::json()>                   defaulter; // default-constructed T as JSON
    };

    static SettingsRegistry& instance();

    template <typename T>
    void add(const std::string& key, T* obj)
    {
        if (m_items.count(key) != 0U)
        {
            std::cerr << "[SettingsRegistry] Duplicate key: \"" << key << "\" — ignoring.\n";
            return;
        }

        Entry entry;
        entry.ptr       = obj;
        entry.loader    = [](void* p, const nlohmann::json& j) { j.get_to(*static_cast<T*>(p)); };
        entry.saver     = [](const void* p) -> nlohmann::json { return *static_cast<const T*>(p); };
        entry.defaulter = []() -> nlohmann::json { return T{}; };

        if constexpr (std::is_base_of_v<DirtyTracker, T>)
        {
            entry.onLoaded = [](void* p) { static_cast<DirtyTracker*>(p)->dirty(); };
        }
        else
        {
            entry.onLoaded = nullptr;
        }

        m_items.emplace(key, std::move(entry));
    }

    void loadXml(const std::string& path);
    void saveXml(const std::string& path) const;

    template <typename T>
    bool setItemValue(const std::string& itemName, const std::string& memberName, const T& value)
    {
        auto it = m_items.find(itemName);
        if (it == m_items.end())
            return false;
        nlohmann::json j = it->second.saver(it->second.ptr);
        if (!j.contains(memberName))
            return false;
        j[memberName] = value;
        it->second.loader(it->second.ptr, j);
        if (it->second.onLoaded)
            it->second.onLoaded(it->second.ptr);
        return true;
    }

    template <typename T>
    bool getItemValue(const std::string& itemName, const std::string& memberName, T& outValue) const
    {
        const auto it = m_items.find(itemName);
        if (it == m_items.end())
            return false;
        const nlohmann::json j = it->second.saver(it->second.ptr);
        if (!j.contains(memberName))
            return false;
        try
        {
            outValue = j.at(memberName).get<T>();
        }
        catch (const nlohmann::json::exception&)
        {
            return false;
        }
        return true;
    }

    // Default-constructed value of an item (its struct's member initializers) as
    // JSON. Returns null json if the key is unknown.
    nlohmann::json getDefaultJson(const std::string& itemName) const;

    // Revert an entire item to its default values. Applies via loader/onLoaded
    // so change callbacks and DirtyTracker fire as on any other update.
    bool resetItem(const std::string& itemName);

    // Revert a single member of an item to its default value.
    bool resetItemValue(const std::string& itemName, const std::string& memberName);

    const std::map<std::string, Entry>& getItems() const { return m_items; }

    void setAutoSave(const std::string& path) { m_autoSavePath = path; }
    const std::string& getAutoSavePath() const { return m_autoSavePath; }

    void registerEnumOptions(const std::string& itemName, const std::string& fieldName,
                             std::vector<std::string> options);

    const std::vector<std::string>* getEnumOptions(const std::string& itemName,
                                                   const std::string& fieldName) const;

private:
    SettingsRegistry()                                   = default;
    SettingsRegistry(const SettingsRegistry&)            = delete;
    SettingsRegistry& operator=(const SettingsRegistry&) = delete;

    // XML <-> JSON conversion, schema-guided by a default-constructed instance's
    // JSON shape (see Entry::defaulter) so text content can be coerced back into
    // the right type (bool/number/string/array/object). Attributes and child
    // elements are treated interchangeably on read (child element wins if a
    // field is given as both); writes always use child elements.
    static nlohmann::json xmlToJson(xmlNode* node, const nlohmann::json& schema);
    static void           jsonToXml(xmlNode* parent, const std::string& name, const nlohmann::json& value);

    std::map<std::string, Entry>                                           m_items;
    std::map<std::string, std::map<std::string, std::vector<std::string>>> m_enumOptions;
    std::string                                                            m_autoSavePath;
};

} // namespace pf
