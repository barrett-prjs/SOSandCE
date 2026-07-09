#pragma once
#include "core/common.h"

class GameState;
class MapManager;
class FogOfWar;
class Division;

struct Camera {
    float x = 0, y = 0;
    float zoom = 1.0f;
    float mapWidth = 1275.0f;
    float mapHeight = 600.0f;

    void pan(float dx, float dy);
    void zoomBy(float factor, float centerX, float centerY, int screenW, int screenH);
    void clampZoom(float minZoom = 0.5f, float maxZoom = 50.0f);


    float worldToScreenX(float wx, int screenW) const;
    float worldToScreenY(float wy, int screenH) const;


    Vec2 screenToWorld(int sx, int sy, int screenW, int screenH) const;


    float normalizeX(float wx) const;
};

class MapRenderer {
public:
    MapRenderer();
    ~MapRenderer();


    void renderMap(SDL_Renderer* r, SDL_Surface* mapSurface, const Camera& cam, int screenW, int screenH);
    void renderDivisions(SDL_Renderer* r, GameState& gs, const Camera& cam, int screenW, int screenH,
                         const std::vector<Division*>* highlightedDivisions = nullptr);
    void renderBattles(SDL_Renderer* r, GameState& gs, const Camera& cam, int screenW, int screenH);
    void renderCities(SDL_Renderer* r, GameState& gs, const Camera& cam, int screenW, int screenH,
                      bool onlyCities = false);
    void renderCommands(SDL_Renderer* r, GameState& gs, const Camera& cam, int screenW, int screenH,
                        const std::vector<int>& selectedRegions, const std::vector<Division*>& selectedDivisions);
    void renderFogOverlay(SDL_Renderer* r, FogOfWar& fog, const Camera& cam, int screenW, int screenH);


    int regionAtScreen(int sx, int sy, const Camera& cam, int screenW, int screenH,
                       SDL_Surface* regionsMap);


    void invalidateTexture() { if (mapTexture_) { SDL_DestroyTexture(mapTexture_); mapTexture_ = nullptr; } }

private:
    SDL_Texture* mapTexture_ = nullptr;
    SDL_Surface* lastSurface_ = nullptr;
    int lastTextureW_ = 0, lastTextureH_ = 0;


    void renderWrapped(SDL_Renderer* r, SDL_Surface* mapSurface, const Camera& cam,
                       int screenW, int screenH);
};
