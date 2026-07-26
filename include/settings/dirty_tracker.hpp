#pragma once

#include <cstdint>
#include <functional>
#include <vector>

class DirtyTracker
{
public:
    void dirty();

    uint32_t getModifiedCount() const { return m_modifiedCount; }

    void addOnChanged(std::function<void()> cb) { m_callbacks.push_back(std::move(cb)); }

    // Non-virtual by design: settings structs derive from DirtyTracker but are
    // always held by value (in SettingsItem<T>) and never deleted through a
    // DirtyTracker* — so no polymorphic destruction occurs.
    ~DirtyTracker() = default;

private:
    uint32_t                           m_modifiedCount{0U};
    std::vector<std::function<void()>> m_callbacks;
};
