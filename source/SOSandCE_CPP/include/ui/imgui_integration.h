#pragma once
#include <SDL.h>

namespace ImGuiLayer {
    void init(SDL_Window* window, SDL_Renderer* renderer);
    void shutdown();
    void newFrame();
    void render();
    void processEvent(const SDL_Event& event);
    void applyHOI4Theme();
}
