#pragma once

#include "core/common.h"

class Engine {
public:
    static Engine& instance();

    bool init(int width = 1200, int height = 675);
    void shutdown();

    SDL_Window*   window   = nullptr;
    SDL_Renderer* renderer = nullptr;
    int WIDTH  = 1200;
    int HEIGHT = 675;
    int FPS    = 60;
    float uiScale = 24.0f;


    void recalcUI(float userUiSize = 24.0f);
    float autoUIScale() const;
    float uiScaleFactor() const;


    SDL_Texture* loadTexture(const std::string& path);


    SDL_Surface* loadSurface(const std::string& path);


    SDL_Texture* surfaceToTexture(SDL_Surface* s);


    void present();


    void clear(Color c = {0, 0, 0, 255});


    TTF_Font* getFont(int size);

    std::string basePath;
    std::string assetsPath;

private:
    Engine() = default;
    ~Engine() = default;
    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;

    std::unordered_map<std::string, SDL_Texture*> textureCache;
    std::unordered_map<int, TTF_Font*> fontCache;
public:
    std::string fontPath;
};
