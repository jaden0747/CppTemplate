#pragma once

// ---------------------------------------------------------------------------
// imgui_app.hpp — ImGuiApp: RAII host for a GLFW window + Dear ImGui + OpenGL.
//
// Owns the full window/UI lifecycle so application code only deals with frames:
//
//   ImGuiApp app({1280, 720, "My App", fontPath, 16.0f});  // throws on failure
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

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <GLFW/glfw3.h>

#include <array>
#include <stdexcept>
#include <string>

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

    explicit ImGuiApp(const Config& cfg)
    {
        if (!glfwInit())
            throw std::runtime_error("glfwInit failed");

#ifdef __APPLE__
        // macOS supports OpenGL up to 4.1; Core Profile requires 3.2+ and FORWARD_COMPAT
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
        glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#else
        // Windows, Linux, WSL
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
#endif
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

        m_window = glfwCreateWindow(cfg.width, cfg.height, cfg.title.c_str(), nullptr, nullptr);
        if (!m_window)
        {
            glfwTerminate();
            throw std::runtime_error("glfwCreateWindow failed");
        }
        glfwMakeContextCurrent(m_window);
        glfwSwapInterval(1);

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        ImGui::StyleColorsDark();

        loadFont(cfg);
        setFontSize(cfg.fontSize);

        ImGui_ImplGlfw_InitForOpenGL(m_window, true);
#ifdef __APPLE__
        ImGui_ImplOpenGL3_Init("#version 150");  // GL 3.2 Core → GLSL 1.50
#else
        ImGui_ImplOpenGL3_Init("#version 130");  // GL 3.3 → GLSL 1.30 (Windows/Linux/WSL)
#endif
    }

    ~ImGuiApp()
    {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        glfwDestroyWindow(m_window);
        glfwTerminate();
    }

    ImGuiApp(const ImGuiApp&)            = delete;
    ImGuiApp& operator=(const ImGuiApp&) = delete;

    GLFWwindow* window() const { return m_window; }
    bool        running() const { return !glfwWindowShouldClose(m_window); }

    // Change the UI font size at runtime. ImGui 1.92 dynamic fonts re-rasterize
    // the font crisply at the new size — no atlas rebuild needed. Setting
    // _NextFrameFontSizeBase applies the change cleanly at the start of the next
    // frame even when called mid-frame (e.g. from a settings-editor callback),
    // mirroring how ImGui's own style editor changes font size.
    void setFontSize(float px)
    {
        ImGuiStyle& style            = ImGui::GetStyle();
        style.FontSizeBase           = px;
        style._NextFrameFontSizeBase = px;
    }

    // Poll input, start a new ImGui frame, and lay down the fullscreen DockSpace
    // so panels drawn afterwards can be docked anywhere.
    void beginFrame()
    {
        glfwPollEvents();
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        drawDockSpace();
    }

    // Render the accumulated ImGui draw data over a cleared framebuffer and swap.
    void endFrame(const std::array<float, 4>& clearColor)
    {
        ImGui::Render();
        int displayW = 0, displayH = 0;
        glfwGetFramebufferSize(m_window, &displayW, &displayH);
        glViewport(0, 0, displayW, displayH);
        glClearColor(clearColor[0], clearColor[1], clearColor[2], clearColor[3]);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(m_window);
    }

private:
    void loadFont(const Config& cfg)
    {
        ImGuiIO& io = ImGui::GetIO();
        if (!cfg.fontPath.empty() && io.Fonts->AddFontFromFileTTF(cfg.fontPath.c_str(), cfg.fontSize))
            return;

        // Font file missing or not specified — fall back to the built-in default.
        ImFontConfig fc;
        fc.SizePixels = cfg.fontSize;
        io.Fonts->AddFontDefault(&fc);
    }

    static void drawDockSpace()
    {
        ImGuiViewport* vp = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(vp->WorkPos);
        ImGui::SetNextWindowSize(vp->WorkSize);
        ImGui::SetNextWindowViewport(vp->ID);
        const ImGuiWindowFlags host_flags =
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoResize   | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoDocking;
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::Begin("##DockSpace", nullptr, host_flags);
        ImGui::PopStyleVar();
        ImGui::DockSpace(ImGui::GetID("MainDockSpace"), ImVec2(0, 0), ImGuiDockNodeFlags_PassthruCentralNode);
        ImGui::End();
    }

    GLFWwindow* m_window = nullptr;
};
