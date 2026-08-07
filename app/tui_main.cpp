// ---------------------------------------------------------------------------
// tui_main.cpp — Terminal entry point. Loads settings, initializes logging
// to file only (no console spam under the FTXUI alt-screen), and runs the
// FTXUI settings viewer/editor until the user quits.
// ---------------------------------------------------------------------------
#include "settings/examples/app_config.hpp"
#include "settings/examples/render_settings.hpp"
#include "settings/settings_tui.hpp"
#include "core/log.hpp"

#include <exception>

int main()
{
    SettingsRegistry::instance().setAutoSave("settings.xml");
    SettingsRegistry::instance().loadXml("settings.xml");

    Log::init(g_app->logLevel, "tui.log");
    auto appLog = Log::get("tui");
    appLog->info("TUI starting (logLevel={})", toString(g_app->logLevel));

    try
    {
        SettingsTui::run();
    }
    catch (const std::exception& e)
    {
        appLog->critical("Fatal: {}", e.what());
        return 1;
    }

    appLog->info("TUI shutting down");
    return 0;
}
