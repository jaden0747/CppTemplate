// ---------------------------------------------------------------------------
// lesson_08_log_panel.cpp — a terminal analog of pf::ImGuiLogSink_mt.
//
// include/pf/ui/imgui_log_sink.hpp buffers spdlog messages and draws them as
// ImGui text each frame. FtxuiLogSink below does the same buffering — same
// sink_it_()/formatter_ pattern, see src/pf/ui/imgui_log_sink.cpp — but hands
// out a snapshot for an FTXUI Renderer to turn into Elements instead.
// ---------------------------------------------------------------------------

#include "lessons.hpp"

#include <pf/log/log.hpp>

#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

#include <spdlog/sinks/base_sink.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

using namespace ftxui;

namespace
{

struct LogEntry
{
    spdlog::level::level_enum level;
    std::string               text;
};

class FtxuiLogSink : public spdlog::sinks::base_sink<std::mutex>
{
public:
    explicit FtxuiLogSink(size_t maxEntries)
        : m_maxEntries(maxEntries)
    {
    }

    // Thread-safe: called from the render loop, copies out under the sink's
    // own mutex_ (inherited from base_sink) rather than holding it while drawing.
    std::vector<LogEntry> snapshot()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return {m_entries.begin(), m_entries.end()};
    }

protected:
    void sink_it_(const spdlog::details::log_msg& msg) override
    {
        spdlog::memory_buf_t formatted;
        formatter_->format(msg, formatted);
        std::string text(formatted.data(), formatted.size());
        if (!text.empty() && text.back() == '\n')
            text.pop_back();

        m_entries.push_back({msg.level, std::move(text)});
        while (m_entries.size() > m_maxEntries)
            m_entries.pop_front();
    }

    void flush_() override
    {
    }

private:
    std::deque<LogEntry> m_entries;
    size_t               m_maxEntries;
};

Color LevelColor(spdlog::level::level_enum level)
{
    switch (level)
    {
    case spdlog::level::trace:
        return Color::GrayDark;
    case spdlog::level::debug:
        return Color::Cyan;
    case spdlog::level::info:
        return Color::Green;
    case spdlog::level::warn:
        return Color::Yellow;
    case spdlog::level::err:
        return Color::Red;
    case spdlog::level::critical:
        return Color::Magenta;
    default:
        return Color::White;
    }
}

constexpr int kVisibleLines = 12;
constexpr int kMaxEntries   = 500;

} // namespace

namespace tutorial
{

void RunLesson08()
{
    auto screen = ScreenInteractive::Fullscreen();

    // Wired once regardless of how many times this lesson is re-entered from
    // the picker — pf::Log::get()/addSink() are idempotent-unsafe to repeat.
    static auto sink = []
    {
        auto s   = std::make_shared<FtxuiLogSink>(kMaxEntries);
        auto log = pf::Log::get("tutorial.lesson08");
        log->set_level(spdlog::level::trace);
        pf::Log::addSink(s);
        s->set_level(spdlog::level::trace); // pf::Log::addSink() defaults new sinks to the global level (info)
        return s;
    }();
    static auto log = pf::Log::get("tutorial.lesson08");

    std::atomic<bool> running{true};
    std::thread       ticker(
        [&]
        {
            int i = 0;
            while (running.load())
            {
                switch (i % 5)
                {
                case 0:
                    log->trace("trace message #{}", i);
                    break;
                case 1:
                    log->debug("debug message #{}", i);
                    break;
                case 2:
                    log->info("info message #{}", i);
                    break;
                case 3:
                    log->warn("warn message #{}", i);
                    break;
                default:
                    log->error("error message #{}", i);
                    break;
                }
                ++i;
                screen.PostEvent(Event::Custom);
                std::this_thread::sleep_for(std::chrono::milliseconds(400));
            }
        });

    // Lines scrolled up from the bottom; 0 means pinned to the latest entry,
    // tracking new log output as it arrives (like a terminal's tail -f).
    int scrollOffset = 0;

    auto renderer = Renderer(
        [&]
        {
            auto entries = sink->snapshot();
            int  total   = static_cast<int>(entries.size());
            int  maxOffset = std::max(0, total - kVisibleLines);
            scrollOffset    = std::clamp(scrollOffset, 0, maxOffset);

            int end   = total - scrollOffset;
            int start = std::max(0, end - kVisibleLines);

            Elements lines;
            for (int i = start; i < end; ++i)
                lines.push_back(text(entries[i].text) | color(LevelColor(entries[i].level)));

            std::string status = scrollOffset == 0 ? "live — tracking newest entries"
                                                     : "scrolled up " + std::to_string(scrollOffset) +
                                                           " line(s) — press End to jump back to live";

            return vbox({
                       text("Terminal log panel") | bold,
                       text("A background thread logs at every level every 400ms; sink keeps the last " +
                            std::to_string(kMaxEntries)) |
                           dim,
                       text("↑/↓, PgUp/PgDn, Home/End, or the mouse wheel to scroll") | dim,
                       separator(),
                       vbox(std::move(lines)) | size(HEIGHT, EQUAL, kVisibleLines) | border,
                       text(status) | dim,
                       separator(),
                       text("Esc to return to the menu") | dim,
                   }) |
                   border;
        });

    auto component = CatchEvent(
        renderer,
        [&](Event event)
        {
            int total     = static_cast<int>(sink->snapshot().size());
            int maxOffset = std::max(0, total - kVisibleLines);

            bool wheelUp   = event.is_mouse() && event.mouse().button == Mouse::WheelUp;
            bool wheelDown = event.is_mouse() && event.mouse().button == Mouse::WheelDown;

            if (event == Event::ArrowUp || wheelUp)
            {
                scrollOffset = std::min(maxOffset, scrollOffset + 1);
                return true;
            }
            if (event == Event::ArrowDown || wheelDown)
            {
                scrollOffset = std::max(0, scrollOffset - 1);
                return true;
            }
            if (event == Event::PageUp)
            {
                scrollOffset = std::min(maxOffset, scrollOffset + kVisibleLines);
                return true;
            }
            if (event == Event::PageDown)
            {
                scrollOffset = std::max(0, scrollOffset - kVisibleLines);
                return true;
            }
            if (event == Event::Home)
            {
                scrollOffset = maxOffset;
                return true;
            }
            if (event == Event::End)
            {
                scrollOffset = 0;
                return true;
            }
            if (event == Event::Escape)
            {
                screen.Exit();
                return true;
            }
            return false;
        });

    screen.Loop(component);

    running.store(false);
    ticker.join();
}

} // namespace tutorial
