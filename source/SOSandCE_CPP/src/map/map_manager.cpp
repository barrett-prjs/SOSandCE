#include "map/map_manager.h"
#include "map/map_functions.h"
#include "core/engine.h"
#include "data/region_data.h"
#include "game/game_state.h"
#include "game/country.h"
#include "game/economy.h"

MapManager::MapManager() = default;

MapManager::~MapManager() {
    if (politicalMap_)       SDL_FreeSurface(politicalMap_);
    if (factionMap_)         SDL_FreeSurface(factionMap_);
    if (ideologyMap_)        SDL_FreeSurface(ideologyMap_);
    if (industryMap_)        SDL_FreeSurface(industryMap_);
    if (biomeMap_)           SDL_FreeSurface(biomeMap_);
    if (resourceMap_)        SDL_FreeSurface(resourceMap_);
    if (regionsMap_)         SDL_FreeSurface(regionsMap_);
    if (cultureMap_)         SDL_FreeSurface(cultureMap_);
    if (worldRegionsMap_)    SDL_FreeSurface(worldRegionsMap_);
    if (modifiedIndustryMap_) SDL_FreeSurface(modifiedIndustryMap_);
    if (cachedTexture_)      SDL_DestroyTexture(cachedTexture_);
}


void MapManager::loadMaps(const std::string& scenarioPath) {
    auto& eng = Engine::instance();


    auto freeSurf = [](SDL_Surface*& s) {
        if (s) { SDL_FreeSurface(s); s = nullptr; }
    };
    freeSurf(politicalMap_);
    freeSurf(factionMap_);
    freeSurf(ideologyMap_);
    freeSurf(industryMap_);
    freeSurf(biomeMap_);
    freeSurf(resourceMap_);
    freeSurf(regionsMap_);
    freeSurf(cultureMap_);
    freeSurf(worldRegionsMap_);
    freeSurf(modifiedIndustryMap_);


    std::string startsDir = eng.assetsPath + "starts/" + scenarioPath + "/";
    printf("[MapManager] Loading maps from: %s\n", startsDir.c_str()); fflush(stdout);
    politicalMap_ = eng.loadSurface(startsDir + "map.png");
    if (!politicalMap_) {
        printf("[MapManager] WARNING: Failed to load political map from %s\n", (startsDir + "map.png").c_str());
        fflush(stdout);
    }


    factionMap_ = eng.loadSurface(startsDir + "factionMap.png");
    if (!factionMap_) factionMap_ = eng.loadSurface(startsDir + "factions.png");
    ideologyMap_ = eng.loadSurface(startsDir + "ideologyMap.png");
    if (!ideologyMap_) ideologyMap_ = eng.loadSurface(startsDir + "ideologies.png");


    if (!factionMap_) {
        factionMap_ = eng.loadSurface(eng.assetsPath + "maps/blank.png");
        if (!factionMap_ && politicalMap_) {
            factionMap_ = SDL_ConvertSurface(politicalMap_, politicalMap_->format, 0);
        }
    }
    if (!ideologyMap_) {
        ideologyMap_ = eng.loadSurface(eng.assetsPath + "maps/blank.png");
        if (!ideologyMap_ && politicalMap_) {
            ideologyMap_ = SDL_ConvertSurface(politicalMap_, politicalMap_->format, 0);
        }
    }


    std::string mapsDir = eng.assetsPath + "maps/";
    industryMap_ = eng.loadSurface(mapsDir + "industryMap.png");
    if (!industryMap_) industryMap_ = eng.loadSurface(mapsDir + "industry.png");
    biomeMap_ = eng.loadSurface(mapsDir + "biomeMap.png");
    if (!biomeMap_) biomeMap_ = eng.loadSurface(mapsDir + "biomes.png");
    regionsMap_ = eng.loadSurface(mapsDir + "regionsMap.png");
    if (!regionsMap_) regionsMap_ = eng.loadSurface(mapsDir + "regions.png");
    worldRegionsMap_ = eng.loadSurface(mapsDir + "worldRegionsMap.png");
    if (!worldRegionsMap_) worldRegionsMap_ = eng.loadSurface(mapsDir + "worldRegions.png");
    cultureMap_ = eng.loadSurface(mapsDir + "cultureMap.png");
    if (!cultureMap_) cultureMap_ = eng.loadSurface(mapsDir + "cultures.png");

    printf("[MapManager] Loaded: political=%d faction=%d ideology=%d industry=%d biome=%d regions=%d\n",
           politicalMap_ != nullptr, factionMap_ != nullptr, ideologyMap_ != nullptr,
           industryMap_ != nullptr, biomeMap_ != nullptr, regionsMap_ != nullptr);
    fflush(stdout);


    if (!industryMap_ && politicalMap_) {
        industryMap_ = SDL_ConvertSurface(politicalMap_, politicalMap_->format, 0);
    }


    if (industryMap_) {
        modifiedIndustryMap_ = SDL_ConvertSurface(industryMap_, industryMap_->format, 0);
    }


    if (politicalMap_) {
        resourceMap_ = SDL_CreateRGBSurfaceWithFormat(
            0, politicalMap_->w, politicalMap_->h,
            politicalMap_->format->BitsPerPixel,
            politicalMap_->format->format);
        if (resourceMap_) {
            SDL_FillRect(resourceMap_, nullptr,
                SDL_MapRGB(resourceMap_->format, 126, 142, 158));
        }
    }


    if (cachedTexture_) {
        SDL_DestroyTexture(cachedTexture_);
        cachedTexture_ = nullptr;
    }
    textureDirty_ = true;
}


