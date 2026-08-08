#pragma once

// ---------------------------------------------------------------------------
// tutorial_tick.hpp — payload type for tutorial/ftxui/examples/lesson_06_dc_ports.cpp.
//
// A fresh, tutorial-only payload (rather than reusing demo::CounterData) so the
// tutorial stays free of any dependency on app/counter_demo.hpp. See
// data_container.hpp: a port payload must be default-constructible and
// copy-assignable.
// ---------------------------------------------------------------------------

namespace demo
{

struct TutorialTick
{
    int    tick           = 0;
    double elapsedSeconds = 0.0;
};

} // namespace demo
