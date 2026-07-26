#include "core/log.hpp"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/ansicolor_sink.h>
#include <spdlog/sinks/basic_file_sink.h>

#include <mutex>
#include <unordered_map>
#include <vector>

namespace
{

spdlog::level::level_enum toSpdlogLevel(LogLevel level)
{
    switch (level)
    {
        case LogLevel::trace:    return spdlog::level::trace;
        case LogLevel::debug:    return spdlog::level::debug;
        case LogLevel::info:     return spdlog::level::info;
        case LogLevel::warn:     return spdlog::level::warn;
        case LogLevel::err:      return spdlog::level::err;
        case LogLevel::critical: return spdlog::level::critical;
        default:                 return spdlog::level::info;
    }
}

} // namespace

struct Log::Instance
{
    std::mutex                                                        m_mu;
    bool                                                               m_initialized = false;
    spdlog::level::level_enum                                         m_level       = spdlog::level::info;
    std::shared_ptr<spdlog::sinks::ansicolor_stdout_sink_mt>          m_consoleSink;
    std::shared_ptr<spdlog::sinks::basic_file_sink_mt>                m_fileSink;
    std::vector<spdlog::sink_ptr>                                     m_extraSinks;
    std::unordered_map<std::string, std::shared_ptr<spdlog::logger>>  m_loggers;

    std::vector<spdlog::sink_ptr> getSinks()
    {
        std::vector<spdlog::sink_ptr> sinks;
        if (m_consoleSink) sinks.push_back(m_consoleSink);
        if (m_fileSink)    sinks.push_back(m_fileSink);
        for (auto& s : m_extraSinks)
            sinks.push_back(s);
        return sinks;
    }
};

Log::Instance& Log::instance()
{
    static Instance s_instance;
    return s_instance;
}

void Log::init(LogLevel level, const std::string& logFile)
{
    auto& inst = instance();
    std::lock_guard<std::mutex> lock(inst.m_mu);

    for (auto& [name, logger] : inst.m_loggers)
        spdlog::drop(name);
    inst.m_loggers.clear();
    inst.m_extraSinks.clear();

    // Use ANSI color sink (fwrite-based) so colors work through cppvsdbg pipes.
    // Windows' wincolor_stdout_sink_mt uses WriteConsoleW which silently fails on pipes.
    inst.m_consoleSink = std::make_shared<spdlog::sinks::ansicolor_stdout_sink_mt>(spdlog::color_mode::always);
    inst.m_fileSink    = std::make_shared<spdlog::sinks::basic_file_sink_mt>(logFile, true);

    const std::string pattern = "%^[%H:%M:%S.%e] [%n] [%l] %v%$";
    inst.m_consoleSink->set_pattern(pattern);
    inst.m_fileSink->set_pattern(pattern);

    inst.m_level = toSpdlogLevel(level);
    inst.m_consoleSink->set_level(inst.m_level);
    inst.m_fileSink->set_level(inst.m_level);

    inst.m_initialized = true;
}

std::shared_ptr<spdlog::logger> Log::get(const std::string& name)
{
    auto& inst = instance();
    std::lock_guard<std::mutex> lock(inst.m_mu);

    auto it = inst.m_loggers.find(name);
    if (it != inst.m_loggers.end())
        return it->second;

    auto sinks  = inst.getSinks();
    auto logger = std::make_shared<spdlog::logger>(name, sinks.begin(), sinks.end());
    logger->set_level(inst.m_level);
    inst.m_loggers[name] = logger;
    // Register with spdlog (drop first in case of re-init race)
    spdlog::drop(name);
    spdlog::register_logger(logger);
    return logger;
}

void Log::setLevel(LogLevel level)
{
    auto& inst = instance();
    std::lock_guard<std::mutex> lock(inst.m_mu);
    inst.m_level = toSpdlogLevel(level);
    if (inst.m_consoleSink) inst.m_consoleSink->set_level(inst.m_level);
    for (auto& [name, logger] : inst.m_loggers)
        logger->set_level(inst.m_level);
}

void Log::addSink(spdlog::sink_ptr sink)
{
    auto& inst = instance();
    std::lock_guard<std::mutex> lock(inst.m_mu);
    inst.m_extraSinks.push_back(sink);
    sink->set_level(inst.m_level);
    // Update existing loggers
    for (auto& [name, logger] : inst.m_loggers)
        logger->sinks().push_back(sink);
}
