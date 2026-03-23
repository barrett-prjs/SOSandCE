#include "ui/rmlui_backend.h"
#include "core/engine.h"
#include <RmlUi/Core.h>
#include <SDL.h>
#include <SDL_image.h>
#include <unordered_map>
#include <cstring>
#include <vector>


static SDL_Renderer* s_renderer = nullptr;
static SDL_Window*   s_window   = nullptr;
static Rml::Context* s_context  = nullptr;


class SDLRenderInterface : public Rml::RenderInterface {
public:
    SDLRenderInterface(SDL_Renderer* r) : renderer(r) {}

    Rml::CompiledGeometryHandle CompileGeometry(Rml::Span<const Rml::Vertex> vertices,
                                                 Rml::Span<const int> indices) override {
        auto* geo = new CompiledGeo;
        geo->vertices.assign(vertices.begin(), vertices.end());
        geo->indices.assign(indices.begin(), indices.end());
        return reinterpret_cast<Rml::CompiledGeometryHandle>(geo);
    }

    void RenderGeometry(Rml::CompiledGeometryHandle geometry, Rml::Vector2f translation,
                        Rml::TextureHandle texture) override {
        auto* geo = reinterpret_cast<CompiledGeo*>(geometry);
        SDL_Texture* tex = reinterpret_cast<SDL_Texture*>(texture);

        for (size_t i = 0; i + 2 < geo->indices.size(); i += 3) {
            const auto& v0 = geo->vertices[geo->indices[i]];
            const auto& v1 = geo->vertices[geo->indices[i + 1]];
            const auto& v2 = geo->vertices[geo->indices[i + 2]];

            SDL_Vertex sdl_verts[3];
            for (int j = 0; j < 3; j++) {
                const auto& v = (j == 0) ? v0 : (j == 1) ? v1 : v2;
                sdl_verts[j].position.x = v.position.x + translation.x;
                sdl_verts[j].position.y = v.position.y + translation.y;
                sdl_verts[j].color.r = v.colour.red;
                sdl_verts[j].color.g = v.colour.green;
                sdl_verts[j].color.b = v.colour.blue;
                sdl_verts[j].color.a = v.colour.alpha;
                sdl_verts[j].tex_coord.x = v.tex_coord.x;
                sdl_verts[j].tex_coord.y = v.tex_coord.y;
            }

            SDL_RenderGeometry(renderer, tex, sdl_verts, 3, nullptr, 0);
        }
    }

    void ReleaseGeometry(Rml::CompiledGeometryHandle geometry) override {
        delete reinterpret_cast<CompiledGeo*>(geometry);
    }

    void EnableScissorRegion(bool enable) override {
        if (enable) {
            SDL_RenderSetClipRect(renderer, &scissor);
        } else {
            SDL_RenderSetClipRect(renderer, nullptr);
        }
    }

    void SetScissorRegion(Rml::Rectanglei region) override {
        scissor = {region.Left(), region.Top(),
                   region.Width(), region.Height()};
    }

    Rml::TextureHandle LoadTexture(Rml::Vector2i& texture_dimensions,
                                    const Rml::String& source) override {
        SDL_Surface* surf = IMG_Load(source.c_str());
        if (!surf) return 0;
        texture_dimensions.x = surf->w;
        texture_dimensions.y = surf->h;
        SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf);
        SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
        SDL_FreeSurface(surf);
        return reinterpret_cast<Rml::TextureHandle>(tex);
    }

    Rml::TextureHandle GenerateTexture(Rml::Span<const Rml::byte> source,
                                        Rml::Vector2i source_dimensions) override {
        SDL_Surface* surf = SDL_CreateRGBSurfaceWithFormat(
            0, source_dimensions.x, source_dimensions.y, 32, SDL_PIXELFORMAT_RGBA32);
        if (!surf) return 0;
        std::memcpy(surf->pixels, source.data(), source.size());
        SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf);
        SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
        SDL_FreeSurface(surf);
        return reinterpret_cast<Rml::TextureHandle>(tex);
    }

    void ReleaseTexture(Rml::TextureHandle texture) override {
        SDL_DestroyTexture(reinterpret_cast<SDL_Texture*>(texture));
    }

private:
    struct CompiledGeo {
        std::vector<Rml::Vertex> vertices;
        std::vector<int> indices;
    };
    SDL_Renderer* renderer;
    SDL_Rect scissor = {};
};


class SDLSystemInterface : public Rml::SystemInterface {
public:
    double GetElapsedTime() override {
        return SDL_GetTicks() / 1000.0;
    }
    bool LogMessage(Rml::Log::Type type, const Rml::String& message) override {
        printf("[RmlUI] %s\n", message.c_str());
        return true;
    }
};

static SDLRenderInterface* s_renderIface = nullptr;
static SDLSystemInterface* s_systemIface = nullptr;


