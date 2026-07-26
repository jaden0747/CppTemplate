#include "core/imgui_app.hpp"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <GLFW/glfw3.h>

#include <stdexcept>

ImGuiApp::ImGuiApp(const Config& cfg)
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

ImGuiApp::~ImGuiApp()
{
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(m_window);
    glfwTerminate();
}

bool ImGuiApp::running() const
{
    return !glfwWindowShouldClose(m_window);
}

void ImGuiApp::setFontSize(float px)
{
    ImGuiStyle& style            = ImGui::GetStyle();
    style.FontSizeBase           = px;
    style._NextFrameFontSizeBase = px;
}

void ImGuiApp::beginFrame()
{
    glfwPollEvents();
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    drawDockSpace();
}

void ImGuiApp::endFrame(const std::array<float, 4>& clearColor)
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

void ImGuiApp::loadFont(const Config& cfg)
{
    ImGuiIO& io = ImGui::GetIO();
    if (!cfg.fontPath.empty() && io.Fonts->AddFontFromFileTTF(cfg.fontPath.c_str(), cfg.fontSize))
        return;

    // Font file missing or not specified — fall back to the built-in default.
    ImFontConfig fc;
    fc.SizePixels = cfg.fontSize;
    io.Fonts->AddFontDefault(&fc);
}

void ImGuiApp::drawDockSpace()
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
