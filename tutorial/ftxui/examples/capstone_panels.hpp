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