static Rml::Input::KeyIdentifier sdlKeyToRml(SDL_Keycode key) {
    switch (key) {
        case SDLK_RETURN:    return Rml::Input::KI_RETURN;
        case SDLK_ESCAPE:    return Rml::Input::KI_ESCAPE;
        case SDLK_BACKSPACE: return Rml::Input::KI_BACK;
        case SDLK_TAB:       return Rml::Input::KI_TAB;
        case SDLK_SPACE:     return Rml::Input::KI_SPACE;
        case SDLK_LEFT:      return Rml::Input::KI_LEFT;
        case SDLK_RIGHT:     return Rml::Input::KI_RIGHT;
        case SDLK_UP:        return Rml::Input::KI_UP;
        case SDLK_DOWN:      return Rml::Input::KI_DOWN;
        case SDLK_DELETE:    return Rml::Input::KI_DELETE;
        default:             return Rml::Input::KI_UNKNOWN;
    }
}

static int sdlModToRml(int sdlMod) {
    int mod = 0;
    if (sdlMod & KMOD_CTRL)  mod |= Rml::Input::KM_CTRL;
    if (sdlMod & KMOD_SHIFT) mod |= Rml::Input::KM_SHIFT;
    if (sdlMod & KMOD_ALT)   mod |= Rml::Input::KM_ALT;
    return mod;
}


namespace RmlBackend {

bool init(SDL_Window* window, SDL_Renderer* renderer) {
    s_window = window;
    s_renderer = renderer;

    s_systemIface = new SDLSystemInterface();
    s_renderIface = new SDLRenderInterface(renderer);

    Rml::SetSystemInterface(s_systemIface);
    Rml::SetRenderInterface(s_renderIface);

    if (!Rml::Initialise()) {
        printf("[RmlUI] Failed to initialise\n");
        return false;
    }

    auto& eng = Engine::instance();
    std::vector<std::string> fontPaths;
    if (!eng.fontPath.empty()) fontPaths.push_back(eng.fontPath);
    fontPaths.push_back(eng.assetsPath + "SourceSans3-Regular.ttf");
    fontPaths.push_back(eng.assetsPath + "arial.TTF");
    fontPaths.push_back(eng.assetsPath + "arial.ttf");
    fontPaths.push_back("assets/SourceSans3-Regular.ttf");
    fontPaths.push_back("assets/arial.TTF");
    fontPaths.push_back("assets/arial.ttf");
    fontPaths.push_back("SourceSans3-Regular.ttf");
    fontPaths.push_back("arial.TTF");

    bool fontLoaded = false;
    for (const auto& fontPath : fontPaths) {
        if (fontPath.empty()) continue;
        if (Rml::LoadFontFace(fontPath, true)) {
            fontLoaded = true;
            printf("[RmlUI] Font loaded: %s\n", fontPath.c_str());
            break;
        }
    }
    if (!fontLoaded) {
        printf("[RmlUI] WARNING: No font loaded!\n");
    }


    int w, h;
    SDL_GetWindowSize(window, &w, &h);
    s_context = Rml::CreateContext("main", Rml::Vector2i(w, h));
    if (!s_context) {
        printf("[RmlUI] Failed to create context\n");
        return false;
    }

    printf("[RmlUI] Initialized: %dx%d\n", w, h);
    return true;
}

void shutdown() {
    if (s_context) {
        Rml::RemoveContext("main");
        s_context = nullptr;
    }
    Rml::Shutdown();
    delete s_renderIface; s_renderIface = nullptr;
    delete s_systemIface; s_systemIface = nullptr;
}

void beginFrame() {

}

void endFrame() {
    if (s_context) {
        s_context->Update();
        s_context->Render();
    }
}

void processEvent(const SDL_Event& event) {
    if (!s_context) return;

    switch (event.type) {
        case SDL_MOUSEMOTION:
            s_context->ProcessMouseMove(event.motion.x, event.motion.y,
                                         sdlModToRml(SDL_GetModState()));
            break;
        case SDL_MOUSEBUTTONDOWN:
            s_context->ProcessMouseButtonDown(event.button.button - 1,
                                               sdlModToRml(SDL_GetModState()));
            break;
        case SDL_MOUSEBUTTONUP:
            s_context->ProcessMouseButtonUp(event.button.button - 1,
                                             sdlModToRml(SDL_GetModState()));
            break;
        case SDL_MOUSEWHEEL:
            s_context->ProcessMouseWheel(Rml::Vector2f(0, -(float)event.wheel.y),
                                          sdlModToRml(SDL_GetModState()));
            break;
        case SDL_KEYDOWN:
            s_context->ProcessKeyDown(sdlKeyToRml(event.key.keysym.sym),
                                       sdlModToRml(event.key.keysym.mod));
            break;
        case SDL_KEYUP:
            s_context->ProcessKeyUp(sdlKeyToRml(event.key.keysym.sym),
                                     sdlModToRml(event.key.keysym.mod));
            break;
        case SDL_TEXTINPUT:
            s_context->ProcessTextInput(Rml::String(event.text.text));
            break;
    }
}

Rml::Context* getContext() {
    return s_context;
}

Rml::ElementDocument* loadDocument(const std::string& path) {
    if (!s_context) return nullptr;
    Rml::ElementDocument* doc = s_context->LoadDocument(path);
    if (doc) {
        doc->Show();
        printf("[RmlUI] Loaded document: %s\n", path.c_str());
    } else {
        printf("[RmlUI] Failed to load: %s\n", path.c_str());
    }
    return doc;
}

}
