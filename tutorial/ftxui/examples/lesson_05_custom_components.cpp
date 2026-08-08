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
