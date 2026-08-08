// ---------------------------------------------------------------------------
// lesson_04_events_and_focus.cpp — raw events, keyboard focus, mouse hover.
// ---------------------------------------------------------------------------

#include "lessons.hpp"

#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

#include <cstdio>
#include <deque>
#include <string>

using namespace ftxui;

namespace tutorial
{

namespace
{

// Named events have a dedicated Event constant; anything else (most control
// sequences) only exposes its raw input bytes via event.input().
std::string DescribeEvent(const Event& event)
{
    if (event.is_character())
        return "Character('" + event.character() + "')";
    if (event.is_mouse())
        return "Mouse";
    if (event == Event::ArrowUp)
        return "ArrowUp";
    if (event == Event::ArrowDown)
        return "ArrowDown";
    if (event == Event::ArrowLeft)
        return "ArrowLeft";
    if (event == Event::ArrowRight)
        return "ArrowRight";
    if (event == Event::Tab)
        return "Tab";
    if (event == Event::TabReverse)
        return "Shift+Tab";
    if (event == Event::Return)
        return "Return";
    if (event == Event::Escape)
        return "Escape";
    if (event == Event::Backspace)
        return "Backspace";

    std::string out = "raw(";
    for (unsigned char c : event.input())
    {
        char buf[8];
        std::snprintf(buf, sizeof(buf), "\\x%02x", c);
        out += buf;
    }
    out += ")";
    return out;
}

} // namespace

void RunLesson04()
{
    auto screen = ScreenInteractive::Fullscreen();
    screen.TrackMouse(true);

    // -- Section 1: raw event log --------------------------------------------
    // CatchEvent sees every event before any child component does. Here it
    // only *observes* (always returns false), so focus/typing below still work.
    std::deque<std::string> eventLog;

    // -- Section 2: keyboard focus --------------------------------------------
    // Three Inputs in one Container::Vertical: arrow-up/down (or Tab) move
    // focus between them automatically — that's Container's job, not
    // something each Input implements itself.
    std::string fieldA;
    std::string fieldB;
    std::string fieldC;
    auto        inputA     = Input(&fieldA, "field A");
    auto        inputB     = Input(&fieldB, "field B");
    auto        inputC     = Input(&fieldC, "field C");
    auto        focusGroup = Container::Vertical({inputA, inputB, inputC});

    // Component::Focused() reports whether that component holds keyboard
    // focus right now — used below to draw a ">" marker next to the active field.
    auto focusSection = Renderer(
        focusGroup,
        [&]
        {
            auto row = [](const char* label, const Component& input)
            {
                return hbox({
                    text(input->Focused() ? "> " : "  "),
                    text(label) | size(WIDTH, EQUAL, 10),
                    input->Render() | flex,
                });
            };
            return vbox({
                row("Field A", inputA),
                row("Field B", inputB),
                row("Field C", inputC),
            });
        });

    // -- Section 3: mouse hover ------------------------------------------------
    // Hoverable sets a bool for as long as the mouse sits over its child —
    // independent of keyboard focus.
    bool hovered   = false;
    auto hoverArea = Renderer([] { return text("Hover me") | center; });
    hoverArea      = Hoverable(hoverArea, &hovered);

    auto hoverSection = Renderer(
        hoverArea,
        [&]
        {
            return hoverArea->Render() | size(WIDTH, EQUAL, 20) | size(HEIGHT, EQUAL, 3) |
                   bgcolor(hovered ? Color::Cyan : Color::GrayDark) | color(Color::Black);
        });

    auto layout = Container::Vertical({focusSection, hoverSection});

    auto component = Renderer(
        layout,
        [&]
        {
            Elements logLines;
            for (const auto& line : eventLog)
                logLines.push_back(text(line) | dim);

            return vbox({
                       text("Events & focus") | bold,
                       separator(),
                       text("Last events (newest first):"),
                       vbox(std::move(logLines)) | size(HEIGHT, EQUAL, 5) | frame,
                       separator(),
                       text("Keyboard focus (Tab / arrows to move):") | bold,
                       focusSection->Render(),
                       separator(),
                       text("Mouse hover:") | bold,
                       hoverSection->Render(),
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
            eventLog.push_front(DescribeEvent(event));
            if (eventLog.size() > 5)
                eventLog.pop_back();
            return false; // never consume — let focus/typing behave normally
        });

    screen.Loop(component);
}

} // namespace tutorial
