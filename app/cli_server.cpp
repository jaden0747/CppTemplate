// ---------------------------------------------------------------------------
// cli_server.cpp — Telnet CLI for runtime settings inspection/modification
// ---------------------------------------------------------------------------
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include "settings/settings_item.hpp"
#include "settings/command_registry.hpp"
#include "settings/examples/app_config.hpp"
#include "settings/examples/render_settings.hpp"
#include "settings/examples/network_config.hpp"

#include <cli/cli.h>
#include <cli/standaloneasioscheduler.h>
#include <cli/clilocalsession.h>
#include <cli/standaloneasioremotecli.h>

#include <iostream>
#include <string>

// Settings globals (g_app, g_render, g_network) are defined as inline variables
// in their example headers and self-register at static-init time.

int main(int argc, char* argv[])
{
    uint16_t port = 5000;
    for (int i = 1; i < argc - 1; ++i)
    {
        if (std::string(argv[i]) == "--port")
            port = static_cast<uint16_t>(std::stoi(argv[i + 1]));
    }

    SettingsRegistry::instance().loadXml("settings.xml");
    std::cout << "Settings loaded from settings.xml\n";

    auto rootMenu = buildRootMenu();
    rootMenu->Insert(
        "save",
        [](std::ostream& out, const std::string& filename)
        {
            SettingsRegistry::instance().saveXml(filename);
            out << "Saved to \"" << filename << "\"\n";
        },
        "Save all settings to an XML file");

    cli::Cli cli(std::move(rootMenu));
    cli.ExitAction([](std::ostream& out) { out << "Goodbye!\n"; });

    cli::StandaloneAsioScheduler scheduler;

    cli::CliLocalTerminalSession localSession(cli, scheduler, std::cout);
    localSession.ExitAction([&scheduler](std::ostream& out)
    {
        out << "Shutting down...\n";
        scheduler.Stop();
    });

    cli::StandaloneAsioCliTelnetServer server(cli, scheduler, port);

    std::cout << "CLI server listening on port " << port << "\n";
    std::cout << "Connect:  telnet localhost " << port << "\n\n";

    scheduler.Run();
    return 0;
}
