#pragma once
#include "core/common.h"

class GameState;

enum class MapMode {
    POLITICAL = 0,
    FACTION,
    IDEOLOGY,
    INDUSTRY,
    BIOME,
    RESOURCE,
    COUNT
};

class MapManager {
public:
    MapManager();
    ~MapManager();

    void loadMaps(const std::string& scenarioPath);
    void setMode(MapMode mode);
    MapMode getMode() const { return currentMode_; }


    SDL_Surface* getActiveSurface() const;
    SDL_Surface* getPoliticalMap() const { return politicalMap_; }
    SDL_Surface* getFactionMap() const { return factionMap_; }
    SDL_Surface* getIdeologyMap() const { return ideologyMap_; }
    SDL_Surface* getIndustryMap() const { return industryMap_; }
    SDL_Surface* getBiomeMap() const { return biomeMap_; }
    SDL_Surface* getResourceMap() const { return resourceMap_; }
    SDL_Surface* getRegionsMap() const { return regionsMap_; }
    SDL_Surface* getCultureMap() const { return cultureMap_; }
    SDL_Surface* getWorldRegionsMap() const { return worldRegionsMap_; }
    SDL_Surface* getModifiedIndustryMap() const { return modifiedIndustryMap_; }


    void markDirty() { textureDirty_ = true; }
    void rebuildTextures(SDL_Renderer* r) { textureDirty_ = true; }
    SDL_Texture* getMapTexture(SDL_Renderer* r);


    void reloadIndustryMap(GameState& gs);
    void generateResourceMap(GameState& gs);

    int mapWidth() const;
    int mapHeight() const;

private:
    SDL_Surface* politicalMap_ = nullptr;
    SDL_Surface* factionMap_ = nullptr;
    SDL_Surface* ideologyMap_ = nullptr;
    SDL_Surface* industryMap_ = nullptr;
    SDL_Surface* biomeMap_ = nullptr;
    SDL_Surface* resourceMap_ = nullptr;
    SDL_Surface* regionsMap_ = nullptr;
    SDL_Surface* cultureMap_ = nullptr;
    SDL_Surface* worldRegionsMap_ = nullptr;
    SDL_Surface* modifiedIndustryMap_ = nullptr;

    SDL_Texture* cachedTexture_ = nullptr;
    MapMode currentMode_ = MapMode::POLITICAL;
    bool textureDirty_ = true;
};
