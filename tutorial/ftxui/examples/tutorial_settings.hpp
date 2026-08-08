#pragma once

// ---------------------------------------------------------------------------
// tutorial_settings.hpp — the pf::SettingsItem<T> shared by lesson 07 and the
// capstones (09/10). Pulled into its own header (rather than declared inline
// in lesson_07_settings_panel.cpp) so both TUs register the exact same
// `inline` global instead of two separately-registered structs racing for
// the "TutorialUiSettings" key — see settings/settings_registry.hpp's
// pf::SettingsRegistry::add(), which ignores (and warns on) duplicate keys.
// ---------------------------------------------------------------------------

#include <pf/settings/settings_item.hpp>

#include <nlohmann/json.hpp>
#include <string>

namespace tutorial
{

struct TutorialUiSettings
{
    bool        showBorder = true;
    int         refreshMs  = 500;
    float       volume     = 0.75f;
    std::string greeting   = "hello, tutorial";

    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(TutorialUiSettings, showBorder, refreshMs, volume, greeting)
};

inline pf::SettingsItem<TutorialUiSettings> g_tutorialUi{"TutorialUiSettings"};

} // namespace tutorial
