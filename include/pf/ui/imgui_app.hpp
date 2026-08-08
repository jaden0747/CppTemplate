#pragma once

// ---------------------------------------------------------------------------
// imgui_app.hpp — ImGuiApp: RAII host for a GLFW window + Dear ImGui + OpenGL.
//
// Owns the full window/UI lifecycle so application code only deals with frames:
//
//   pf::ImGuiApp app({1280, 720, "My App", fontPath, 16.0f});  // throws on failure
//   while (app.running())
//   {
//       app.beginFrame();                 // poll events, new frame, dockspace
//       MyPanel::draw();                  // ... draw ImGui panels ...
//       app.endFrame(clearColor);         // render + swap
//   }
//
// Construction sets up GLFW, the OpenGL context, the ImGui context, the docking
// flags and the UI font; destruction tears it all down. Non-copyable.
// ---------------------------------------------------------------------------

#include <array>
#include <string>

struct GLFWwindow;

namespace pf
{

class ImGuiApp
{
public:
    struct Config
    {
        int         width    = 1280;
        int         height   = 720;
        std::string title    = "App";
        std::string fontPath = "";       // empty → built-in default font
        float       fontSize = 16.0f;
    };

    explicit ImGuiApp(const Config& cfg);
    ~ImGuiApp();

    ImGuiApp(const ImGuiApp&)            = delete;
    ImGuiApp& operator=(const ImGuiApp&) = delete;

    GLFWwindow* window() const { return m_window; }
    bool        running() const;

    // Change the UI font size at runtime. ImGui 1.92 dynamic fonts re-rasterize
    // the font crisply at the new size — no atlas rebuild needed. Setting
    // _NextFrameFontSizeBase applies the change cleanly at the start of the next
    // frame even when called mid-frame (e.g. from a settings-editor callback),
    // mirroring how ImGui's own style editor changes font size.
    void setFontSize(float px);

    // Poll input, start a new ImGui frame, and lay down the fullscreen DockSpace
    // so panels drawn afterwards can be docked anywhere.
    void beginFrame();

    // Render the accumulated ImGui draw data over a cleared framebuffer and swap.
    void endFrame(const std::array<float, 4>& clearColor);

private:
    void        loadFont(const Config& cfg);
    static void drawDockSpace();

    GLFWwindow* m_window = nullptr;
};

} // namespace pf
