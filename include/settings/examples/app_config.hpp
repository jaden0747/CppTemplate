#pragma once

#include "settings/dirty_tracker.hpp"
#include "settings/settings_item.hpp"
#include "core/log_level.hpp"

#include <nlohmann/json.hpp>
#include <string>

struct AppConfig : DirtyTracker
{
    std::string appName      = "MyApp";
    int         windowWidth  = 1280;
    int         windowHeight = 720;
    bool        fullscreen   = false;
    float       targetFps    = 60.0f;
    float       uiFontSize   = 16.0f;
    std::string uiFontPath   = "resources/font/ComicMonoNF-Regular.ttf";
    LogLevel    logLevel     = LogLevel::info;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(AppConfig,
        appName, windowWidth, windowHeight, fullscreen, targetFps, uiFontSize, uiFontPath, logLevel)

    // Self-registers editor metadata at SettingsItem construction (static-init
    // time) — picked up automatically via SettingsItem's HasRegisterMetadata hook.
    static void registerMetadata(SettingsRegistry& reg, const std::string& key)
    {
        reg.registerEnumOptions(key, "logLevel", logLevelOptions());
    }
};

// One definition shared across every translation unit / binary (C++17 inline
// variable). Self-registers with SettingsRegistry at static-init time.
inline SettingsItem<AppConfig> g_app{"AppConfig"};
