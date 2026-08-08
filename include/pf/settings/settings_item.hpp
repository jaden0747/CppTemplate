#pragma once

#include <pf/settings/settings_registry.hpp>

#include <functional>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace pf
{

namespace detail
{
// Detects an optional `static void T::registerMetadata(SettingsRegistry&, const std::string&)`
// hook so a settings struct can self-register editor metadata (e.g. enum combo
// options) next to its definition — no manual wiring in main().
template <typename T, typename = void>
struct HasRegisterMetadata : std::false_type
{
};

template <typename T>
struct HasRegisterMetadata<T, std::void_t<decltype(T::registerMetadata(
                                  std::declval<SettingsRegistry&>(), std::declval<const std::string&>()))>>
    : std::true_type
{
};
} // namespace detail

/// ---------------------------------------------------------------------------
/// SettingsItem<T>
///
/// Wraps a settings struct T, registers it in SettingsRegistry under the
/// given string key at construction time.
///
/// Per-field change callbacks via member pointer (T must inherit DirtyTracker):
///
///   g_app.onChanged(&AppConfig::windowWidth, [](int w) { ... });
///
/// The callback fires only when that specific field changes value.
/// The snapshot is stored in the lambda closure — no extra bookkeeping.
/// ---------------------------------------------------------------------------
template <typename T>
class SettingsItem
{
public:
    explicit SettingsItem(const std::string& name)
        : m_data{}
    {
        SettingsRegistry::instance().add(name, &m_data);

        if constexpr (detail::HasRegisterMetadata<T>::value)
            T::registerMetadata(SettingsRegistry::instance(), name);

        if constexpr (std::is_base_of_v<DirtyTracker, T>)
            m_data.addOnChanged([this]() { fireFieldCallbacks(); });
    }

    ~SettingsItem() = default;

    const T* operator->() const& { return &m_data; }
    T*       operator->()      & { return &m_data; }

    const T& data() const& { return m_data; }
    T&       data()      & { return m_data; }

    const T* operator->() const&& = delete;
    T*       operator->()      && = delete;
    const T& data()       const&& = delete;
    T&       data()             && = delete;

    // Register a callback for a specific field identified by member pointer.
    // Fires only when that field's value changes. Requires T : DirtyTracker.
    template <typename Field, typename Fn>
    void onChanged(Field T::* member, Fn&& cb)
    {
        static_assert(std::is_base_of_v<DirtyTracker, T>,
            "SettingsItem::onChanged() requires T to inherit DirtyTracker");

        Field snapshot = m_data.*member;

        m_fieldCallbacks.push_back(
            [this, member, cb = std::forward<Fn>(cb), snapshot = std::move(snapshot)]() mutable {
                const Field& current = m_data.*member;
                if (!(snapshot == current))
                {
                    snapshot = current;
                    cb(current);
                }
            });
    }

private:
    void fireFieldCallbacks()
    {
        for (auto& cb : m_fieldCallbacks)
            cb();
    }

    SettingsItem(const SettingsItem&)            = delete;
    SettingsItem& operator=(const SettingsItem&) = delete;

    T                                  m_data;
    std::vector<std::function<void()>> m_fieldCallbacks;
};

} // namespace pf
