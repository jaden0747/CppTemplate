#include "core/imgui_log_sink.hpp"

template <typename Mutex>
ImGuiLogSink<Mutex>::ImGuiLogSink(size_t maxEntries)
    : m_maxEntries(maxEntries)
{}

template <typename Mutex>
void ImGuiLogSink<Mutex>::draw(const char* title, bool* pOpen)
{
    ImGui::Begin(title, pOpen);

    // Level filter
    ImGui::Text("Filter:");
    ImGui::SameLine();
    static int filterLevel = 0; // trace
    ImGui::Combo("##level", &filterLevel,
        "Trace\0Debug\0Info\0Warn\0Error\0Critical\0");

    ImGui::SameLine();
    if (ImGui::Button("Clear"))
    {
        std::lock_guard<Mutex> lock(this->mutex_);
        m_entries.clear();
    }

    ImGui::SameLine();
    static bool autoScroll = true;
    ImGui::Checkbox("Auto-scroll", &autoScroll);

    ImGui::Separator();

    ImGui::BeginChild("LogScrollRegion", ImVec2(0, 0), false,
        ImGuiWindowFlags_HorizontalScrollbar);

    {
        std::lock_guard<Mutex> lock(this->mutex_);
        for (const auto& entry : m_entries)
        {
            if (static_cast<int>(entry.level) < filterLevel)
                continue;

            ImVec4 color = getColor(entry.level);
            ImGui::PushStyleColor(ImGuiCol_Text, color);
            ImGui::TextUnformatted(entry.text.c_str());
            ImGui::PopStyleColor();
        }
    }

    if (autoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
        ImGui::SetScrollHereY(1.0f);

    ImGui::EndChild();
    ImGui::End();
}

template <typename Mutex>
void ImGuiLogSink<Mutex>::sink_it_(const spdlog::details::log_msg& msg)
{
    spdlog::memory_buf_t formatted;
    this->formatter_->format(msg, formatted);

    ImGuiLogEntry entry;
    entry.level = msg.level;
    entry.text  = std::string(formatted.data(), formatted.size());
    // Remove trailing newline
    if (!entry.text.empty() && entry.text.back() == '\n')
        entry.text.pop_back();

    m_entries.push_back(std::move(entry));
    while (m_entries.size() > m_maxEntries)
        m_entries.pop_front();
}

template <typename Mutex>
void ImGuiLogSink<Mutex>::flush_()
{}

template <typename Mutex>
ImVec4 ImGuiLogSink<Mutex>::getColor(spdlog::level::level_enum level)
{
    switch (level)
    {
        case spdlog::level::trace:    return ImVec4(0.5f, 0.5f, 0.5f, 1.0f); // gray
        case spdlog::level::debug:    return ImVec4(0.4f, 0.7f, 1.0f, 1.0f); // light blue
        case spdlog::level::info:     return ImVec4(0.0f, 0.9f, 0.4f, 1.0f); // green
        case spdlog::level::warn:     return ImVec4(1.0f, 0.8f, 0.0f, 1.0f); // yellow
        case spdlog::level::err:      return ImVec4(1.0f, 0.3f, 0.3f, 1.0f); // red
        case spdlog::level::critical: return ImVec4(1.0f, 0.0f, 0.5f, 1.0f); // magenta
        default:                      return ImVec4(1.0f, 1.0f, 1.0f, 1.0f); // white
    }
}

template class ImGuiLogSink<std::mutex>;
template class ImGuiLogSink<spdlog::details::null_mutex>;
