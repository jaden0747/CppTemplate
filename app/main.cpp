// ---------------------------------------------------------------------------
// main.cpp — Application entry point. Wires together the building blocks:
//   - pf::SettingsRegistry  (JSON-backed runtime configuration)
//   - Log + ImGui sink  (logging to console/file/UI panel)
//   - pf::ImGuiApp          (window + Dear ImGui + OpenGL host)
//   - CounterDemo       (Sender/Receiver ports demo)
// Each concern lives behind its own abstraction; main() just orchestrates them.
// ---------------------------------------------------------------------------
#include <demo/app_config.hpp>
#include <demo/render_settings.hpp>
#include <pf/ui/settings_editor.hpp>
#include <pf/log/log.hpp>
#include <pf/ui/imgui_log_sink.hpp>
#include <pf/ui/imgui_app.hpp>

#include "counter_demo.hpp"

#include <GLFW/glfw3.h>

#include <exception>
#include <memory>

// Settings globals (demo::g_app, demo::g_render) are defined as inline variables
// in their example headers and self-register at static-init time.

// ---------------------------------------------------------------------------
// Initialize logging: console + file sink (pf::Log::init) plus an ImGui panel sink.
// Returns the ImGui sink so the main loop can draw it each frame.
// ---------------------------------------------------------------------------
static std::shared_ptr<pf::ImGuiLogSink_mt> setupLogging()
{
    pf::Log::init(demo::g_app->logLevel, "app.log");
    auto sink = std::make_shared<pf::ImGuiLogSink_mt>();
    sink->set_pattern("%^[%H:%M:%S.%e] [%n] [%l] %v%$");
    pf::Log::addSink(sink);
    return sink;
}

// ---------------------------------------------------------------------------
// Bind settings changes to live window / render state. Each callback fires only
// when its specific field changes (after a load or an editor edit).
// ---------------------------------------------------------------------------
static void installSettingsBindings(pf::ImGuiApp& app)
{
    GLFWwindow* window = app.window();
    demo::g_app.onChanged(&demo::AppConfig::windowWidth,  [window](int w)                   { glfwSetWindowSize(window, w, demo::g_app->windowHeight); });
    demo::g_app.onChanged(&demo::AppConfig::windowHeight, [window](int h)                   { glfwSetWindowSize(window, demo::g_app->windowWidth, h); });
    demo::g_app.onChanged(&demo::AppConfig::appName,      [window](const std::string& name) { glfwSetWindowTitle(window, name.c_str()); });
    demo::g_app.onChanged(&demo::AppConfig::logLevel,     [](pf::LogLevel lvl)              { pf::Log::setLevel(lvl); });
    demo::g_app.onChanged(&demo::AppConfig::uiFontSize,   [&app](float px)                  { app.setFontSize(px); });

    demo::g_render.onChanged(&demo::RenderSettings::wireframe,
        [](bool wf) { glPolygonMode(GL_FRONT_AND_BACK, wf ? GL_LINE : GL_FILL); });

    // Apply initial render state.
    glPolygonMode(GL_FRONT_AND_BACK, demo::g_render->wireframe ? GL_LINE : GL_FILL);
}

int main()
{
    // Enum combo options self-register from demo::AppConfig::registerMetadata().
    pf::SettingsRegistry::instance().setAutoSave("settings.xml");
    pf::SettingsRegistry::instance().loadXml("settings.xml");

    auto imguiSink = setupLogging();
    auto appLog    = pf::Log::get("app");
    appLog->info("Application starting (logLevel={})", pf::toString(demo::g_app->logLevel));

    try
    {
        pf::ImGuiApp app({demo::g_app->windowWidth, demo::g_app->windowHeight, demo::g_app->appName,
                          demo::g_app->uiFontPath, demo::g_app->uiFontSize});

        installSettingsBindings(app);

        CounterDemo counter;
        counter.start();

        while (app.running())
        {
            counter.update();
            app.beginFrame();

            pf::SettingsEditor::draw("Settings");
            counter.drawPanel("Data Ports");
            imguiSink->draw("Log");

            app.endFrame(demo::g_render->clearColor);
            counter.cleanup();
        }

        counter.stop();
    }
    catch (const std::exception& e)
    {
        appLog->critical("Fatal: {}", e.what());
        return 1;
    }

    appLog->info("Application shutting down");
    return 0;
}
