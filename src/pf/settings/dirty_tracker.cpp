#include <pf/settings/dirty_tracker.hpp>

namespace pf
{

void DirtyTracker::dirty()
{
    ++m_modifiedCount;
    for (auto& cb : m_callbacks)
        cb();
}

} // namespace pf
