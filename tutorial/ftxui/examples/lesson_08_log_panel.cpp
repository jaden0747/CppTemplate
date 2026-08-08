// ---------------------------------------------------------------------------
// lesson_08_log_panel.cpp — a terminal analog of ImGuiLogSink_mt.
//
// include/core/imgui_log_sink.hpp buffers spdlog messages and draws them as
// ImGui text each frame. FtxuiLogSink below does the same buffering — same
// sink_it_()/formatter_ pattern, see libs/core/imgui_log_sink.cpp — but hands
// out a snapshot for an FTXUI Renderer to turn into Elements instead.
// ---------------------------------------------------------------------------

#include "lessons.hpp"

#include "core/log.hpp"

#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

#include <spdlog/sinks/base_sink.h>

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

} // namespace

namespace tutorial
{

void RunLesson08()
{
    auto screen = ScreenInteractive::Fullscreen();

    // Wired once regardless of how many times this lesson is re-entered from
    // the picker — Log::get()/addSink() are idempotent-unsafe to repeat.
    static auto sink = []
    {
        auto s   = std::make_shared<FtxuiLogSink>(12);
        auto log = Log::get("tutorial.lesson08");
        log->set_level(spdlog::level::trace);
        Log::addSink(s);
        s->set_level(spdlog::level::trace); // Log::addSink() defaults new sinks to the global level (info)
        return s;
    }();
    static auto log = Log::get("tutorial.lesson08");

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

    auto renderer = Renderer(
        [&]
        {
            Elements lines;
            for (const auto& entry : sink->snapshot())
                lines.push_back(text(entry.text) | color(LevelColor(entry.level)));

            return vbox({
                       text("Terminal log panel") | bold,
                       text("A background thread logs at every level every 400ms; sink keeps the last 12") | dim,
                       separator(),
                       vbox(std::move(lines)) | size(HEIGHT, EQUAL, 12) | border,
                       separator(),
                       text("Esc to return to the menu") | dim,
                   }) |
                   border;
        });

    auto component = CatchEvent(
        renderer,
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

    running.store(false);
    ticker.join();
}

} // namespace tutorial
