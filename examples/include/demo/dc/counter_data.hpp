#pragma once

// ---------------------------------------------------------------------------
// counter_data.hpp — payload type for a Sender/Receiver port pipeline.
//
// Port payload structs are owned by the *application*, not the platform: the
// type IS the contract between two of your threads, so it lives in your
// namespace (here `demo`) while pf::dc supplies the ports. One per header, so
// producer and consumer share a single definition.
//
// Keep them plain: a port payload must be default-constructible and
// copy-assignable (pf::dc::Mempool resets slots with `T{}` and fan-out copies
// them).
// ---------------------------------------------------------------------------

namespace demo
{

struct CounterData
{
    int   value     = 0;
    float timestamp = 0.0f;
};

} // namespace demo
