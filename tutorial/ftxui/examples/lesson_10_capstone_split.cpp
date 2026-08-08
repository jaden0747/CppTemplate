// ---------------------------------------------------------------------------
// lesson_10_capstone_split.cpp — the same three panels as lesson 09, this
// time all visible at once via nested ResizableSplit: drag a divider (mouse)
// to resize.
// ---------------------------------------------------------------------------

#include "capstone_panels.hpp"
#include "lessons.hpp"

#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

using namespace ftxui;

namespace tutorial
{

void RunLesson10()
{
    auto screen = ScreenInteractive::Fullscreen();
    screen.TrackMouse(true);

    DataPanel     dataPanel(screen);
    LogPanel      logPanel(screen);
    SettingsPanel settingsPanel("tutorial_settings.xml");

    int settingsHeight = 10; // rows given to the top-right pane (Settings)
    int dataWidth      = 26; // columns given to the left pane (Live Data)

    // ResizableSplitTop(main, back, size): `main` occupies `size` rows at
    // the top, `back` fills what's left below it.
    auto rightSplit = ResizableSplitTop(settingsPanel.component(), logPanel.component(), &settingsHeight);
    // ResizableSplitLeft(main, back, size): `main` occupies `size` columns
    // on the left, `back` (here, the split above) fills the rest.
    auto fullSplit = ResizableSplitLeft(dataPanel.component(), rightSplit, &dataWidth);

    auto component = Renderer(
        fullSplit,
        [&]
        {
            return vbox({
                       text("Capstone - resizable split") | bold,
                       text("Drag a divider with the mouse; all three panels stay visible") | dim,
                       separator(),
                       fullSplit->Render() | flex,
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
