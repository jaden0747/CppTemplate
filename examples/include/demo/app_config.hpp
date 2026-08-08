#pragma once

// ---------------------------------------------------------------------------
// Example application config — NOT part of the pf package.
//
// Downstream projects define their own settings structs like this one; the
// platform ships the machinery (pf::SettingsItem / pf::SettingsRegistry), not
// the schema. Lives in `namespace demo` so the app/platform boundary stays
// visible at every call site.
// ---------------------------------------------------------------------------

#include <pf/settings/dirty_tracker.hpp>
#include <pf/settings/settings_item.hpp>
#include <pf/log/log_level.hpp>

#include <nlohmann/json.hpp>
#include <string>

namespace demo
{

struct AppConfig : pf::DirtyTracker
{
    std::string  appName      = "MyApp";
    int          windowWidth  = 1280;
    int          windowHeight = 720;
    bool         fullscreen   = false;
    float        targetFps    = 60.0f;
    float        uiFontSize   = 16.0f;
    std::string  uiFontPath   = "resources/font/ComicMonoNF-Regular.ttf";
    pf::LogLevel logLevel     = pf::LogLevel::info;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(AppConfig,
        appName, windowWidth, windowHeight, fullscreen, targetFps, uiFontSize, uiFontPath, logLevel)

    // Self-registers editor metadata at SettingsItem construction (static-init
    // time) — picked up automatically via SettingsItem's HasRegisterMetadata hook.
    static void registerMetadata(pf::SettingsRegistry& reg, const std::string& key)
    {
        reg.registerEnumOptions(key, "logLevel", pf::logLevelOptions());
    }
};

// One definition shared across every translation unit / binary (C++17 inline
// variable). Self-registers with pf::SettingsRegistry at static-init time.
inline pf::SettingsItem<AppConfig> g_app{"AppConfig"};

} // namespace demo
