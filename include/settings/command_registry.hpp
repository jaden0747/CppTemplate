#pragma once

#include "settings/settings_registry.hpp"

#include <cli/cli.h>

#include <memory>
#include <string>

/// ---------------------------------------------------------------------------
/// command_registry.hpp
///
/// Builds the CLI root menu using daniele77/cli.
///
/// Usage:
///   auto rootMenu = buildRootMenu();
///   // add your domain-specific commands to rootMenu ...
///   cli::Cli cli(std::move(rootMenu));
/// ---------------------------------------------------------------------------
inline std::unique_ptr<cli::Menu> buildRootMenu()
{
    auto rootMenu = std::make_unique<cli::Menu>("app");

    // ------------------------------------------------------------------
    // "coding" submenu — replaces CodingCommand (list / get / set / export)
    // ------------------------------------------------------------------
    auto codingMenu = std::make_unique<cli::Menu>("coding");

    codingMenu->Insert(
        "list",
        [](std::ostream& out, const std::string& itemName)
        {
            auto& items = SettingsRegistry::instance().getItems();
            auto  it    = items.find(itemName);
            if (it == items.end())
            {
                out << "\"" << itemName << "\" is not a valid coding item name\n";
                return;
            }
            out << itemName << "\n";
            for (auto& [key, _] : it->second.saver(it->second.ptr).items())
                out << "  " << key << "\n";
        },
        "List members of a specific coding item");

    codingMenu->Insert(
        "list",
        [](std::ostream& out)
        {
            for (auto& [key, _] : SettingsRegistry::instance().getItems())
                out << key << "\n";
        },
        "List all registered coding items");

    codingMenu->Insert(
        "get",
        [](std::ostream& out, const std::string& itemName)
        {
            auto& items = SettingsRegistry::instance().getItems();
            auto  it    = items.find(itemName);
            if (it == items.end())
            {
                out << "\"" << itemName << "\" is not a valid coding item name\n";
                return;
            }
            out << it->second.saver(it->second.ptr).dump(4) << "\n";
        },
        "Get a coding item as JSON");

    codingMenu->Insert(
        "get",
        [](std::ostream& out)
        {
            nlohmann::json root;
            for (auto& [key, entry] : SettingsRegistry::instance().getItems())
                root[key] = entry.saver(entry.ptr);
            out << root.dump(4) << "\n";
        },
        "Get all coding items as JSON");

    codingMenu->Insert(
        "set",
        [](std::ostream& out, const std::string& itemName,
           const std::string& memberName, const std::string& jsonValue)
        {
            auto& items = SettingsRegistry::instance().getItems();
            auto  it    = items.find(itemName);
            if (it == items.end())
            {
                out << "\"" << itemName << "\" is not a valid coding item name\n";
                return;
            }
            nlohmann::json j = it->second.saver(it->second.ptr);
            if (!j.contains(memberName))
            {
                out << "\"" << memberName << "\" is not a valid member of \"" << itemName << "\"\n";
                return;
            }
            try
            {
                j[memberName] = nlohmann::json::parse(jsonValue);
            }
            catch (const nlohmann::json::parse_error&)
            {
                j[memberName] = jsonValue;
            }
            it->second.loader(it->second.ptr, j);
            if (it->second.onLoaded)
                it->second.onLoaded(it->second.ptr);
            out << "OK\n";
        },
        "Set a coding member: set <item> <member> <json_value>");

    codingMenu->Insert(
        "export",
        [](std::ostream& out, const std::string& filename)
        {
            SettingsRegistry::instance().saveJson(filename);
            out << "Exported to \"" << filename << "\"\n";
        },
        "Export all coding items to a JSON file");

    rootMenu->Insert(std::move(codingMenu));

    rootMenu->Insert(
        "status",
        [](std::ostream& out)
        {
            out << "Server is running. Registered items:\n";
            for (auto& [key, _] : SettingsRegistry::instance().getItems())
                out << "  " << key << "\n";
        },
        "Show server status");

    rootMenu->Insert(
        "echo",
        [](std::ostream& out, const std::string& message)
        {
            out << message << "\n";
        },
        "Echo a message back");

    return rootMenu;
}
