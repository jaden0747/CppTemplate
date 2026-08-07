#pragma once

// ---------------------------------------------------------------------------
// SettingsTui — FTXUI full-screen terminal viewer/editor for SettingsRegistry.
//
// Left panel: list of registered SettingsItem names.
// Right panel: editable fields for the selected item (flattened, nested
// objects shown as "sub/key" paths). Apply writes edits back into the live
// settings object; Save/Reload persist to/from settings.xml.
//
// Usage:
//   SettingsTui::run();   // blocks until the user quits
// ---------------------------------------------------------------------------
namespace SettingsTui
{

void run();

} // namespace SettingsTui
