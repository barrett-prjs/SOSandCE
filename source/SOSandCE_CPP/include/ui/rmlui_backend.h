#pragma once
#include <SDL.h>
#include <RmlUi/Core.h>

namespace RmlBackend {
    bool init(SDL_Window* window, SDL_Renderer* renderer);
    void shutdown();
    void beginFrame();
    void endFrame();
    void processEvent(const SDL_Event& event);
    Rml::Context* getContext();


    Rml::ElementDocument* loadDocument(const std::string& path);
}
