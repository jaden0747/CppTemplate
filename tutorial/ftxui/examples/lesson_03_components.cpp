// ---------------------------------------------------------------------------
// lesson_03_components.cpp — the built-in interactive components.
// ---------------------------------------------------------------------------

#include "lessons.hpp"

#include <ftxui/component/component.hpp>
#include <ftxui/component/component_options.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

#include <string>
#include <vector>

using namespace ftxui;

namespace tutorial
{

namespace
{

// Label a component with its name to the left, like app/tui_main.cpp's Wrap().
Component Labeled(const std::string& name, Component component)
{
    return Renderer(
        component,
        [name, component]
        {
            return hbox({
                       text(name) | size(WIDTH, EQUAL, 10),
                       separator(),
                       component->Render() | xflex,
                   }) |
                   xflex;
        });
}

} // namespace

void RunLesson03()
{
    auto screen = ScreenInteractive::Fullscreen();

    // -- Menu: a list, arrow keys move `selected`.
    const std::vector<std::string> menuEntries  = {"Alpha", "Beta", "Gamma"};
    int                            menuSelected = 0;
    auto                           menu         = Labeled("Menu", Menu(&menuEntries, &menuSelected));

    // -- Toggle: a two-state Menu variant, rendered horizontally.
    std::vector<std::string> toggleEntries  = {"Off", "On"};
    int                      toggleSelected = 0;
    auto                     toggle         = Labeled("Toggle", Toggle(&toggleEntries, &toggleSelected));

    // -- Checkbox: independent bools, grouped in a Container so Tab/arrows
    // cycle focus between them.
    bool checkboxA  = false;
    bool checkboxB  = true;
    auto checkboxes = Labeled(
        "Checkbox",
        Container::Vertical({
            Checkbox("Option A", &checkboxA),
            Checkbox("Option B", &checkboxB),
        }));

    // -- Radiobox: one-of-many, like Menu but rendered as a list of radio
    // buttons instead of a highlighted row.
    std::vector<std::string> radioEntries  = {"Small", "Medium", "Large"};
    int                      radioSelected = 1;
    auto                     radiobox      = Labeled("Radiobox", Radiobox(&radioEntries, &radioSelected));

    // -- Dropdown: Menu + Checkbox combined — collapsed until opened.
    std::vector<std::string> dropdownEntries  = {"Red", "Green", "Blue"};
    int                      dropdownSelected = 0;
    auto                     dropdown         = Labeled("Dropdown", Dropdown(&dropdownEntries, &dropdownSelected));

    // -- Input: editable text, bound directly to a std::string.
    std::string inputValue;
    auto        input = Labeled("Input", Input(&inputValue, "type here..."));

    // -- Slider: drag/arrow-key a numeric value within [min, max].
    int  sliderValue = 40;
    auto slider      = Labeled("Slider", Slider("", &sliderValue, 0, 100, 1));

    // Container::Vertical composes children into one focus-navigable group;
    // arrow keys move within a child (e.g. Menu), Tab moves between children.
    auto layout = Container::Vertical({
        menu,
        toggle,
        checkboxes,
        radiobox,
        dropdown,
        input,
        slider,
    });

    auto component = Renderer(
        layout,
        [&]
        {
            return vbox({
                       text("Built-in components") | bold,
                       separator(),
                       layout->Render(),
                       separator(),
                       text("Esc to return to the menu") | dim,
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
