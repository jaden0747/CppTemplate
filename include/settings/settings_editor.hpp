#pragma once

// ---------------------------------------------------------------------------
// SettingsEditor — sidebar + content ImGui panel for runtime settings editing.
//
// Left panel: searchable, resizable list of registered SettingsItem names.
// Right panel: editable fields for the selected item.
// New SettingsItem<T> registrations appear automatically.
//
// Usage (main loop):
//   SettingsEditor::draw("Settings");
// ---------------------------------------------------------------------------
namespace SettingsEditor
{

void draw(const char* title, bool* pOpen = nullptr);

} // namespace SettingsEditor
