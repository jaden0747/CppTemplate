// ---------------------------------------------------------------------------
// lesson_07_settings_panel.cpp — a terminal analog of pf::SettingsEditor.
//
// pf::SettingsEditor (include/pf/ui/settings_editor.hpp) walks
// pf::SettingsRegistry::getItems() and, for the selected item, its JSON fields —
// rendering an ImGui widget per field. This lesson does the same walk but
// builds FTXUI components instead. Unlike ImGui (redraw-from-scratch every
// frame), FTXUI components are persistent, so changing which item is
// selected has to explicitly rebuild the field editor's Component subtree —
// see rebuild() below.
//
// One nesting pitfall this lesson works around: a Container::Vertical
// nested inside another Container::Vertical "traps" Tab — the inner
// container always wraps Tab/TabReverse within its own children and never
// lets the event bubble up to reach later siblings of the outer one. So
// `root` below is a single FLAT container holding the item menu, every
// field widget, and the three buttons as direct children; grouping in the
// rendered layout ("Items:", "Fields:", ...) is done separately via
// `fieldWidgets`, purely for display.
// ---------------------------------------------------------------------------

#include "lessons.hpp"
#include "tutorial_settings.hpp"

#include <pf/settings/settings_registry.hpp>

#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

#include <nlohmann/json.hpp>

#include <stdexcept>
#include <string>
#include <vector>

using namespace ftxui;

namespace
{

// One editable field's live UI state — mirrors the underlying JSON value
// until the user commits (Enter for text fields, immediately for Checkbox).
struct FieldMirror
{
    std::string key;
    bool        isBool  = false;
    bool        boolVal = false;
    std::string textVal; // used for int/float/string fields alike
};

} // namespace

