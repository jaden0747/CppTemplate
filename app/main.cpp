// ---------------------------------------------------------------------------
// main.cpp — Application entry point. Wires together the building blocks:
//   - SettingsRegistry  (JSON-backed runtime configuration)
//   - Log + ImGui sink  (logging to console/file/UI panel)
//   - ImGuiApp          (window + Dear ImGui + OpenGL host)
//   - CounterDemo       (Sender/Receiver ports demo)
// Each concern lives behind its own abstraction; main() just orchestrates them.
// ---------------------------------------------------------------------------
#include "settings/examples/app_config.hpp"
#include "settings/examples/render_settings.hpp"
#include "settings/settings_editor.hpp"
#include "core/log.hpp"
#include "core/imgui_log_sink.hpp"
#include "core/imgui_app.hpp"

#include "counter_demo.hpp"

#include <GLFW/glfw3.h>

#include <exception>
#include <memory>

// Settings globals (g_app, g_render) are defined as inline variables in their
// example headers and self-register at static-init time.

// ---------------------------------------------------------------------------
// Initialize logging: console + file sink (Log::init) plus an ImGui panel sink.
// Returns the ImGui sink so the main loop can draw it each frame.
// ---------------------------------------------------------------------------
static std::shared_ptr<ImGuiLogSink_mt> setupLogging()
{
    Log::init(g_app->logLevel, "app.log");
    auto sink = std::make_shared<ImGuiLogSink_mt>();
    sink->set_pattern("%^[%H:%M:%S.%e] [%n] [%l] %v%$");
    Log::addSink(sink);
    return sink;
}

// ---------------------------------------------------------------------------
// Bind settings changes to live window / render state. Each callback fires only
// when its specific field changes (after a load or an editor edit).
// ---------------------------------------------------------------------------
static void installSettingsBindings(ImGuiApp& app)
{
    GLFWwindow* window = app.window();
    g_app.onChanged(&AppConfig::windowWidth,  [window](int w)                   { glfwSetWindowSize(window, w, g_app->windowHeight); });
    g_app.onChanged(&AppConfig::windowHeight, [window](int h)                   { glfwSetWindowSize(window, g_app->windowWidth, h); });
    g_app.onChanged(&AppConfig::appName,      [window](const std::string& name) { glfwSetWindowTitle(window, name.c_str()); });
    g_app.onChanged(&AppConfig::logLevel,     [](LogLevel lvl)                  { Log::setLevel(lvl); });
    g_app.onChanged(&AppConfig::uiFontSize,   [&app](float px)                  { app.setFontSize(px); });

    g_render.onChanged(&RenderSettings::wireframe,
        [](bool wf) { glPolygonMode(GL_FRONT_AND_BACK, wf ? GL_LINE : GL_FILL); });

    // Apply initial render state.
    glPolygonMode(GL_FRONT_AND_BACK, g_render->wireframe ? GL_LINE : GL_FILL);
}

int main()
{
    // Enum combo options self-register from AppConfig::registerMetadata().
    SettingsRegistry::instance().setAutoSave("settings.json");
    SettingsRegistry::instance().loadJson("settings.json");

    auto imguiSink = setupLogging();
    auto appLog    = Log::get("app");
    appLog->info("Application starting (logLevel={})", toString(g_app->logLevel));

    try
    {
        ImGuiApp app({g_app->windowWidth, g_app->windowHeight, g_app->appName,
                      g_app->uiFontPath, g_app->uiFontSize});

        installSettingsBindings(app);

        CounterDemo counter;
        counter.start();

        while (app.running())
        {
            counter.update();
            app.beginFrame();

            SettingsEditor::draw("Settings");
            counter.drawPanel("Data Ports");
            imguiSink->draw("Log");

            app.endFrame(g_render->clearColor);
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
