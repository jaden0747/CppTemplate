#pragma once

// ---------------------------------------------------------------------------
// counter_data.hpp — payload type for a Sender/Receiver port pipeline.
//
// Port payload structs live here (one per header) so producers and consumers
// share a single definition — the type IS the contract between the two threads.
// Keep them plain: a port payload must be default-constructible and
// copy-assignable (Mempool resets slots with `T{}` and fan-out copies them).
// ---------------------------------------------------------------------------

namespace dc
{

struct CounterData
{
    int   value     = 0;
    float timestamp = 0.0f;
};

} // namespace dc