void MapManager::setMode(MapMode mode) {
    if (currentMode_ != mode) {
        currentMode_ = mode;
        textureDirty_ = true;
    }
}


SDL_Surface* MapManager::getActiveSurface() const {
    switch (currentMode_) {
        case MapMode::POLITICAL: return politicalMap_;
        case MapMode::FACTION:   return factionMap_;
        case MapMode::IDEOLOGY:  return ideologyMap_;
        case MapMode::INDUSTRY:  return modifiedIndustryMap_ ? modifiedIndustryMap_ : industryMap_;
        case MapMode::BIOME:     return biomeMap_;
        case MapMode::RESOURCE:  return resourceMap_;
        default:                 return politicalMap_;
    }
}


SDL_Texture* MapManager::getMapTexture(SDL_Renderer* r) {
    if (!textureDirty_ && cachedTexture_) return cachedTexture_;

    SDL_Surface* active = getActiveSurface();
    if (!active) return nullptr;

    if (cachedTexture_) {
        SDL_DestroyTexture(cachedTexture_);
        cachedTexture_ = nullptr;
    }

    cachedTexture_ = SDL_CreateTextureFromSurface(r, active);
    textureDirty_ = false;
    return cachedTexture_;
}


void MapManager::reloadIndustryMap(GameState& gs) {
    if (!industryMap_) return;


    if (modifiedIndustryMap_) {
        SDL_FreeSurface(modifiedIndustryMap_);
        modifiedIndustryMap_ = nullptr;
    }


    modifiedIndustryMap_ = SDL_ConvertSurface(industryMap_, industryMap_->format, 0);
    if (!modifiedIndustryMap_) return;

    SDL_LockSurface(modifiedIndustryMap_);

    Country* controlled = gs.getCountry(gs.controlledCountry);
    if (!controlled) {
        SDL_UnlockSurface(modifiedIndustryMap_);
        textureDirty_ = true;
        return;
    }


    std::set<int> ownedRegions(controlled->regions.begin(), controlled->regions.end());
    auto& regData = RegionData::instance();

    int w = modifiedIndustryMap_->w;
    int h = modifiedIndustryMap_->h;


    if (regionsMap_) {
        SDL_LockSurface(regionsMap_);
        for (int y = 0; y < h; y++) {
            for (int x = 0; x < w; x++) {
                Color regColor = getPixel(regionsMap_, x, y);
                int regionId = regData.getRegion(regColor);

                if (regionId >= 0 && ownedRegions.find(regionId) == ownedRegions.end()) {

                    Color c = getPixel(modifiedIndustryMap_, x, y);
                    uint8_t grey = static_cast<uint8_t>(0.3f * c.r + 0.59f * c.g + 0.11f * c.b);
                    setPixel(modifiedIndustryMap_, x, y, {grey, grey, grey, c.a});
                }
            }
        }
        SDL_UnlockSurface(regionsMap_);
    }

    SDL_UnlockSurface(modifiedIndustryMap_);
    textureDirty_ = true;
}


void MapManager::generateResourceMap(GameState& gs) {
    if (!industryMap_) return;

    if (resourceMap_) {
        SDL_FreeSurface(resourceMap_);
    }

    resourceMap_ = SDL_ConvertSurface(industryMap_, industryMap_->format, 0);
    if (!resourceMap_) return;


    static const std::unordered_map<std::string, Color> resourceColors = {
        {"oil",       {50, 50, 60}},
        {"steel",     {100, 140, 220}},
        {"aluminum",  {170, 220, 255}},
        {"tungsten",  {220, 150, 40}},
        {"chromium",  {200, 60, 200}},
        {"rubber",    {40, 190, 40}},
    };

    auto& regData = RegionData::instance();


    std::unordered_map<int, Color> regionResourceColor;

    for (auto& [name, country] : gs.countries) {
        if (!country) continue;
        for (int regId : country->regions) {
            const auto& res = regData.getResources(regId);
            if (res.empty()) continue;


            float maxVal = 0;
            std::string dominant;
            for (const auto& [rname, rval] : res) {
                if (rval > maxVal) {
                    maxVal = rval;
                    dominant = rname;
                }
            }
            if (!dominant.empty()) {
                auto it = resourceColors.find(dominant);
                if (it != resourceColors.end()) {
                    regionResourceColor[regId] = it->second;
                }
            }
        }
    }


    SDL_LockSurface(resourceMap_);
    SDL_LockSurface(regionsMap_);

    int w = resourceMap_->w;
    int h = resourceMap_->h;

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {

            Color baseColor = getPixel(resourceMap_, x, y);
            if (baseColor.r != 255 || baseColor.g != 255 || baseColor.b != 255) continue;


            Color regColor = getPixel(regionsMap_, x, y);
            if (regColor.r == 0 && regColor.g == 0 && regColor.b == 0) continue;

            int regionId = regData.getRegion(regColor);
            if (regionId < 0) continue;

            auto it = regionResourceColor.find(regionId);
            if (it != regionResourceColor.end()) {
                setPixel(resourceMap_, x, y, it->second);
            }

        }
    }

    SDL_UnlockSurface(regionsMap_);
    SDL_UnlockSurface(resourceMap_);

    textureDirty_ = true;
}


int MapManager::mapWidth() const {
    return politicalMap_ ? politicalMap_->w : 1275;
}

int MapManager::mapHeight() const {
    return politicalMap_ ? politicalMap_->h : 600;
}
