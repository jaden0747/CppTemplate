#pragma once

// Example settings struct — NOT part of the pf package. See app_config.hpp.

#include <pf/settings/dirty_tracker.hpp>
#include <pf/settings/settings_item.hpp>

#include <nlohmann/json.hpp>
#include <array>
#include <string>

namespace demo
{

struct RenderSettings : pf::DirtyTracker
{
    std::array<float, 4> clearColor    = {0.1f, 0.1f, 0.1f, 1.0f};
    bool                 wireframe     = false;
    std::string          shadowQuality = "medium";  // "low" | "medium" | "high"
    int                  maxLights     = 8;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(RenderSettings,
        clearColor, wireframe, shadowQuality, maxLights)
};

// One definition shared across every translation unit / binary (C++17 inline
// variable). Self-registers with pf::SettingsRegistry at static-init time.
inline pf::SettingsItem<RenderSettings> g_render{"RenderSettings"};

} // namespace demo
