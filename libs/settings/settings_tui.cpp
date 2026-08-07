#include "settings/settings_tui.hpp"

#include "settings/settings_registry.hpp"

#include <ftxui/component/component.hpp>
#include <ftxui/component/component_base.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

#include <map>
#include <memory>
#include <string>
#include <vector>

using namespace ftxui;

namespace SettingsTui
{

namespace
{

enum class LeafType
{
    Bool,
    Int,
    Float,
    String,
    Json
};

// Right-hand pane: rebuilds its Input/Checkbox children whenever the selected
// item name changes, mirroring SettingsEditor::draw's field walk but backed
// by FTXUI components instead of ImGui immediate-mode widgets.
class FieldsPane : public ComponentBase
{
public:
    explicit FieldsPane(const std::string* selectedItem) : m_selectedItem(selectedItem) {}

    Element Render() override
    {
        rebuildIfNeeded();
        if (ChildCount() == 0)
            return text("(no fields)") | dim;

        // ComponentBase::Render() only composes 0/1 children correctly; lay
        // out our (possibly many) field rows ourselves.
        Elements rows;
        for (int i = 0; i < ChildCount(); ++i)
            rows.push_back(ChildAt(i)->Render());
        return vbox(std::move(rows)) | vscroll_indicator | frame;
    }

    bool OnEvent(Event event) override
    {
        rebuildIfNeeded();
        return ComponentBase::OnEvent(event);
    }

    // Called after external mutations (Reload/Reset) so stale buffer text
    // doesn't linger; next Render()/OnEvent() repopulates from the registry.
    void invalidate() { m_hasBuilt = false; }

    void applyToRegistry()
    {
        auto& registry = SettingsRegistry::instance();
        auto  it       = registry.getItems().find(m_builtItem);
        if (it == registry.getItems().end())
            return;

        nlohmann::json j = it->second.saver(it->second.ptr);
        for (auto& path : m_paths)
        {
            nlohmann::json::json_pointer ptr("/" + path);
            switch (m_types[path])
            {
            case LeafType::Bool:
                j[ptr] = m_boolBuffers[path];
                break;
            case LeafType::Int:
                try { j[ptr] = std::stoll(m_textBuffers[path]); } catch (...) {}
                break;
            case LeafType::Float:
                try { j[ptr] = std::stod(m_textBuffers[path]); } catch (...) {}
                break;
            case LeafType::String:
                j[ptr] = m_textBuffers[path];
                break;
            case LeafType::Json:
                try { j[ptr] = nlohmann::json::parse(m_textBuffers[path]); } catch (...) {}
                break;
            }
        }

        it->second.loader(it->second.ptr, j);
        if (it->second.onLoaded)
            it->second.onLoaded(it->second.ptr);
        if (!registry.getAutoSavePath().empty())
            registry.saveXml(registry.getAutoSavePath());
    }

private:
    void flatten(const std::string& path, const nlohmann::json& val)
    {
        if (val.is_object())
        {
            for (auto& [subKey, subVal] : val.items())
                flatten(path.empty() ? subKey : path + "/" + subKey, subVal);
            return;
        }

        m_paths.push_back(path);
        if (val.is_boolean())
        {
            m_types[path]       = LeafType::Bool;
            m_boolBuffers[path] = val.get<bool>();
        }
        else if (val.is_number_integer())
        {
            m_types[path]       = LeafType::Int;
            m_textBuffers[path] = std::to_string(val.get<long long>());
        }
        else if (val.is_number_float())
        {
            m_types[path]       = LeafType::Float;
            m_textBuffers[path] = std::to_string(val.get<double>());
        }
        else if (val.is_string())
        {
            m_types[path]       = LeafType::String;
            m_textBuffers[path] = val.get<std::string>();
        }
        else
        {
            m_types[path]       = LeafType::Json;
            m_textBuffers[path] = val.dump();
        }
    }

    void rebuildIfNeeded()
    {
        if (m_hasBuilt && *m_selectedItem == m_builtItem)
            return;
        m_hasBuilt  = true;
        m_builtItem = *m_selectedItem;

        DetachAllChildren();
        m_paths.clear();
        m_types.clear();
        m_textBuffers.clear();
        m_boolBuffers.clear();

        auto& registry = SettingsRegistry::instance();
        auto  it       = registry.getItems().find(m_builtItem);
        if (it == registry.getItems().end())
            return;

        nlohmann::json j = it->second.saver(it->second.ptr);
        for (auto& [key, val] : j.items())
            flatten(key, val);

        for (auto& path : m_paths)
        {
            Component field;
            if (m_types[path] == LeafType::Bool)
            {
                field = Checkbox(path, &m_boolBuffers[path]);
            }
            else
            {
                auto input = Input(&m_textBuffers[path], path);
                field      = Renderer(input, [path, input] {
                    return hbox({text(path) | size(WIDTH, EQUAL, 24), text(": "), input->Render() | flex});
                });
            }
            Add(field);
        }
    }

    const std::string*                    m_selectedItem;
    bool                                   m_hasBuilt  = false;
    std::string                           m_builtItem;
    std::vector<std::string>              m_paths;
    std::map<std::string, LeafType>       m_types;
    std::map<std::string, std::string>    m_textBuffers;
    std::map<std::string, bool>           m_boolBuffers;
};

} // namespace

void run()
{
    auto& registry = SettingsRegistry::instance();

    std::vector<std::string> itemNames;
    for (auto& [name, entry] : registry.getItems())
        itemNames.push_back(name);

    int selectedIndex = 0;
    std::string selectedItem = itemNames.empty() ? "" : itemNames[0];

    auto screen = ScreenInteractive::Fullscreen();

    MenuOption menuOption;
    menuOption.on_change = [&] {
        if (selectedIndex >= 0 && selectedIndex < static_cast<int>(itemNames.size()))
            selectedItem = itemNames[selectedIndex];
    };
    auto menu = Menu(&itemNames, &selectedIndex, menuOption);

    auto fieldsPane = Make<FieldsPane>(&selectedItem);

    auto pane = std::static_pointer_cast<FieldsPane>(fieldsPane);

    auto applyButton  = Button("Apply", [&, pane] { pane->applyToRegistry(); });
    auto saveButton   = Button("Save", [&] { registry.saveXml("settings.xml"); });
    auto reloadButton = Button("Reload", [&, pane] { registry.loadXml("settings.xml"); pane->invalidate(); });
    auto resetButton  = Button("Reset to Defaults", [&, pane] {
        if (registry.resetItem(selectedItem) && !registry.getAutoSavePath().empty())
            registry.saveXml(registry.getAutoSavePath());
        pane->invalidate();
    });
    auto quitButton = Button("Quit", screen.ExitLoopClosure());

    auto buttonsRow = Container::Horizontal({applyButton, saveButton, reloadButton, resetButton, quitButton});

    auto layout = Container::Vertical({
        Container::Horizontal({menu, fieldsPane}),
        buttonsRow,
    });

    auto renderer = Renderer(layout, [&] {
        return vbox({
                   hbox({
                       menu->Render() | vscroll_indicator | frame | size(WIDTH, EQUAL, 28) | border,
                       fieldsPane->Render() | flex | border,
                   }) | flex,
                   buttonsRow->Render() | border,
               }) |
               border;
    });

    registry.setAutoSave("settings.xml");
    screen.Loop(renderer);
}

} // namespace SettingsTui
