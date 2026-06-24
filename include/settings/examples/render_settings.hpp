#pragma once

#include "settings/dirty_tracker.hpp"
#include "settings/settings_item.hpp"

#include <nlohmann/json.hpp>
#include <array>
#include <string>

struct RenderSettings : DirtyTracker
{
    std::array<float, 4> clearColor    = {0.1f, 0.1f, 0.1f, 1.0f};
    bool                 wireframe     = false;
    std::string          shadowQuality = "medium";  // "low" | "medium" | "high"
    int                  maxLights     = 8;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(RenderSettings,
        clearColor, wireframe, shadowQuality, maxLights)
};

// One definition shared across every translation unit / binary (C++17 inline
// variable). Self-registers with SettingsRegistry at static-init time.
inline SettingsItem<RenderSettings> g_render{"RenderSettings"};
