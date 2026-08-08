// ---------------------------------------------------------------------------
// main.cpp — lesson picker for the FTXUI tutorial (see TUTORIAL.md).
//
// Shows an interactive Menu of all lessons (a live example of the Menu
// component covered in lesson 03). Selecting one hands off to that lesson's
// own ScreenInteractive::Loop; when the lesson exits, control returns here so
// you can browse the whole tutorial in one run.
// ---------------------------------------------------------------------------

#include "examples/lessons.hpp"

#include <ftxui/component/component.hpp>
#include <ftxui/component/component_options.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

using namespace ftxui;

int main()
{
    const auto& lessons = tutorial::Lessons();

    std::vector<std::string> entries;
    for (const auto& lesson : lessons)
        entries.push_back(lesson.title);
    entries.push_back("Quit");
    const int quitIndex = static_cast<int>(entries.size()) - 1;

    int selected = 0;

    for (;;)
    {
        auto screen = ScreenInteractive::Fullscreen();

        bool enterPressed = false;

        MenuOption option;
        option.entries  = &entries;
        option.selected = &selected;
        option.on_enter = [&]
        {
            enterPressed = true;
            screen.Exit();
        };
        auto menu = Menu(option);

        auto layout = Renderer(
            menu,
            [&]
            {
                return vbox({
                           text("FTXUI Tutorial") | bold,
                           text("Arrows to move, Enter to select, Ctrl+C to quit anytime") | dim,
                           separator(),
                           menu->Render() | frame | flex,
                       }) |
                       border;
            });

        screen.Loop(layout);

        if (!enterPressed || selected == quitIndex)
            break;

        lessons[selected].run();
    }

    return 0;
}
