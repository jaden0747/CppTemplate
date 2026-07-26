#pragma once

#include <cli/cli.h>

#include <memory>

/// ---------------------------------------------------------------------------
/// command_registry.hpp
///
/// Builds the CLI root menu using daniele77/cli.
///
/// Usage:
///   auto rootMenu = buildRootMenu();
///   // add your domain-specific commands to rootMenu ...
///   cli::Cli cli(std::move(rootMenu));
/// ---------------------------------------------------------------------------
std::unique_ptr<cli::Menu> buildRootMenu();
