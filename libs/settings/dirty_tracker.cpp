#include "settings/dirty_tracker.hpp"

void DirtyTracker::dirty()
{
    ++m_modifiedCount;
    for (auto& cb : m_callbacks)
        cb();
}
