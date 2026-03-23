#include "ui/imgui_integration.h"
#include "core/engine.h"
#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_sdlrenderer2.h"
#include <algorithm>
#include <cmath>

namespace ImGuiLayer {

static SDL_Renderer* s_renderer = nullptr;

void init(SDL_Window* window, SDL_Renderer* renderer) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;


    auto& eng = Engine::instance();
    std::string engineFont = eng.fontPath;

    std::vector<std::string> fontPaths;
    if (!engineFont.empty()) fontPaths.push_back(engineFont);
    fontPaths.push_back(eng.assetsPath + "SourceSans3-Regular.ttf");
    fontPaths.push_back(eng.assetsPath + "arial.TTF");
    fontPaths.push_back(eng.assetsPath + "arial.ttf");
    fontPaths.push_back("assets/SourceSans3-Regular.ttf");
    fontPaths.push_back("assets/arial.TTF");
    fontPaths.push_back("assets/arial.ttf");
    fontPaths.push_back("SourceSans3-Regular.ttf");
    fontPaths.push_back("arial.TTF");
    fontPaths.push_back("arial.ttf");


    int screenW = 1920, screenH = 1080;
    SDL_GetWindowSize(window, &screenW, &screenH);
    float fontScale = std::max(1.0f, screenH / 720.0f);
    float baseSize = 18.0f * fontScale;

    bool fontLoaded = false;
    for (auto& path : fontPaths) {
        ImFont* f = io.Fonts->AddFontFromFileTTF(path.c_str(), baseSize);
        if (f) {
            io.Fonts->AddFontFromFileTTF(path.c_str(), baseSize * 1.4f);
            io.Fonts->AddFontFromFileTTF(path.c_str(), baseSize * 1.8f);
            io.Fonts->AddFontFromFileTTF(path.c_str(), baseSize * 0.8f);
            fontLoaded = true;
            break;
        }
    }
    if (!fontLoaded) {
        io.Fonts->AddFontDefault();
        io.FontGlobalScale = fontScale;
    }

    s_renderer = renderer;
    ImGui_ImplSDL2_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer2_Init(renderer);

    applyHOI4Theme();


    ImGuiStyle& st = ImGui::GetStyle();
    st.WindowPadding = ImVec2(12 * fontScale, 12 * fontScale);
    st.FramePadding = ImVec2(8 * fontScale, 5 * fontScale);
    st.ItemSpacing = ImVec2(8 * fontScale, 6 * fontScale);
    st.ScrollbarSize = 14 * fontScale;
    st.GrabMinSize = 12 * fontScale;
}

void shutdown() {
    ImGui_ImplSDLRenderer2_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
}

void newFrame() {
    ImGui_ImplSDLRenderer2_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();
}

void render() {
    ImGui::Render();
    ImDrawData* dd = ImGui::GetDrawData();
    if (dd && s_renderer) {
        ImGui_ImplSDLRenderer2_RenderDrawData(dd, s_renderer);
    }
}

void processEvent(const SDL_Event& event) {
    ImGui_ImplSDL2_ProcessEvent(&event);
}

