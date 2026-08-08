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
