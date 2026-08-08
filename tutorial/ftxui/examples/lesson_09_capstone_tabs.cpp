// ---------------------------------------------------------------------------
// lesson_09_capstone_tabs.cpp — combine the dc:: data, settings, and log
// panels (see capstone_panels.hpp) into one app via Container::Tab: one
// child visible at a time, picked by an index.
// ---------------------------------------------------------------------------

#include "capstone_panels.hpp"
#include "lessons.hpp"

#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

#include <string>
#include <vector>

using namespace ftxui;

namespace tutorial
{

void RunLesson09()
{
    auto screen = ScreenInteractive::Fullscreen();

    DataPanel     dataPanel(screen);
    LogPanel      logPanel(screen);
    SettingsPanel settingsPanel("tutorial_settings.xml");

    std::vector<std::string> tabNames    = {"Live Data", "Settings", "Log"};
    int                      tabSelected = 0;
    auto                     tabSwitch   = Toggle(&tabNames, &tabSelected);

    // Container::Tab renders only the child at `tabSelected`; the others
    // still exist (and, for DataPanel/LogPanel, still tick in the
    // background) — they're just not drawn.
    auto tabContent = Container::Tab(
        {
            dataPanel.component(),
            settingsPanel.component(),
            logPanel.component(),
        },
        &tabSelected);

    auto layout = Container::Vertical({tabSwitch, tabContent});

    auto component = Renderer(
        layout,
        [&]
        {
            return vbox({
                       text("Capstone - tabs") | bold,
                       text("Left/Right to switch tabs (background panels keep ticking)") | dim,
                       separator(),
                       tabSwitch->Render(),
                       separator(),
                       tabContent->Render() | flex,
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
