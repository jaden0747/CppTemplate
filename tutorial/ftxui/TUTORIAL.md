# FTXUI Tutorial

A hands-on, code-first walkthrough of [FTXUI](https://github.com/ArthurSonzogni/FTXUI) 5.0.0 — this
project's terminal-UI library — building from bare DOM elements up to a
capstone app that wires FTXUI into this project's own `pf::SettingsItem<T>`,
`pf::dc::SenderPort`/`ReceiverPort`, and `Log` systems.

Assumes you're already comfortable with immediate-mode UI concepts (this
project's `app/` leans heavily on Dear ImGui) — so this skips re-explaining
*why* declarative/immediate-mode UI works and goes straight at FTXUI's own
API surface.

## Running it

```bash
cmake --build --preset conan-debug --target ftxui_tutorial
./build/Debug/ftxui_tutorial
```

Arrow keys + Enter pick a lesson from the menu; each lesson runs its own
`ScreenInteractive` loop and returns to the picker when you press **Esc**
(the two capstones also use Esc). Every lesson is linked into one binary,
so you can browse the whole tutorial in one run.

## Layout

```
tutorial/ftxui/
  TUTORIAL.md              — this file
  main.cpp                 — lesson picker
  examples/
    lessons.hpp             — RunLessonNN() declarations + the picker's table
    lesson_01_hello_world.cpp
    lesson_02_layout_and_styling.cpp
    lesson_03_components.cpp
    lesson_04_events_and_focus.cpp
    lesson_05_custom_components.cpp
    lesson_06_dc_ports.cpp
    tutorial_settings.hpp   — shared pf::SettingsItem<T>, used by lesson 07 and the capstones
    lesson_07_settings_panel.cpp
    lesson_08_log_panel.cpp
    capstone_panels.hpp/.cpp — DataPanel/LogPanel/SettingsPanel, shared by lessons 09-10
    lesson_09_capstone_tabs.cpp
    lesson_10_capstone_split.cpp
```

Plus one new payload header outside `tutorial/`, per this project's port
convention (`examples/include/demo/dc/`, one payload per file):
`examples/include/demo/dc/tutorial_tick.hpp`, used by lesson 06 and the
capstones.

---

## The picker: `main.cpp` + `lessons.hpp`

`lessons.hpp` just declares each lesson's entry point and the table the
picker menu walks:

```cpp
#pragma once

// ---------------------------------------------------------------------------
// lessons.hpp — entry points for each tutorial lesson, plus the table
// main.cpp's picker menu walks. Each RunLessonNN() owns its own
// ScreenInteractive and event loop; it returns once that lesson's own
// exit control (a "Back to menu" button/binding) is triggered.
// ---------------------------------------------------------------------------

#include <string>
#include <vector>

namespace tutorial
{

void RunLesson01();
void RunLesson02();
void RunLesson03();
void RunLesson04();
void RunLesson05();
void RunLesson06();
void RunLesson07();
void RunLesson08();
void RunLesson09();
void RunLesson10();

struct Lesson
{
    std::string title;
    void (*run)();
};

inline const std::vector<Lesson>& Lessons()
{
    static const std::vector<Lesson> lessons = {
        {"01. Hello World & the render loop", RunLesson01},
        {"02. Layout & styling", RunLesson02},
        {"03. Built-in components", RunLesson03},
        {"04. Events & focus", RunLesson04},
        {"05. Custom components", RunLesson05},
        {"06. Cross-thread updates (pf::dc:: ports)", RunLesson06},
        {"07. Settings-driven panel (pf::SettingsRegistry)", RunLesson07},
        {"08. Terminal log panel (pf::Log::addSink)", RunLesson08},
        {"09. Capstone - tabs", RunLesson09},
        {"10. Capstone - resizable split", RunLesson10},
    };
    return lessons;
}

} // namespace tutorial
```

`main.cpp` shows an interactive `Menu` of lesson titles — itself a live
example of the `Menu` component covered in lesson 03. Selecting one hands
off to that lesson's own `ScreenInteractive::Loop()`; when it exits, control
returns here (a fresh `ScreenInteractive` is constructed each iteration —
once a `Loop()` call returns, its `ScreenInteractive` instance is not meant
to be reused for another `Loop()`).

```cpp
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
```

---

## Lesson 01 — Hello World & the render loop

FTXUI has two core building blocks:

- **`Element`** — an immutable tree describing what to draw *this frame*
  (`text`, `hbox`, `vbox`, `border`, ...). Cheap; rebuilt fresh every render,
  same as an ImGui-style immediate-mode call.
- **`Component`** — a stateful, `shared_ptr`-managed node that renders an
  `Element` *and* handles `Event` (keyboard/mouse). `Menu`/`Button`/`Input`
  (lesson 03) are all `Component`s built on top of `Element`s.

The one real surprise coming from Dear ImGui: `ScreenInteractive::Loop()`
does **not** redraw every frame on a timer — it blocks until an `Event`
arrives (keypress, mouse, resize, or a posted `Event::Custom`) and redraws
once per event. The ticker thread below posts a `Custom` event on a fixed
cadence so the loop wakes up on its own; `app/tui_main.cpp`'s loop counter
uses the same trick.

```cpp
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
```

---

## Lesson 02 — Layout & styling

`hbox`/`vbox` composition, flex sizing (`flex`, `xflex`, `size(WIDTH/HEIGHT,
EQUAL/GREATER_THAN/LESS_THAN, n)`), text decorators (`bold`, `dim`,
`underlined`, `color()`/`bgcolor()`), border variants, and `gauge()`. All
static content this time — no interactive components yet, just Esc to exit.

```cpp
// ---------------------------------------------------------------------------
// lesson_02_layout_and_styling.cpp — hbox/vbox, flex sizing, decorators.
// ---------------------------------------------------------------------------

#include "lessons.hpp"

#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

using namespace ftxui;

namespace tutorial
{

namespace
{

// A small labeled box, reused across the sizing/border sections below.
Element Box(const std::string& label, Color bg)
{
    return text(label) | center | bgcolor(bg) | color(Color::Black);
}

Element SizingSection()
{
    return vbox({
        text("Flex sizing") | bold,
        text("hbox() gives each child its natural width unless told otherwise:") | dim,
        hbox({
            Box("fixed 10", Color::Yellow) | size(WIDTH, EQUAL, 10),
            separator(),
            Box("flex (grows)", Color::Cyan) | flex,
            separator(),
            Box("fixed 14", Color::Yellow) | size(WIDTH, EQUAL, 14),
        }) | size(HEIGHT, EQUAL, 3),
        text(""),
        text("xflex only grows on the X axis; yflex only on Y:") | dim,
        hbox({
            Box("xflex", Color::Green) | xflex,
            separator(),
            Box("notflex (natural size)", Color::Red),
        }) | size(HEIGHT, EQUAL, 3),
    });
}

Element DecoratorSection()
{
    return vbox({
        text("Text decorators") | bold,
        text("bold, dim, underlined, inverted, strikethrough, color()/bgcolor()") | dim,
        hbox({
            text("bold") | bold,
            text("  "),
            text("dim") | dim,
            text("  "),
            text("underlined") | underlined,
            text("  "),
            text("inverted") | inverted,
            text("  "),
            text("strikethrough") | strikethrough,
            text("  "),
            text("color") | color(Color::Green),
        }),
    });
}

Element BorderSection()
{
    return vbox({
        text("Borders") | bold,
        text("border()/borderLight() default; borderDouble/borderRounded/borderHeavy vary the glyphs:") | dim,
        hbox({
            text("light") | border,
            text(" "),
            text("double") | borderDouble,
            text(" "),
            text("rounded") | borderRounded,
            text(" "),
            text("heavy") | borderHeavy,
        }),
    });
}

Element GaugeSection()
{
    return vbox({
        text("Gauges") | bold,
        text("A gauge() is just an Element — drive it from any float you own:") | dim,
        hbox({text("33% "), gauge(0.33f) | flex}),
        hbox({text("80% "), gauge(0.80f) | flex | color(Color::Green)}),
    });
}

} // namespace

void RunLesson02()
{
    auto screen = ScreenInteractive::Fullscreen();

    auto renderer = Renderer(
        [&]
        {
            return vbox({
                       text("Layout & styling") | bold,
                       separator(),
                       SizingSection(),
                       separatorEmpty(),
                       DecoratorSection(),
                       separatorEmpty(),
                       BorderSection(),
                       separatorEmpty(),
                       GaugeSection(),
                       filler(),
                       text("Esc to return to the menu") | dim,
                   }) |
                   border;
        });

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
}

} // namespace tutorial
```

---

## Lesson 03 — Built-in components

`Menu`, `Toggle`, `Checkbox`, `Radiobox`, `Dropdown`, `Input`, `Slider` —
composed with `Container::Vertical`, which groups children into one
focus-navigable unit (arrow keys move within a child, Tab moves between
children). Same idea as the vendored FTXUI gallery in `app/tui_main.cpp`
(the `tui` target), written fresh here with per-component commentary and a
couple of components (`Dropdown`) that gallery doesn't cover.

```cpp
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
```

---

## Lesson 04 — Events & focus

Three things:

1. **Raw event inspection** — `CatchEvent` sees every event before any child
   component does; here it only observes (always returns `false`) and logs
   a human-readable description of the last 5 events.
2. **Keyboard focus** — `Component::Focused()` reports whether a node holds
   focus right now; three `Input`s in one `Container::Vertical` demonstrate
   automatic Tab/arrow-key focus cycling.
3. **Mouse hover** — `Hoverable` tracks mouse-over independently of keyboard
   focus.

```cpp
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
```

---

## Lesson 05 — Custom components

Every built-in (`Menu`, `Button`, `Input`, ...) is just a `ComponentBase`
subclass wrapped in `Make<T>()`. This lesson writes one from scratch: a
`Stepper` that renders `< value >` and handles `ArrowLeft`/`ArrowRight` (or
`-`/`+`) while focused — overriding `Render()`, `OnEvent()`, and
`Focusable()`.

```cpp
// ---------------------------------------------------------------------------
// lesson_05_custom_components.cpp — subclassing ComponentBase.
//
// Every built-in (Menu, Button, Input, ...) is just a ComponentBase subclass
// wrapped in Make<T>(). This lesson writes one from scratch: a Stepper that
// renders "< value >" and handles ArrowLeft/ArrowRight (or -/+) while focused.
// ---------------------------------------------------------------------------

#include "lessons.hpp"

#include <ftxui/component/component.hpp>
#include <ftxui/component/component_base.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

#include <algorithm>
#include <string>

using namespace ftxui;

namespace tutorial
{

namespace
{

class StepperBase : public ComponentBase
{
public:
    StepperBase(std::string label, int* value, int min, int max, int step)
        : m_label(std::move(label))
        , m_value(value)
        , m_min(min)
        , m_max(max)
        , m_step(step)
    {
    }

    // Called every render. `Focused()` (inherited from ComponentBase) is
    // true when this node holds keyboard focus within its parent Container.
    Element Render() override
    {
        auto content = hbox({
            text(m_label) | size(WIDTH, EQUAL, 8),
            text("< "),
            text(std::to_string(*m_value)) | size(WIDTH, EQUAL, 4) | center,
            text(" >"),
        });
        return Focused() ? content | inverted : content;
    }

    // Called for every event while this node (or a descendant) is part of the
    // focus chain. Return true once handled to stop it propagating further.
    bool OnEvent(Event event) override
    {
        if (!Focused())
            return false;

        if (event == Event::ArrowLeft || event == Event::Character('-'))
        {
            *m_value = std::max(m_min, *m_value - m_step);
            return true;
        }
        if (event == Event::ArrowRight || event == Event::Character('+'))
        {
            *m_value = std::min(m_max, *m_value + m_step);
            return true;
        }
        return false;
    }

    // Without this, Container skips the node when Tab/arrow-navigating —
    // it would be visible but unreachable.
    bool Focusable() const override
    {
        return true;
    }

private:
    std::string m_label;
    int*        m_value;
    int         m_min;
    int         m_max;
    int         m_step;
};

// Factory function matching the style of the built-in Menu()/Button()/etc.
// constructors — callers never see StepperBase or Make<> directly.
Component Stepper(std::string label, int* value, int min, int max, int step = 1)
{
    return Make<StepperBase>(std::move(label), value, min, max, step);
}

} // namespace

void RunLesson05()
{
    auto screen = ScreenInteractive::Fullscreen();

    int red = 128, green = 64, blue = 200;

    // Custom components compose with built-ins and Container:: just like any
    // other Component — that's the whole point of the ComponentBase interface.
    auto steppers = Container::Vertical({
        Stepper("R", &red, 0, 255, 5),
        Stepper("G", &green, 0, 255, 5),
        Stepper("B", &blue, 0, 255, 5),
    });

    auto component = Renderer(
        steppers,
        [&]
        {
            Color swatch(static_cast<uint8_t>(red), static_cast<uint8_t>(green), static_cast<uint8_t>(blue));
            return vbox({
                       text("Custom components") | bold,
                       text("Tab to move between steppers, Left/Right (or -/+) to change") | dim,
                       separator(),
                       steppers->Render(),
                       separator(),
                       text("Swatch:"),
                       text("        ") | bgcolor(swatch) | border,
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
```

---

## Lesson 06 — Cross-thread updates via `pf::dc::` ports

Project ground rule: data crossing threads goes through a `pf::dc::SenderPort`/
`ReceiverPort` pipeline, never a shared global/queue/condition variable (see
`include/pf/dc/data_container.hpp` and `app/counter_demo.hpp`, the same
pattern feeding the ImGui app's debug panel instead of a terminal).

This lesson needs its own payload type. Per the project's port convention
(one payload struct per file, under `examples/include/demo/dc/`), that's a
new header — a **fresh, tutorial-only payload** rather than reusing
`demo::CounterData`, so the tutorial has no dependency on `app/counter_demo.hpp`:

```cpp
#pragma once

// ---------------------------------------------------------------------------
// tutorial_tick.hpp — payload type for tutorial/ftxui/examples/lesson_06_dc_ports.cpp.
//
// A fresh, tutorial-only payload (rather than reusing demo::CounterData) so the
// tutorial stays free of any dependency on app/counter_demo.hpp. See
// data_container.hpp: a port payload must be default-constructible and
// copy-assignable.
// ---------------------------------------------------------------------------

namespace dc
{

struct TutorialTick
{
    int    tick           = 0;
    double elapsedSeconds = 0.0;
};

} // namespace dc
```

A background thread produces `demo::TutorialTick` values and delivers them
through the port pipeline; the render loop's `Renderer` lambda calls
`update()`/`getData()`/`cleanup()` — the same main-loop pattern documented
in `data_container.hpp` — since that lambda *is* this lesson's one frame hook:

```cpp
// ---------------------------------------------------------------------------
// lesson_06_dc_ports.cpp — feeding a live FTXUI view from a background
// thread via pf::dc::SenderPort/ReceiverPort.
//
// Project ground rule: data crossing threads goes through a pf::dc:: port
// pipeline, never a shared global/queue/condition variable. See
// include/pf/dc/data_container.hpp and app/counter_demo.hpp (the same
// pattern feeding the ImGui app's debug panel instead of a terminal).
// ---------------------------------------------------------------------------

#include "lessons.hpp"

#include <pf/dc/data_container.hpp>
#include <demo/dc/tutorial_tick.hpp>
#include <pf/log/log.hpp>

#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

#include <atomic>
#include <chrono>
#include <thread>

using namespace ftxui;

namespace tutorial
{

void RunLesson06()
{
    static auto log = pf::Log::get("tutorial.lesson06");

    auto screen = ScreenInteractive::Fullscreen();

    // Pool + sender are the producer side; both would normally be file-scope
    // globals so any producer can reach them. Kept local here since the
    // producer thread lives entirely inside this function.
    pf::dc::Mempool<demo::TutorialTick>    pool(4);
    pf::dc::SenderPort<demo::TutorialTick> sender;
    sender.connectMempool(pool);

    // Receiver lives at the consumer site — here, the render loop.
    pf::dc::ReceiverPort<demo::TutorialTick> receiver;
    receiver.connect(sender);

    std::atomic<bool> running{true};
    std::thread       producer(
        [&]
        {
            using clock      = std::chrono::steady_clock;
            const auto start = clock::now();
            int        tick  = 0;
            while (running.load())
            {
                // reserve() → fill → deliver(); reserve() returns nullptr when
                // the pool is exhausted (every slot still held by a receiver) —
                // drop the send rather than block the producer.
                if (demo::TutorialTick* slot = sender.reserve())
                {
                    slot->tick           = tick++;
                    slot->elapsedSeconds = std::chrono::duration<double>(clock::now() - start).count();
                    sender.deliver();
                }
                screen.PostEvent(Event::Custom); // wake the render loop
                std::this_thread::sleep_for(std::chrono::milliseconds(150));
            }
        });
    log->info("pf::dc:: producer thread started");

    auto renderer = Renderer(
        [&]
        {
            // Main-loop pattern from data_container.hpp: update() promotes the
            // latest delivered slot, cleanup() clears the "new" flag. Both
            // happen here since this lambda IS the render loop's one frame hook.
            receiver.update();
            Element body;
            if (const demo::TutorialTick* d = receiver.getData())
            {
                body = vbox({
                    text("tick    : " + std::to_string(d->tick)),
                    text("elapsed : " + std::to_string(d->elapsedSeconds) + "s"),
                });
            }
            else
            {
                body = text("waiting for data...") | dim;
            }
            receiver.cleanup();

            return vbox({
                       text("Cross-thread updates via pf::dc:: ports") | bold,
                       text("A background thread posts a demo::TutorialTick every 150ms") | dim,
                       separator(),
                       body,
                       separator(),
                       text("Esc to return to the menu") | dim,
                   }) |
                   border;
        });

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
    producer.join();
    log->info("pf::dc:: producer thread stopped");
}

} // namespace tutorial
```

---

## Lesson 07 — Settings-driven panel

A terminal analog of `pf::SettingsEditor` (`include/pf/ui/settings_editor.hpp`):
walk `pf::SettingsRegistry::getItems()` and, for the selected item, its JSON
fields — rendering a widget per field. `pf::SettingsEditor` does this by
redrawing ImGui widgets from scratch every frame; FTXUI components are
*persistent*, so changing which item is selected means explicitly
rebuilding the field editor's `Component` subtree (`rebuild()` below).

This lesson needs a `pf::SettingsItem<T>` to point at. It's pulled into its own
tiny header (rather than declared inline in the lesson file) so both this
lesson and the capstones (09/10) register the *same* `inline` global,
instead of two separately-registered structs racing for the
`"TutorialUiSettings"` key — `pf::SettingsRegistry::add()` ignores (and warns
on) duplicate keys:

```cpp
#pragma once

// ---------------------------------------------------------------------------
// tutorial_settings.hpp — the pf::SettingsItem<T> shared by lesson 07 and the
// capstones (09/10). Pulled into its own header (rather than declared inline
// in lesson_07_settings_panel.cpp) so both TUs register the exact same
// `inline` global instead of two separately-registered structs racing for
// the "TutorialUiSettings" key — see settings/settings_registry.hpp's
// pf::SettingsRegistry::add(), which ignores (and warns on) duplicate keys.
// ---------------------------------------------------------------------------

#include <pf/settings/settings_item.hpp>

#include <nlohmann/json.hpp>
#include <string>

namespace tutorial
{

struct TutorialUiSettings
{
    bool        showBorder = true;
    int         refreshMs  = 500;
    float       volume     = 0.75f;
    std::string greeting   = "hello, tutorial";

    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(TutorialUiSettings, showBorder, refreshMs, volume, greeting)
};

inline pf::SettingsItem<TutorialUiSettings> g_tutorialUi{"TutorialUiSettings"};

} // namespace tutorial
```

Two real bugs worth knowing about, both fixed in the code below:

- **`InputOption` defaults to `multiline = true`.** `Input`'s `Event::Return`
  handler calls `on_enter()` either way, but when multiline it *also* inserts
  a literal `'\n'` into the content first. For a single-line "commit on
  Enter" field, set `opt.multiline = false` explicitly.
- **Nesting a `Container::Vertical` inside another `Container::Vertical`
  traps Tab.** `VerticalContainer::EventHandler` always calls
  `MoveSelectorWrap()` on `Event::Tab`, which — as long as the container has
  more than one focusable child — *always* finds one to wrap to and returns
  `true`. Since a parent container's `OnEvent` only calls its own
  `EventHandler` when the active child's `OnEvent` returns `false`, Tab can
  never bubble out of a nested container with 2+ focusable children. An
  earlier version of this lesson put the settings fields in their own
  `fieldContainer` nested inside an outer container alongside the item menu
  and the Save/Reload/Reset buttons — Tab would cycle through the fields
  forever and never reach the buttons. The fix: keep everything in **one
  flat `Container::Vertical`** (`root` below), and do the "Items:" /
  "Fields:" / buttons visual grouping separately at render time via a plain
  `std::vector<Component>` (`fieldWidgets`), decoupled from the focus tree.

```cpp
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
```

---

## Lesson 08 — Terminal log panel

A terminal analog of `pf::ImGuiLogSink_mt` (`include/pf/ui/imgui_log_sink.hpp`):
buffer spdlog messages in a custom sink, draw them each render.
`FtxuiLogSink` reuses the exact same `sink_it_()`/`formatter_` pattern as
`src/pf/ui/imgui_log_sink.cpp`, just building `ftxui::Element`s instead of
issuing ImGui draw calls.

One thing to watch: `pf::Log::get()` before `pf::Log::init()` returns a logger with
zero sinks and the registry's default level (`info`) — so `trace`/`debug`
calls go nowhere until you explicitly `log->set_level(spdlog::level::trace)`
and `sink->set_level(spdlog::level::trace)` (`pf::Log::addSink()` defaults a new
sink to the *current* global level, which is `info` if `pf::Log::init()` was
never called).

```cpp
// ---------------------------------------------------------------------------
// lesson_08_log_panel.cpp — a terminal analog of pf::ImGuiLogSink_mt.
//
// include/pf/ui/imgui_log_sink.hpp buffers spdlog messages and draws them as
// ImGui text each frame. FtxuiLogSink below does the same buffering — same
// sink_it_()/formatter_ pattern, see src/pf/ui/imgui_log_sink.cpp — but hands
// out a snapshot for an FTXUI Renderer to turn into Elements instead.
// ---------------------------------------------------------------------------

#include "lessons.hpp"

#include <pf/log/log.hpp>

#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

#include <spdlog/sinks/base_sink.h>

#include <atomic>
#include <chrono>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

using namespace ftxui;

namespace
{

struct LogEntry
{
    spdlog::level::level_enum level;
    std::string               text;
};

class FtxuiLogSink : public spdlog::sinks::base_sink<std::mutex>
{
public:
    explicit FtxuiLogSink(size_t maxEntries)
        : m_maxEntries(maxEntries)
    {
    }

    // Thread-safe: called from the render loop, copies out under the sink's
    // own mutex_ (inherited from base_sink) rather than holding it while drawing.
    std::vector<LogEntry> snapshot()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return {m_entries.begin(), m_entries.end()};
    }

protected:
    void sink_it_(const spdlog::details::log_msg& msg) override
    {
        spdlog::memory_buf_t formatted;
        formatter_->format(msg, formatted);
        std::string text(formatted.data(), formatted.size());
        if (!text.empty() && text.back() == '\n')
            text.pop_back();

        m_entries.push_back({msg.level, std::move(text)});
        while (m_entries.size() > m_maxEntries)
            m_entries.pop_front();
    }

    void flush_() override
    {
    }

private:
    std::deque<LogEntry> m_entries;
    size_t               m_maxEntries;
};

Color LevelColor(spdlog::level::level_enum level)
{
    switch (level)
    {
    case spdlog::level::trace:
        return Color::GrayDark;
    case spdlog::level::debug:
        return Color::Cyan;
    case spdlog::level::info:
        return Color::Green;
    case spdlog::level::warn:
        return Color::Yellow;
    case spdlog::level::err:
        return Color::Red;
    case spdlog::level::critical:
        return Color::Magenta;
    default:
        return Color::White;
    }
}

} // namespace

namespace tutorial
{

void RunLesson08()
{
    auto screen = ScreenInteractive::Fullscreen();

    // Wired once regardless of how many times this lesson is re-entered from
    // the picker — pf::Log::get()/addSink() are idempotent-unsafe to repeat.
    static auto sink = []
    {
        auto s   = std::make_shared<FtxuiLogSink>(12);
        auto log = pf::Log::get("tutorial.lesson08");
        log->set_level(spdlog::level::trace);
        pf::Log::addSink(s);
        s->set_level(spdlog::level::trace); // pf::Log::addSink() defaults new sinks to the global level (info)
        return s;
    }();
    static auto log = pf::Log::get("tutorial.lesson08");

    std::atomic<bool> running{true};
    std::thread       ticker(
        [&]
        {
            int i = 0;
            while (running.load())
            {
                switch (i % 5)
                {
                case 0:
                    log->trace("trace message #{}", i);
                    break;
                case 1:
                    log->debug("debug message #{}", i);
                    break;
                case 2:
                    log->info("info message #{}", i);
                    break;
                case 3:
                    log->warn("warn message #{}", i);
                    break;
                default:
                    log->error("error message #{}", i);
                    break;
                }
                ++i;
                screen.PostEvent(Event::Custom);
                std::this_thread::sleep_for(std::chrono::milliseconds(400));
            }
        });

    auto renderer = Renderer(
        [&]
        {
            Elements lines;
            for (const auto& entry : sink->snapshot())
                lines.push_back(text(entry.text) | color(LevelColor(entry.level)));

            return vbox({
                       text("Terminal log panel") | bold,
                       text("A background thread logs at every level every 400ms; sink keeps the last 12") | dim,
                       separator(),
                       vbox(std::move(lines)) | size(HEIGHT, EQUAL, 12) | border,
                       separator(),
                       text("Esc to return to the menu") | dim,
                   }) |
                   border;
        });

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
```

---

## Shared capstone panels

Lessons 09 and 10 both combine the exact same three techniques (pf::dc:: ports,
pf::SettingsRegistry, pf::Log::addSink) — just arranged differently (tabs vs. a
resizable split). Rather than duplicate ~300 lines twice, `capstone_panels.hpp`/
`.cpp` wraps each as an RAII class exposing a ready-to-compose
`ftxui::Component`: `DataPanel`, `LogPanel`, `SettingsPanel`. Each owns any
background thread it starts and joins it on destruction, so a caller just
needs to keep the object alive for the duration of its
`ScreenInteractive::Loop()`.

`SettingsPanel` is the same flat-`Container::Vertical` design from lesson 07
(see its notes on the Tab-trapping pitfall above), refactored into a class:
member functions can safely capture `this` by reference in lambdas, since
the object's lifetime is guaranteed by the caller — no `shared_ptr`
lifetime bookkeeping needed for what would otherwise be a factory function
returning a bare `Component` after its own stack frame ends.

```cpp
#pragma once

// ---------------------------------------------------------------------------
// capstone_panels.hpp — the three lesson 06/07/08 techniques (pf::dc:: ports,
// pf::SettingsRegistry, pf::Log::addSink), each wrapped as an RAII class exposing a
// ready-to-compose ftxui::Component. Shared by lesson_09 (tabs) and
// lesson_10 (split) so neither has to re-derive this logic — see those two
// lessons for what's actually new: Container::Tab and ResizableSplit.
//
// Each class owns any background thread it starts and joins it on
// destruction, so a caller just needs to keep the object alive for the
// duration of its ScreenInteractive::Loop().
// ---------------------------------------------------------------------------

#include <pf/dc/data_container.hpp>
#include <demo/dc/tutorial_tick.hpp>
#include <pf/log/log.hpp>

#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>

#include <spdlog/sinks/base_sink.h>

#include <atomic>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace tutorial
{

// -- Live pf::dc:: data panel (lesson 06) ----------------------------------------
class DataPanel
{
public:
    explicit DataPanel(ftxui::ScreenInteractive& screen);
    ~DataPanel();

    DataPanel(const DataPanel&)            = delete;
    DataPanel& operator=(const DataPanel&) = delete;

    ftxui::Component component() const
    {
        return m_component;
    }

private:
    pf::dc::Mempool<demo::TutorialTick>      m_pool{4};
    pf::dc::SenderPort<demo::TutorialTick>   m_sender;
    pf::dc::ReceiverPort<demo::TutorialTick> m_receiver;
    std::atomic<bool>                  m_running{true};
    std::thread                        m_thread;
    ftxui::Component                   m_component;
};

// -- Terminal log panel (lesson 08) ------------------------------------------
struct LogEntry
{
    spdlog::level::level_enum level;
    std::string               text;
};

class FtxuiLogSink : public spdlog::sinks::base_sink<std::mutex>
{
public:
    explicit FtxuiLogSink(size_t maxEntries);

    std::vector<LogEntry> snapshot();

protected:
    void sink_it_(const spdlog::details::log_msg& msg) override;
    void flush_() override
    {
    }

private:
    std::deque<LogEntry> m_entries;
    size_t               m_maxEntries;
};

class LogPanel
{
public:
    explicit LogPanel(ftxui::ScreenInteractive& screen);
    ~LogPanel();

    LogPanel(const LogPanel&)            = delete;
    LogPanel& operator=(const LogPanel&) = delete;

    ftxui::Component component() const
    {
        return m_component;
    }

private:
    std::shared_ptr<FtxuiLogSink>   m_sink;
    std::shared_ptr<spdlog::logger> m_log;
    std::atomic<bool>               m_running{true};
    std::thread                     m_thread;
    ftxui::Component                m_component;
};

// -- Settings-driven panel (lesson 07) ---------------------------------------
class SettingsPanel
{
public:
    explicit SettingsPanel(std::string autoSavePath);

    ftxui::Component component() const
    {
        return m_component;
    }

private:
    struct FieldMirror
    {
        std::string key;
        bool        isBool  = false;
        bool        boolVal = false;
        std::string textVal;
    };

    void rebuild();

    std::string                   m_autoSavePath;
    std::vector<std::string>      m_itemNames;
    int                           m_selectedItem = 0;
    std::vector<FieldMirror>      m_mirrors;
    std::vector<ftxui::Component> m_fieldWidgets; // parallel to m_mirrors, in display order

    // A Container::Vertical nested inside another Container::Vertical traps
    // Tab navigation inside the inner one (see lesson 07's comment for
    // details), so m_root is a single FLAT container holding the item menu,
    // every field widget, and the three buttons as direct children.
    ftxui::Component m_root;
    ftxui::Component m_itemMenu;
    ftxui::Component m_saveButton;
    ftxui::Component m_reloadButton;
    ftxui::Component m_resetButton;
    ftxui::Component m_component;
};

} // namespace tutorial
```
```cpp
#include "capstone_panels.hpp"
#include "tutorial_settings.hpp"

#include <pf/settings/settings_registry.hpp>

#include <ftxui/component/component_options.hpp>
#include <ftxui/dom/elements.hpp>

#include <chrono>
#include <stdexcept>

using namespace ftxui;

namespace tutorial
{

// -----------------------------------------------------------------------
// DataPanel
// -----------------------------------------------------------------------

DataPanel::DataPanel(ScreenInteractive& screen)
{
    m_sender.connectMempool(m_pool);
    m_receiver.connect(m_sender);

    m_thread = std::thread(
        [this, &screen]
        {
            using clock      = std::chrono::steady_clock;
            const auto start = clock::now();
            int        tick  = 0;
            while (m_running.load())
            {
                if (demo::TutorialTick* slot = m_sender.reserve())
                {
                    slot->tick           = tick++;
                    slot->elapsedSeconds = std::chrono::duration<double>(clock::now() - start).count();
                    m_sender.deliver();
                }
                screen.PostEvent(Event::Custom);
                std::this_thread::sleep_for(std::chrono::milliseconds(150));
            }
        });

    m_component = Renderer(
        [this]
        {
            m_receiver.update();
            Element body;
            if (const demo::TutorialTick* d = m_receiver.getData())
            {
                body = vbox({
                    text("tick    : " + std::to_string(d->tick)),
                    text("elapsed : " + std::to_string(d->elapsedSeconds) + "s"),
                });
            }
            else
            {
                body = text("waiting for data...") | dim;
            }
            m_receiver.cleanup();
            return body;
        });
}

DataPanel::~DataPanel()
{
    m_running.store(false);
    if (m_thread.joinable())
        m_thread.join();
}

// -----------------------------------------------------------------------
// FtxuiLogSink
// -----------------------------------------------------------------------

FtxuiLogSink::FtxuiLogSink(size_t maxEntries)
    : m_maxEntries(maxEntries)
{
}

std::vector<LogEntry> FtxuiLogSink::snapshot()
{
    std::lock_guard<std::mutex> lock(mutex_);
    return {m_entries.begin(), m_entries.end()};
}

void FtxuiLogSink::sink_it_(const spdlog::details::log_msg& msg)
{
    spdlog::memory_buf_t formatted;
    formatter_->format(msg, formatted);
    std::string text(formatted.data(), formatted.size());
    if (!text.empty() && text.back() == '\n')
        text.pop_back();

    m_entries.push_back({msg.level, std::move(text)});
    while (m_entries.size() > m_maxEntries)
        m_entries.pop_front();
}

namespace
{

Color LevelColor(spdlog::level::level_enum level)
{
    switch (level)
    {
    case spdlog::level::trace:
        return Color::GrayDark;
    case spdlog::level::debug:
        return Color::Cyan;
    case spdlog::level::info:
        return Color::Green;
    case spdlog::level::warn:
        return Color::Yellow;
    case spdlog::level::err:
        return Color::Red;
    case spdlog::level::critical:
        return Color::Magenta;
    default:
        return Color::White;
    }
}

} // namespace

// -----------------------------------------------------------------------
// LogPanel
// -----------------------------------------------------------------------

LogPanel::LogPanel(ScreenInteractive& screen)
{
    m_sink = std::make_shared<FtxuiLogSink>(10);
    m_log  = pf::Log::get("tutorial.capstone");
    m_log->set_level(spdlog::level::trace);
    pf::Log::addSink(m_sink);
    m_sink->set_level(spdlog::level::trace);

    m_thread = std::thread(
        [this, &screen]
        {
            int i = 0;
            while (m_running.load())
            {
                switch (i % 5)
                {
                case 0:
                    m_log->trace("trace #{}", i);
                    break;
                case 1:
                    m_log->debug("debug #{}", i);
                    break;
                case 2:
                    m_log->info("info #{}", i);
                    break;
                case 3:
                    m_log->warn("warn #{}", i);
                    break;
                default:
                    m_log->error("error #{}", i);
                    break;
                }
                ++i;
                screen.PostEvent(Event::Custom);
                std::this_thread::sleep_for(std::chrono::milliseconds(400));
            }
        });

    auto sink   = m_sink; // copy of the shared_ptr, kept alive by the render lambda itself
    m_component = Renderer(
        [sink]
        {
            Elements lines;
            for (const auto& entry : sink->snapshot())
                lines.push_back(text(entry.text) | color(LevelColor(entry.level)));
            return vbox(std::move(lines));
        });
}

LogPanel::~LogPanel()
{
    m_running.store(false);
    if (m_thread.joinable())
        m_thread.join();
}

// -----------------------------------------------------------------------
// SettingsPanel
// -----------------------------------------------------------------------

SettingsPanel::SettingsPanel(std::string autoSavePath)
    : m_autoSavePath(std::move(autoSavePath))
    , m_root(Container::Vertical({}))
{
    auto& registry = pf::SettingsRegistry::instance();
    for (const auto& [name, entry] : registry.getItems())
        m_itemNames.push_back(name);

    MenuOption itemMenuOption;
    itemMenuOption.entries   = &m_itemNames;
    itemMenuOption.selected  = &m_selectedItem;
    itemMenuOption.on_change = [this] { rebuild(); };
    m_itemMenu               = Menu(itemMenuOption);

    m_saveButton   = Button("Save", [this] { pf::SettingsRegistry::instance().saveXml(m_autoSavePath); });
    m_reloadButton = Button(
        "Reload",
        [this]
        {
            pf::SettingsRegistry::instance().loadXml(m_autoSavePath);
            rebuild();
        });
    m_resetButton = Button(
        "Reset to Defaults",
        [this]
        {
            if (!m_itemNames.empty())
            {
                pf::SettingsRegistry::instance().resetItem(m_itemNames[m_selectedItem]);
                rebuild();
            }
        });

    rebuild(); // safe now: m_itemMenu/m_saveButton/m_reloadButton/m_resetButton all exist

    m_component = Renderer(
        m_root,
        [this]
        {
            Elements fieldRows;
            for (const auto& w : m_fieldWidgets)
                fieldRows.push_back(w->Render());

            return vbox({
                text("Items:"),
                m_itemMenu->Render() | frame | size(HEIGHT, EQUAL, 3),
                separator(),
                vbox(std::move(fieldRows)),
                separator(),
                hbox({m_saveButton->Render(), m_reloadButton->Render(), m_resetButton->Render()}),
            });
        });
}

void SettingsPanel::rebuild()
{
    auto& registry = pf::SettingsRegistry::instance();
    m_root->DetachAllChildren();
    m_mirrors.clear();
    m_fieldWidgets.clear();
    m_root->Add(m_itemMenu);

    if (!m_itemNames.empty())
    {
        const std::string& itemName = m_itemNames[m_selectedItem];
        const auto&        entry    = registry.getItems().at(itemName);
        nlohmann::json     j        = entry.saver(entry.ptr);

        m_mirrors.reserve(j.size());
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
                continue; // arrays/objects: out of scope for this panel
            }
            m_mirrors.push_back(std::move(m));
        }

        auto labeledInput = [](const std::string& key, InputOption opt)
        {
            // Default multiline=true still calls on_enter but ALSO inserts a
            // literal '\n' into the content — force single-line so Enter just commits.
            opt.multiline   = false;
            Component input = Input(opt);
            return Renderer(input, [key, input] { return hbox({text(key + ": "), input->Render()}); });
        };

        for (size_t i = 0; i < m_mirrors.size(); ++i)
        {
            FieldMirror&          m        = m_mirrors[i];
            const nlohmann::json& original = j.at(m.key);
            Component             field;

            if (m.isBool)
            {
                CheckboxOption opt;
                opt.on_change = [itemName, &m]
                { pf::SettingsRegistry::instance().setItemValue<bool>(itemName, m.key, m.boolVal); };
                // FTXUI 5.0.0 only implements the (label, checked, option) overload of
                // Checkbox(), not the CheckboxOption-only one declared in its header.
                field = Checkbox(m.key, &m.boolVal, opt);
            }
            else if (original.is_number_integer())
            {
                InputOption opt;
                opt.content  = &m.textVal;
                opt.on_enter = [itemName, &m]
                {
                    try
                    {
                        int parsed = std::stoi(m.textVal);
                        pf::SettingsRegistry::instance().setItemValue<int>(itemName, m.key, parsed);
                        // Re-derive the display text from the parsed value — otherwise
                        // e.g. "12abc" stays on screen even though 12 was what committed.
                        m.textVal = std::to_string(parsed);
                    }
                    catch (const std::exception&)
                    {
                    }
                };
                field = labeledInput(m.key, opt);
            }
            else if (original.is_number_float())
            {
                InputOption opt;
                opt.content  = &m.textVal;
                opt.on_enter = [itemName, &m]
                {
                    try
                    {
                        float parsed = std::stof(m.textVal);
                        pf::SettingsRegistry::instance().setItemValue<float>(itemName, m.key, parsed);
                        m.textVal = std::to_string(parsed);
                    }
                    catch (const std::exception&)
                    {
                    }
                };
                field = labeledInput(m.key, opt);
            }
            else // string
            {
                InputOption opt;
                opt.content  = &m.textVal;
                opt.on_enter = [itemName, &m]
                { pf::SettingsRegistry::instance().setItemValue<std::string>(itemName, m.key, m.textVal); };
                field = labeledInput(m.key, opt);
            }

            m_fieldWidgets.push_back(field);
            m_root->Add(field);
        }
    }

    m_root->Add(m_saveButton);
    m_root->Add(m_reloadButton);
    m_root->Add(m_resetButton);
}

} // namespace tutorial
```

---

## Lesson 09 — Capstone: tabs

`Container::Tab(children, selector)` renders only the child at `*selector`;
the others still exist — `DataPanel`/`LogPanel`'s background threads keep
ticking even on a tab that isn't currently visible — they're just not drawn.
Tab switching here uses `Toggle` (Left/Right), a component from lesson 03,
not the Tab *key*.

> **Note:** once you Tab into a panel's own fields (e.g. the Settings tab),
> keyboard Tab stays trapped inside that panel's `SettingsPanel::m_root` (see
> lesson 07) — the same "nested container eats Tab" behavior, just one level
> up. Left/Right still switches tabs at any time; there's just no way to Tab
> *out* of a panel's internal fields back to the tab switcher itself once
> you've tabbed in. Untangling that fully would mean flattening the entire
> capstone (tab switcher + all three panels' controls) into one container,
> which throws away the whole point of `DataPanel`/`LogPanel`/`SettingsPanel`
> being independently reusable — not worth it for a capstone demo.

```cpp
// ---------------------------------------------------------------------------
// lesson_09_capstone_tabs.cpp — combine the pf::dc:: data, settings, and log
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
```

---

## Lesson 10 — Capstone: resizable split

The same three panels, all visible at once via nested `ResizableSplit`:
`ResizableSplitTop(main, back, size)` gives `main` the top `size` rows and
`back` the rest; `ResizableSplitLeft(main, back, size)` gives `main` the
left `size` columns and `back` the rest. Nesting the two produces a
three-pane layout with two independently draggable dividers (mouse).

```cpp
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
```

---

## Appendix: things that bit us building this

A short list of non-obvious FTXUI 5.0.0 behavior discovered while writing
these lessons — useful if you extend the tutorial or build your own panels:

1. **`Checkbox(CheckboxOption)` is declared but not implemented.** FTXUI
   5.0.0's `component.hpp` declares both `Checkbox(CheckboxOption)` and
   `Checkbox(ConstStringRef label, bool* checked, CheckboxOption option)`,
   but `checkbox.cpp` only *defines* the three-argument overload — the
   `CheckboxOption`-only one link-fails with "undefined reference". Always
   call `Checkbox(label, &value, opt)`.
2. **`InputOption::multiline` defaults to `true`**, and `Event::Return`
   inserts `'\n'` into the content in addition to calling `on_enter()`. Set
   `opt.multiline = false` for single-line "commit on Enter" fields (see
   lesson 07).
3. **A `Container::Vertical`/`Horizontal` nested inside another one traps
   Tab.** `VerticalContainer`/`HorizontalContainer::EventHandler` always
   wraps `Event::Tab`/`TabReverse` within its own children when it has 2+
   focusable ones, so the event never bubbles up to a parent container's
   later siblings. Keep one focus group flat; do visual grouping separately
   at render time (see lesson 07 and `SettingsPanel`).
4. **`Container::Tab` doesn't intercept Tab itself** — it just forwards
   whatever event it receives to the active child — so it composes fine
   with the panels above; the trapping above happens *inside* whichever
   panel currently has focus, not because of `Container::Tab`.
5. **`pf::Log::get()` before `pf::Log::init()`** returns a logger with no sinks and
   the registry's default (`info`) level — `trace`/`debug` calls are
   silently dropped until you raise the logger's *and* the sink's level
   explicitly (see lesson 08).
