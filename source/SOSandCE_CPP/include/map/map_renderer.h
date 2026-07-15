#pragma once
#include "core/common.h"
#include <future>

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


    void renderMap(SDL_Renderer* r, SDL_Surface* mapSurface, const Camera& cam, int screenW, int screenH,
                   SDL_Surface* terrainSurface = nullptr);
    void renderDivisions(SDL_Renderer* r, GameState& gs, const Camera& cam, int screenW, int screenH,
                         const std::vector<Division*>* highlightedDivisions = nullptr);
    void renderBattles(SDL_Renderer* r, GameState& gs, const Camera& cam, int screenW, int screenH);
    void renderCities(SDL_Renderer* r, GameState& gs, const Camera& cam, int screenW, int screenH,
                      bool onlyCities = false);
    void renderCommands(SDL_Renderer* r, GameState& gs, const Camera& cam, int screenW, int screenH,
                        const std::vector<int>& selectedRegions, const std::vector<Division*>& selectedDivisions);
    void renderFogOverlay(SDL_Renderer* r, FogOfWar& fog, const Camera& cam, int screenW, int screenH);
    void renderCountryHighlight(SDL_Renderer* r, GameState& gs, const Camera& cam,
                                int screenW, int screenH, const std::string& countryName);
    void renderPuppetStripes(SDL_Renderer* r, GameState& gs, const Camera& cam,
                             int screenW, int screenH);


    int regionAtScreen(int sx, int sy, const Camera& cam, int screenW, int screenH,
                       SDL_Surface* regionsMap);


    void invalidateTexture() { mapTextureDirty_ = true; }


    void setStyleContext(SDL_Surface* regionsMask) { styleMask_ = regionsMask; }


    void setClock(float hours, float anchorWorldX, int month, int day);

private:
    float clockHours_ = -1.0f;
    float clockAnchorX_ = 0.0f;
    int clockDayOfYear_ = 172;
    SDL_Texture* nightTex_ = nullptr;
    float nightTexHours_ = -999.0f;
    float nightTexAnchor_ = -999.0f;
    uint32_t nightTexBuiltTicks_ = 0;

    SDL_Texture* selOverlayTex_ = nullptr;
    std::vector<int> selOverlayKey_;
    SDL_Surface* selOverlayMask_ = nullptr;

    SDL_Texture* hlTex_ = nullptr;
    std::string hlKey_;
    SDL_Surface* hlMask_ = nullptr;

    SDL_Texture* pupTex_ = nullptr;
    std::string pupKey_;
    SDL_Surface* pupMask_ = nullptr;

    SDL_Texture* mapTexture_ = nullptr;
    SDL_Surface* lastSurface_ = nullptr;
    int lastTextureW_ = 0, lastTextureH_ = 0;

    SDL_Texture* terrainTexture_ = nullptr;
    SDL_Surface* lastTerrainSurface_ = nullptr;
    SDL_Surface* styleMask_ = nullptr;
    bool mapTextureDirty_ = false;
    uint32_t lastStyleBuildTicks_ = 0;

    std::future<SDL_Surface*> styleJob_;
    SDL_Surface* styleSnapshot_ = nullptr;
    SDL_Surface* pendingDisplay_ = nullptr;
    int pendingRow_ = 0;


    void renderWrapped(SDL_Renderer* r, SDL_Surface* mapSurface, const Camera& cam,
                       int screenW, int screenH, SDL_Surface* terrainSurface);
};
