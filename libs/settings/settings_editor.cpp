#include "settings/settings_editor.hpp"

#include "settings/settings_registry.hpp"

#include <imgui.h>
#include <imgui_stdlib.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <cstring>
#include <string>

namespace SettingsEditor
{

namespace
{

bool drawCombo(const std::string& key, nlohmann::json& val,
               const std::vector<std::string>& options)
{
    const std::string current = val.get<std::string>();
    int               idx     = 0;
    for (int i = 0; i < static_cast<int>(options.size()); ++i)
        if (options[i] == current) { idx = i; break; }

    std::string itemStr;
    for (auto& opt : options) { itemStr += opt; itemStr += '\0'; }
    itemStr += '\0';

    if (ImGui::Combo(key.c_str(), &idx, itemStr.c_str()))
    {
        val = options[idx];
        return true;
    }
    return false;
}

// Append a small "Reset" button that reverts `val` to `defVal`. Shown only when
// a default exists and the current value differs from it.
bool drawResetButton(const nlohmann::json* defVal, nlohmann::json& val)
{
    if (!defVal || *defVal == val)
        return false;
    ImGui::SameLine();
    if (ImGui::SmallButton("Reset"))
    {
        val = *defVal;
        return true;
    }
    return false;
}

bool drawField(const std::string& key, nlohmann::json& val,
               const nlohmann::json* defVal = nullptr,
               const std::vector<std::string>* enumOptions = nullptr)
{
    bool changed = false;
    ImGui::PushID(key.c_str());

    // Nested object: recurse, pairing each member with its default sub-value.
    if (val.is_object())
    {
        if (ImGui::TreeNodeEx(key.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
        {
            for (auto& [subKey, subVal] : val.items())
            {
                const nlohmann::json* subDef =
                    (defVal && defVal->is_object() && defVal->contains(subKey)) ? &defVal->at(subKey) : nullptr;
                if (drawField(subKey, subVal, subDef)) changed = true;
            }
            ImGui::TreePop();
        }
        ImGui::PopID();
        return changed;
    }

    // Leaf widgets.
    if (enumOptions && val.is_string())
    {
        changed = drawCombo(key, val, *enumOptions);
    }
    else if (val.is_boolean())
    {
        bool b = val.get<bool>();
        if (ImGui::Checkbox(key.c_str(), &b)) { val = b; changed = true; }
    }
    else if (val.is_number_integer())
    {
        int i = val.get<int>();
        if (ImGui::DragInt(key.c_str(), &i)) { val = i; changed = true; }
    }
    else if (val.is_number_float())
    {
        float f = val.get<float>();
        if (ImGui::DragFloat(key.c_str(), &f, 0.01f)) { val = f; changed = true; }
    }
    else if (val.is_string())
    {
        std::string s = val.get<std::string>();
        if (ImGui::InputText(key.c_str(), &s, ImGuiInputTextFlags_EnterReturnsTrue))
        { val = s; changed = true; }
    }
    else if (val.is_array() && val.size() == 4 && val[0].is_number() &&
             key.find("olor") != std::string::npos)
    {
        float arr[4] = {val[0].get<float>(), val[1].get<float>(), val[2].get<float>(), val[3].get<float>()};
        if (ImGui::ColorEdit4(key.c_str(), arr))
        { val = nlohmann::json::array({arr[0], arr[1], arr[2], arr[3]}); changed = true; }
    }
    else
    {
        ImGui::TextDisabled("%s: %s", key.c_str(), val.dump().c_str());
    }

    if (drawResetButton(defVal, val)) changed = true;

    ImGui::PopID();
    return changed;
}

bool icontains(const std::string& haystack, const char* needle)
{
    return std::search(
               haystack.begin(), haystack.end(), needle, needle + std::strlen(needle),
               [](char a, char b) {
                   return std::tolower((unsigned char)a) == std::tolower((unsigned char)b);
               }) != haystack.end();
}

} // namespace

void draw(const char* title, bool* pOpen)
{
    ImGui::Begin(title, pOpen);

    auto& registry = SettingsRegistry::instance();
    auto& items    = registry.getItems();

    static std::string selectedKey;
    static float       sidebarWidth  = 160.0f;
    static char        searchBuf[128] = {};

    if (selectedKey.empty() && !items.empty())
        selectedKey = items.begin()->first;

    const float bottomBarHeight = ImGui::GetFrameHeightWithSpacing() + ImGui::GetStyle().ItemSpacing.y;
    const float contentHeight   = ImGui::GetContentRegionAvail().y - bottomBarHeight;

    // --- Left sidebar ---
    ImGui::BeginChild("##sidebar", ImVec2(sidebarWidth, contentHeight), true);

    ImGui::SetNextItemWidth(-1.0f);
    ImGui::InputTextWithHint("##search", "Search...", searchBuf, sizeof(searchBuf));
    ImGui::Separator();

    for (auto& [itemName, _] : items)
    {
        if (searchBuf[0] != '\0' && !icontains(itemName, searchBuf))
            continue;
        if (ImGui::Selectable(itemName.c_str(), itemName == selectedKey))
            selectedKey = itemName;
    }

    ImGui::EndChild();

    // --- Splitter ---
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::GetStyleColorVec4(ImGuiCol_SeparatorHovered));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImGui::GetStyleColorVec4(ImGuiCol_SeparatorActive));
    ImGui::Button("##splitter", ImVec2(4.0f, contentHeight));
    ImGui::PopStyleColor(3);

    if (ImGui::IsItemHovered() || ImGui::IsItemActive())
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
    if (ImGui::IsItemActive())
        sidebarWidth = std::clamp(sidebarWidth + ImGui::GetIO().MouseDelta.x, 80.0f, 400.0f);

    ImGui::SameLine();

    // --- Right content ---
    ImGui::BeginChild("##content", ImVec2(0.0f, contentHeight), false);

    auto it = items.find(selectedKey);
    if (it != items.end())
    {
        auto& [itemName, entry] = *it;
        nlohmann::json j        = entry.saver(entry.ptr);
        nlohmann::json defaults = registry.getDefaultJson(itemName);
        bool           changed  = false;

        for (auto& [key, val] : j.items())
        {
            const auto*           opts = registry.getEnumOptions(itemName, key);
            const nlohmann::json* def  = defaults.contains(key) ? &defaults.at(key) : nullptr;
            if (drawField(key, val, def, opts)) changed = true;
        }

        if (changed)
        {
            entry.loader(entry.ptr, j);
            if (entry.onLoaded) entry.onLoaded(entry.ptr);
            if (!registry.getAutoSavePath().empty())
                registry.saveXml(registry.getAutoSavePath());
        }
    }

    ImGui::EndChild();

    // --- Bottom bar ---
    ImGui::Separator();
    if (ImGui::Button("Save"))   registry.saveXml("settings.xml");
    ImGui::SameLine();
    if (ImGui::Button("Reload")) registry.loadXml("settings.xml");
    ImGui::SameLine();
    if (ImGui::Button("Reset to Defaults"))
    {
        if (registry.resetItem(selectedKey) && !registry.getAutoSavePath().empty())
            registry.saveXml(registry.getAutoSavePath());
    }

    ImGui::End();
}

} // namespace SettingsEditor