void applyHOI4Theme() {
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;


    style.WindowRounding = 10.0f;
    style.WindowBorderSize = 2.0f;
    style.WindowPadding = ImVec2(16, 16);
    style.WindowTitleAlign = ImVec2(0.5f, 0.5f);


    style.FrameRounding = 6.0f;
    style.FramePadding = ImVec2(10, 6);
    style.FrameBorderSize = 1.5f;


    style.GrabRounding = 4.0f;
    style.GrabMinSize = 14.0f;


    style.TabRounding = 6.0f;
    style.TabBorderSize = 1.0f;


    style.ScrollbarRounding = 8.0f;
    style.ScrollbarSize = 16.0f;


    style.PopupRounding = 8.0f;
    style.PopupBorderSize = 1.5f;


    style.ItemSpacing = ImVec2(10, 8);
    style.ItemInnerSpacing = ImVec2(8, 6);
    style.IndentSpacing = 22.0f;
    style.ChildRounding = 6.0f;
    style.ChildBorderSize = 1.0f;
    style.SeparatorTextBorderSize = 2.0f;


    ImVec4 bg          = ImVec4(0.059f, 0.067f, 0.078f, 1.0f);
    ImVec4 bgDark      = ImVec4(0.039f, 0.047f, 0.055f, 1.0f);
    ImVec4 panel       = ImVec4(0.110f, 0.118f, 0.137f, 1.0f);
    ImVec4 panelAlt    = ImVec4(0.133f, 0.145f, 0.169f, 1.0f);
    ImVec4 header      = ImVec4(0.086f, 0.094f, 0.118f, 1.0f);
    ImVec4 btn         = ImVec4(0.22f, 0.24f, 0.28f, 1.0f);
    ImVec4 btnHover    = ImVec4(0.30f, 0.32f, 0.38f, 1.0f);
    ImVec4 btnPress    = ImVec4(0.18f, 0.19f, 0.22f, 1.0f);
    ImVec4 border      = ImVec4(0.235f, 0.251f, 0.282f, 1.0f);
    ImVec4 borderHi    = ImVec4(0.345f, 0.369f, 0.408f, 1.0f);
    ImVec4 gold        = ImVec4(0.753f, 0.659f, 0.408f, 1.0f);
    ImVec4 goldBright  = ImVec4(0.863f, 0.769f, 0.510f, 1.0f);
    ImVec4 goldDim     = ImVec4(0.510f, 0.447f, 0.282f, 1.0f);
    ImVec4 cream       = ImVec4(0.824f, 0.784f, 0.675f, 1.0f);
    ImVec4 grey        = ImVec4(0.549f, 0.565f, 0.596f, 1.0f);
    ImVec4 red         = ImVec4(0.725f, 0.235f, 0.235f, 1.0f);
    ImVec4 green       = ImVec4(0.235f, 0.647f, 0.314f, 1.0f);


    colors[ImGuiCol_Text]                   = cream;
    colors[ImGuiCol_TextDisabled]           = grey;


    colors[ImGuiCol_WindowBg]               = panel;
    colors[ImGuiCol_ChildBg]                = ImVec4(bgDark.x, bgDark.y, bgDark.z, 0.5f);
    colors[ImGuiCol_PopupBg]                = ImVec4(panel.x, panel.y, panel.z, 0.95f);


    colors[ImGuiCol_Border]                 = goldDim;
    colors[ImGuiCol_BorderShadow]           = ImVec4(0, 0, 0, 0.3f);


    colors[ImGuiCol_FrameBg]                = ImVec4(btn.x, btn.y, btn.z, 0.7f);
    colors[ImGuiCol_FrameBgHovered]         = btnHover;
    colors[ImGuiCol_FrameBgActive]          = btnPress;


    colors[ImGuiCol_TitleBg]                = ImVec4(0.14f, 0.13f, 0.10f, 1.0f);
    colors[ImGuiCol_TitleBgActive]          = ImVec4(0.18f, 0.16f, 0.12f, 1.0f);
    colors[ImGuiCol_TitleBgCollapsed]       = bgDark;


    colors[ImGuiCol_MenuBarBg]              = header;


    colors[ImGuiCol_ScrollbarBg]            = bgDark;
    colors[ImGuiCol_ScrollbarGrab]          = goldDim;
    colors[ImGuiCol_ScrollbarGrabHovered]   = gold;
    colors[ImGuiCol_ScrollbarGrabActive]    = goldBright;


    colors[ImGuiCol_Button]                 = btn;
    colors[ImGuiCol_ButtonHovered]          = btnHover;
    colors[ImGuiCol_ButtonActive]           = btnPress;


    colors[ImGuiCol_Header]                 = ImVec4(goldDim.x, goldDim.y, goldDim.z, 0.3f);
    colors[ImGuiCol_HeaderHovered]          = ImVec4(goldDim.x, goldDim.y, goldDim.z, 0.5f);
    colors[ImGuiCol_HeaderActive]           = ImVec4(gold.x, gold.y, gold.z, 0.4f);


    colors[ImGuiCol_Separator]              = ImVec4(goldDim.x, goldDim.y, goldDim.z, 0.6f);
    colors[ImGuiCol_SeparatorHovered]       = gold;
    colors[ImGuiCol_SeparatorActive]        = goldBright;


    colors[ImGuiCol_ResizeGrip]             = ImVec4(goldDim.x, goldDim.y, goldDim.z, 0.3f);
    colors[ImGuiCol_ResizeGripHovered]      = ImVec4(gold.x, gold.y, gold.z, 0.5f);
    colors[ImGuiCol_ResizeGripActive]       = goldBright;


    colors[ImGuiCol_Tab]                    = btn;
    colors[ImGuiCol_TabHovered]             = btnHover;
    colors[ImGuiCol_TabSelected]            = ImVec4(goldDim.x, goldDim.y, goldDim.z, 0.4f);
    colors[ImGuiCol_TabSelectedOverline]    = gold;
    colors[ImGuiCol_TabDimmed]              = bgDark;
    colors[ImGuiCol_TabDimmedSelected]      = header;


    colors[ImGuiCol_TableHeaderBg]          = header;
    colors[ImGuiCol_TableBorderStrong]      = goldDim;
    colors[ImGuiCol_TableBorderLight]       = border;
    colors[ImGuiCol_TableRowBg]             = ImVec4(0, 0, 0, 0);
    colors[ImGuiCol_TableRowBgAlt]          = ImVec4(1, 1, 1, 0.02f);


    colors[ImGuiCol_CheckMark]              = goldBright;
    colors[ImGuiCol_SliderGrab]             = gold;
    colors[ImGuiCol_SliderGrabActive]       = goldBright;


    colors[ImGuiCol_NavHighlight]           = gold;


    colors[ImGuiCol_DragDropTarget]         = goldBright;


    colors[ImGuiCol_PlotLines]              = gold;
    colors[ImGuiCol_PlotHistogram]          = goldDim;
    colors[ImGuiCol_PlotLinesHovered]       = goldBright;
    colors[ImGuiCol_PlotHistogramHovered]   = gold;


    colors[ImGuiCol_ModalWindowDimBg]       = ImVec4(0, 0, 0, 0.6f);
}

}
