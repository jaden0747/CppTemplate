// ---------------------------------------------------------------------------
// tui_imtui_main.cpp — Vendored from ImTui's own ncurses example
// (examples/ncurses0/main.cpp @ tag v1.0.5), paired with imtui_demo.cpp/.h
// (examples/imtui-demo.cpp/.h, the classic Dear ImGui demo window ported to
// text), to demonstrate what the library can do. See
// https://github.com/ggerganov/imtui for the original; third_party/imtui/
// holds the library itself (backend + the patched Dear ImGui it renders).
// ---------------------------------------------------------------------------
#include "imtui/imtui.h"

#include "imtui/imtui-impl-ncurses.h"

#include "imtui_demo.h"

int main() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    auto screen = ImTui_ImplNcurses_Init(true);
    ImTui_ImplText_Init();

    bool demo = true;
    int nframes = 0;
    float fval = 1.23f;

    while (true) {
        ImTui_ImplNcurses_NewFrame();
        ImTui_ImplText_NewFrame();

        ImGui::NewFrame();

        ImGui::SetNextWindowPos(ImVec2(4, 2), ImGuiCond_Once);
        ImGui::SetNextWindowSize(ImVec2(50.0, 10.0), ImGuiCond_Once);
        ImGui::Begin("Hello, world!");
        ImGui::Text("NFrames = %d", nframes++);
        ImGui::Text("Mouse Pos : x = %g, y = %g", ImGui::GetIO().MousePos.x, ImGui::GetIO().MousePos.y);
        ImGui::Text("Time per frame %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
        ImGui::Text("Float:");
        ImGui::SameLine();
        ImGui::SliderFloat("##float", &fval, 0.0f, 10.0f);
        ImGui::End();

        ImTui::ShowDemoWindow(&demo);

        ImGui::Render();

        ImTui_ImplText_RenderDrawData(ImGui::GetDrawData(), screen);
        ImTui_ImplNcurses_DrawScreen();
    }

    ImTui_ImplText_Shutdown();
    ImTui_ImplNcurses_Shutdown();

    return 0;
}
