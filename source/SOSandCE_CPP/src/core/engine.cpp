#include "core/engine.h"

Engine& Engine::instance() {
    static Engine eng;
    return eng;
}

bool Engine::init(int width, int height) {
    printf("[Engine] SDL_Init...\n"); fflush(stdout);
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER) < 0) {
        printf("[Engine] SDL_Init FAILED: %s\n", SDL_GetError());
        return false;
    }
    if (IMG_Init(IMG_INIT_PNG) == 0) {
        SDL_Log("IMG_Init failed: %s", IMG_GetError());
        return false;
    }
    if (TTF_Init() < 0) {
        SDL_Log("TTF_Init failed: %s", TTF_GetError());
        return false;
    }


    char* bp = SDL_GetBasePath();
    if (bp) { basePath = bp; SDL_free(bp); }
    else basePath = "./";
    assetsPath = basePath + "assets/";

    auto hasBundledFont = [](const std::string& root) {
        return fs::exists(root + "SourceSans3-Regular.ttf") ||
               fs::exists(root + "arial.TTF") ||
               fs::exists(root + "arial.ttf");
    };


    if (!hasBundledFont(assetsPath)) {

        std::string altPath = basePath + "../../assets/";
        if (hasBundledFont(altPath)) {
            assetsPath = fs::canonical(altPath).string() + "/";

            std::replace(assetsPath.begin(), assetsPath.end(), '\\', '/');
            if (assetsPath.back() != '/') assetsPath += '/';
        }
    }

    printf("[Engine] basePath: %s\n", basePath.c_str());
    printf("[Engine] assetsPath: %s\n", assetsPath.c_str());
    printf("[Engine] font search: %s\n", assetsPath.c_str());
    fflush(stdout);


    std::vector<std::string> fontCandidates = {
        assetsPath + "SourceSans3-Regular.ttf",
        assetsPath + "arial.TTF",
        assetsPath + "arial.ttf",
        basePath + "SourceSans3-Regular.ttf",
        basePath + "arial.TTF",
        basePath + "arial.ttf"
    };
    fontPath.clear();
    for (const auto& candidate : fontCandidates) {
        if (fs::exists(candidate)) {
            fontPath = candidate;
            break;
        }
    }


    SDL_DisplayMode dm;
    SDL_GetCurrentDisplayMode(0, &dm);
    WIDTH = dm.w;
    HEIGHT = dm.h;
    printf("[Engine] Display: %dx%d\n", dm.w, dm.h);


    window = SDL_CreateWindow(
        "Spirits of Steel: Community Edition v2.0",
        SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
        WIDTH, HEIGHT,
        SDL_WINDOW_SHOWN | SDL_WINDOW_FULLSCREEN_DESKTOP | SDL_WINDOW_RESIZABLE | SDL_WINDOW_MAXIMIZED
    );
    if (!window) {
        SDL_Log("SDL_CreateWindow failed: %s", SDL_GetError());
        return false;
    }

    renderer = SDL_CreateRenderer(window, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) {
        SDL_Log("SDL_CreateRenderer failed: %s", SDL_GetError());
        return false;
    }

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);


    std::string iconPath = assetsPath + "icon.png";
    if (!fs::exists(iconPath)) iconPath = basePath + "icon.png";
    if (fs::exists(iconPath)) {
        SDL_Surface* icon = IMG_Load(iconPath.c_str());
        if (icon) { SDL_SetWindowIcon(window, icon); SDL_FreeSurface(icon); }
    }

    recalcUI();
    return true;
}

float Engine::autoUIScale() const {
    return std::max(12.0f, static_cast<float>(HEIGHT) / 675.0f * 24.0f);
}

float Engine::uiScaleFactor() const {
    float autoScale = autoUIScale();
    return autoScale > 0.0f ? (uiScale / autoScale) : 1.0f;
}

void Engine::recalcUI(float userUiSize) {
    float autoScale = autoUIScale();
    uiScale = autoScale * std::max(0.5f, userUiSize / 24.0f);
}

void Engine::shutdown() {
    for (auto& [k, t] : textureCache) SDL_DestroyTexture(t);
    textureCache.clear();
    for (auto& [k, f] : fontCache) TTF_CloseFont(f);
    fontCache.clear();
    if (renderer) { SDL_DestroyRenderer(renderer); renderer = nullptr; }
    if (window) { SDL_DestroyWindow(window); window = nullptr; }
    TTF_Quit();
    IMG_Quit();
    SDL_Quit();
}

TTF_Font* Engine::getFont(int size) {
    size = std::max(1, size);
    auto it = fontCache.find(size);
    if (it != fontCache.end()) return it->second;
    TTF_Font* f = TTF_OpenFont(fontPath.c_str(), size);
    if (!f) {
        SDL_Log("Failed to open font %s size %d: %s", fontPath.c_str(), size, TTF_GetError());
        return nullptr;
    }
    fontCache[size] = f;
    return f;
}

SDL_Texture* Engine::loadTexture(const std::string& path) {
    auto it = textureCache.find(path);
    if (it != textureCache.end()) return it->second;
    SDL_Surface* s = IMG_Load(path.c_str());
    if (!s) return nullptr;
    SDL_Texture* t = SDL_CreateTextureFromSurface(renderer, s);
    SDL_FreeSurface(s);
    if (t) textureCache[path] = t;
    return t;
}

SDL_Surface* Engine::loadSurface(const std::string& path) {
    SDL_Surface* s = IMG_Load(path.c_str());
    if (!s) {
        SDL_Log("Failed to load surface: %s - %s", path.c_str(), IMG_GetError());
        return nullptr;
    }

    SDL_Surface* converted = SDL_ConvertSurfaceFormat(s, SDL_PIXELFORMAT_RGBA32, 0);
    SDL_FreeSurface(s);
    return converted;
}

SDL_Texture* Engine::surfaceToTexture(SDL_Surface* s) {
    if (!s) return nullptr;
    return SDL_CreateTextureFromSurface(renderer, s);
}

void Engine::present() {
    SDL_RenderPresent(renderer);
}

void Engine::clear(Color c) {
    SDL_SetRenderDrawColor(renderer, c.r, c.g, c.b, c.a);
    SDL_RenderClear(renderer);
}
