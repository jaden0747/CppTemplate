#pragma once

#include <cli/cli.h>

#include <memory>

/// ---------------------------------------------------------------------------
/// command_registry.hpp
///
/// Builds the CLI root menu using daniele77/cli.
///
/// Usage:
///   auto rootMenu = pf::buildRootMenu();
///   // add your domain-specific commands to rootMenu ...
///   cli::Cli cli(std::move(rootMenu));
///
/// `::cli` is daniele77/cli's namespace, spelled fully-qualified throughout so
/// it can never be shadowed by a future `pf::cli` namespace.
/// ---------------------------------------------------------------------------
namespace pf
{

std::unique_ptr<::cli::Menu> buildRootMenu();

} // namespace pf
