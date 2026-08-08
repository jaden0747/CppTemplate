#pragma once

// ---------------------------------------------------------------------------
// log.hpp — Logging module
//
// Wraps spdlog with:
//   - Auto-detected ANSI color console sink (plain in non-TTY/Debug Console)
//   - File sink (app.log, no colors)
//   - Per-module named loggers (get_logger("module_name"))
//   - Runtime log level from settings
//   - ImGui log panel sink (see imgui_log_sink.hpp)
//
// Usage:
//   #include <pf/log/log.hpp>
//   static auto logger = Log::get("mymodule");
//   logger->info("Hello {}", 42);
//   logger->warn("Something is off");
// ---------------------------------------------------------------------------

#include <pf/log/log_level.hpp>

#include <spdlog/common.h>
#include <spdlog/logger.h>

#include <memory>
#include <string>

namespace pf
{

class Log
{
public:
    // Initialize the logging system. Call once at startup after settings are loaded.
    // Safe to call multiple times (reinitializes).
    static void init(LogLevel level = LogLevel::info, const std::string& logFile = "app.log");

    // Get or create a named logger
    static std::shared_ptr<spdlog::logger> get(const std::string& name);

    static void setLevel(LogLevel level);

    // Add an extra sink (e.g. ImGui sink) — call before or after init
    static void addSink(spdlog::sink_ptr sink);

private:
    struct Instance;
    static Instance& instance();
};

} // namespace pf