namespace tutorial
{

void RunLesson07()
{
    auto screen = ScreenInteractive::Fullscreen();

    auto&                    registry     = pf::SettingsRegistry::instance();
    const std::string        autoSavePath = "tutorial_settings.xml";
    std::vector<std::string> itemNames;
    for (const auto& [name, entry] : registry.getItems())
        itemNames.push_back(name);
    int selectedItem = 0;

    std::vector<FieldMirror> mirrors;
    std::vector<Component>   fieldWidgets; // parallel to mirrors, in display order
    auto                     root = Container::Vertical({});

    // Persistent controls (constructed once below); rebuild() re-adds them to
    // `root` on every call since DetachAllChildren() clears everything.
    Component itemMenu;
    Component saveButton;
    Component reloadButton;
    Component resetButton;

    // Rebuild the field editor for the currently selected item: tear down
    // `root`'s children, snapshot the item's JSON fields into `mirrors`
    // (population happens fully before any Component is built, so pointers
    // taken into `mirrors` below stay valid), then build one widget per
    // supported field type and re-flatten everything back into `root`.
    std::function<void()> rebuild = [&]
    {
        root->DetachAllChildren();
        mirrors.clear();
        fieldWidgets.clear();
        root->Add(itemMenu);

        if (!itemNames.empty())
        {
            const std::string& itemName = itemNames[selectedItem];
            const auto&        entry    = registry.getItems().at(itemName);
            nlohmann::json     j        = entry.saver(entry.ptr);

            mirrors.reserve(j.size());
            for (const auto& [key, val] : j.items())
            {
                FieldMirror m;
                m.key = key;
                if (val.is_boolean())
                {
                    m.isBool  = true;
                    m.boolVal = val.get<bool>();
                }
                else if (val.is_number_integer())
                {
                    m.textVal = std::to_string(val.get<int>());
                }
                else if (val.is_number_float())
                {
                    m.textVal = std::to_string(val.get<float>());
                }
                else if (val.is_string())
                {
                    m.textVal = val.get<std::string>();
                }
                else
                {
                    continue; // arrays/objects: out of scope for this lesson
                }
                mirrors.push_back(std::move(m));
            }

            // Text-input fields (int/float/string) all render the same way:
            // "key: <input>". Only the on_enter parsing differs.
            auto labeledInput = [](const std::string& key, InputOption opt)
            {
                // Default multiline=true still calls on_enter but ALSO inserts a
                // literal '\n' into the content — force single-line so Enter just commits.
                opt.multiline   = false;
                Component input = Input(opt);
                return Renderer(input, [key, input] { return hbox({text(key + ": "), input->Render()}); });
            };

            for (size_t i = 0; i < mirrors.size(); ++i)
            {
                FieldMirror&          m        = mirrors[i];
                const nlohmann::json& original = j.at(m.key);
                Component             field;

                if (m.isBool)
                {
                    CheckboxOption opt;
                    opt.on_change = [&registry, itemName, &m]
                    { registry.setItemValue<bool>(itemName, m.key, m.boolVal); };
                    // FTXUI 5.0.0 only implements the (label, checked, option) overload of
                    // Checkbox(), not the CheckboxOption-only one declared in its header.
                    field = Checkbox(m.key, &m.boolVal, opt);
                }
                else if (original.is_number_integer())
                {
                    InputOption opt;
                    opt.content  = &m.textVal;
                    opt.on_enter = [&registry, itemName, &m]
                    {
                        try
                        {
                            int parsed = std::stoi(m.textVal);
                            registry.setItemValue<int>(itemName, m.key, parsed);
                            // Re-derive the display text from the parsed value — otherwise
                            // e.g. "12abc" stays on screen even though 12 was what committed.
                            m.textVal = std::to_string(parsed);
                        }
                        catch (const std::exception&)
                        {
                            // leave the field as typed; commit is skipped on bad input
                        }
                    };
                    field = labeledInput(m.key, opt);
                }
                else if (original.is_number_float())
                {
                    InputOption opt;
                    opt.content  = &m.textVal;
                    opt.on_enter = [&registry, itemName, &m]
                    {
                        try
                        {
                            float parsed = std::stof(m.textVal);
                            registry.setItemValue<float>(itemName, m.key, parsed);
                            m.textVal = std::to_string(parsed);
                        }
                        catch (const std::exception&)
                        {
                            // leave the field as typed; commit is skipped on bad input
                        }
                    };
                    field = labeledInput(m.key, opt);
                }
                else // string
                {
                    InputOption opt;
                    opt.content  = &m.textVal;
                    opt.on_enter = [&registry, itemName, &m]
                    { registry.setItemValue<std::string>(itemName, m.key, m.textVal); };
                    field = labeledInput(m.key, opt);
                }

                fieldWidgets.push_back(field);
                root->Add(field);
            }
        }

        root->Add(saveButton);
        root->Add(reloadButton);
        root->Add(resetButton);
    };

    MenuOption itemMenuOption;
    itemMenuOption.entries   = &itemNames;
    itemMenuOption.selected  = &selectedItem;
    itemMenuOption.on_change = rebuild;
    itemMenu                 = Menu(itemMenuOption);

    saveButton   = Button("Save", [&] { registry.saveXml(autoSavePath); });
    reloadButton = Button(
        "Reload",
        [&]
        {
            registry.loadXml(autoSavePath);
            rebuild();
        });
    resetButton = Button(
        "Reset to Defaults",
        [&]
        {
            if (!itemNames.empty())
            {
                registry.resetItem(itemNames[selectedItem]);
                rebuild();
            }
        });

    rebuild(); // safe now: itemMenu/saveButton/reloadButton/resetButton all exist

    auto component = Renderer(
        root,
        [&]
        {
            Elements fieldRows;
            for (const auto& w : fieldWidgets)
                fieldRows.push_back(w->Render());

            return vbox({
                       text("Settings-driven panel") | bold,
                       text("Registered pf::SettingsItem<T> instances, live-edited via pf::SettingsRegistry") | dim,
                       separator(),
                       text("Items:") | bold,
                       itemMenu->Render() | frame | size(HEIGHT, EQUAL, 3),
                       separator(),
                       text("Fields (Enter commits text fields):") | bold,
                       vbox(std::move(fieldRows)),
                       separator(),
                       hbox({saveButton->Render(), reloadButton->Render(), resetButton->Render()}),
                       separator(),
                       text("Persists to " + autoSavePath) | dim,
                       text("Tab/Shift+Tab to move focus, Esc to return to the menu") | dim,
                   }) |
                   border;
        });

    component |= CatchEvent(
        [&](Event event)
        {
            if (event == Event::Escape)
            {
                screen.Exit();
                return true;
            }
            return false;
        });

    screen.Loop(component);
}

} // namespace tutorial
