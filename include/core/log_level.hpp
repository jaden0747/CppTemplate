#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include <vector>

enum class LogLevel { trace, debug, info, warn, err, critical };

NLOHMANN_JSON_SERIALIZE_ENUM(LogLevel, {
    {LogLevel::trace,    "trace"},
    {LogLevel::debug,    "debug"},
    {LogLevel::info,     "info"},
    {LogLevel::warn,     "warn"},
    {LogLevel::err,      "error"},
    {LogLevel::critical, "critical"},
})

std::string toString(LogLevel level);

// Display options for the settings editor combo box. Strings are derived from
// the NLOHMANN_JSON_SERIALIZE_ENUM map above (single source of truth), so they
// always match what gets serialized to JSON.
std::vector<std::string> logLevelOptions();
