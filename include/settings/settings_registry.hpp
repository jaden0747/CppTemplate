#pragma once

#include "settings/dirty_tracker.hpp"

#include <nlohmann/json.hpp>

#include <cstdint>
#include <fstream>
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

    static SettingsRegistry& instance()
    {
        static SettingsRegistry s_instance;
        return s_instance;
    }

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

    void loadJson(const std::string& path)
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

    void saveJson(const std::string& path) const
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
    nlohmann::json getDefaultJson(const std::string& itemName) const
    {
        auto it = m_items.find(itemName);
        if (it == m_items.end() || !it->second.defaulter)
            return nlohmann::json{};
        return it->second.defaulter();
    }

    // Revert an entire item to its default values. Applies via loader/onLoaded
    // so change callbacks and DirtyTracker fire as on any other update.
    bool resetItem(const std::string& itemName)
    {
        auto it = m_items.find(itemName);
        if (it == m_items.end() || !it->second.defaulter)
            return false;
        it->second.loader(it->second.ptr, it->second.defaulter());
        if (it->second.onLoaded)
            it->second.onLoaded(it->second.ptr);
        return true;
    }

    // Revert a single member of an item to its default value.
    bool resetItemValue(const std::string& itemName, const std::string& memberName)
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

    const std::map<std::string, Entry>& getItems() const { return m_items; }

    void setAutoSave(const std::string& path) { m_autoSavePath = path; }
    const std::string& getAutoSavePath() const { return m_autoSavePath; }

    void registerEnumOptions(const std::string& itemName, const std::string& fieldName,
                             std::vector<std::string> options)
    {
        m_enumOptions[itemName][fieldName] = std::move(options);
    }

    const std::vector<std::string>* getEnumOptions(const std::string& itemName,
                                                   const std::string& fieldName) const
    {
        auto it = m_enumOptions.find(itemName);
        if (it == m_enumOptions.end()) return nullptr;
        auto it2 = it->second.find(fieldName);
        if (it2 == it->second.end()) return nullptr;
        return &it2->second;
    }

private:
    SettingsRegistry()                                   = default;
    SettingsRegistry(const SettingsRegistry&)            = delete;
    SettingsRegistry& operator=(const SettingsRegistry&) = delete;

    std::map<std::string, Entry>                                           m_items;
    std::map<std::string, std::map<std::string, std::vector<std::string>>> m_enumOptions;
    std::string                                                            m_autoSavePath;
};
