#pragma once

#include "settings/settings_item.hpp"

#include <nlohmann/json.hpp>
#include <string>

struct EndpointInfo
{
    std::string host = "localhost";
    int         port = 8080;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(EndpointInfo, host, port)
};

struct NetworkConfig
{
    EndpointInfo endpoint  = {};
    int          timeoutMs = 5000;
    bool         enableTLS = false;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(NetworkConfig, endpoint, timeoutMs, enableTLS)
};

// One definition shared across every translation unit / binary (C++17 inline
// variable). Self-registers with SettingsRegistry at static-init time.
inline SettingsItem<NetworkConfig> g_network{"NetworkConfig"};

