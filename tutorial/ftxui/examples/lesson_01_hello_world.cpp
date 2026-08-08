// ---------------------------------------------------------------------------
// lesson_01_hello_world.cpp — Element vs Component, and the render loop.
// ---------------------------------------------------------------------------

#include "lessons.hpp"

#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

#include <atomic>
#include <chrono>
#include <thread>

using namespace ftxui;

namespace tutorial
{

// FTXUI has two core building blocks:
//   Element   — an immutable tree describing what to draw *this frame*
//               (text, hbox, vbox, border, ...). Cheap; you rebuild it fresh
//               every render, same as an ImGui-style immediate-mode call.
//   Component — a stateful, shared_ptr-managed node that renders an Element
//               *and* handles Event (keyboard/mouse). Menu/Button/Input below
//               in lesson 03 are all Components built on top of Elements.
//
// The one real surprise coming from Dear ImGui: ScreenInteractive::Loop()
// does not redraw every frame on a timer — it blocks until an Event arrives
// (keypress, mouse, resize, or a posted Event::Custom) and redraws once per
// event. The ticker thread below posts a Custom event on a fixed cadence so
// the loop wakes up on its own; app/tui_main.cpp's loop counter uses the same
// trick.
void RunLesson01()
{
    auto screen = ScreenInteractive::Fullscreen();

    std::atomic<bool> running{true};
    int               frame = 0;

    std::thread ticker(
        [&]
        {
            while (running.load())
            {
                screen.PostEvent(Event::Custom);
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
            }
        });

    // Renderer is the simplest Component: it wraps a function returning an
    // Element, with no children and no event handling of its own.
    auto renderer = Renderer(
        [&]
        {
            ++frame;
            return vbox({
                       text("Hello, FTXUI!") | bold,
                       separator(),
                       text("Frames rendered: " + std::to_string(frame)),
                       text(""),
                       text("Esc to return to the menu") | dim,
                   }) |
                   border;
        });

    // CatchEvent runs `on_event` before the wrapped component sees the
    // event; returning true marks it handled and stops propagation.
    auto component = CatchEvent(
        renderer,
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

    running.store(false);
    ticker.join();
}

} // namespace tutorial
