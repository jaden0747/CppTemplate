#include "core/log_level.hpp"

std::string toString(LogLevel level)
{
    return nlohmann::json(level).get<std::string>();
}

std::vector<std::string> logLevelOptions()
{
    std::vector<std::string> opts;
    for (LogLevel l : {LogLevel::trace, LogLevel::debug, LogLevel::info,
                       LogLevel::warn, LogLevel::err, LogLevel::critical})
        opts.push_back(toString(l));
    return opts;
}
