#pragma once

// ---------------------------------------------------------------------------
// imgui_log_sink.hpp — spdlog sink that buffers log entries for ImGui display
//
// Usage:
//   auto sink = std::make_shared<ImGuiLogSink_mt>();
//   Log::addSink(sink);
//   // In main loop:
//   sink->draw("Log");
// ---------------------------------------------------------------------------

#include <spdlog/sinks/base_sink.h>
#include <spdlog/details/null_mutex.h>

#include <imgui.h>

#include <deque>
#include <mutex>
#include <string>

struct ImGuiLogEntry
{
    spdlog::level::level_enum level;
    std::string               text;
};

template <typename Mutex>
class ImGuiLogSink : public spdlog::sinks::base_sink<Mutex>
{
public:
    explicit ImGuiLogSink(size_t maxEntries = 1000);

    // Call from main thread to render the log panel
    void draw(const char* title, bool* pOpen = nullptr);

protected:
    void sink_it_(const spdlog::details::log_msg& msg) override;
    void flush_() override;

private:
    static ImVec4 getColor(spdlog::level::level_enum level);

    std::deque<ImGuiLogEntry> m_entries;
    size_t                    m_maxEntries;
};

extern template class ImGuiLogSink<std::mutex>;
extern template class ImGuiLogSink<spdlog::details::null_mutex>;

using ImGuiLogSink_mt = ImGuiLogSink<std::mutex>;
using ImGuiLogSink_st = ImGuiLogSink<spdlog::details::null_mutex>;
