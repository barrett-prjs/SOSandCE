#include "game/fog_of_war.h"
#include "game/game_state.h"
#include "game/country.h"
#include "game/faction.h"
#include "game/division.h"
#include "data/region_data.h"


FogOfWar::FogOfWar() = default;

FogOfWar::~FogOfWar() {
    if (overlay_) {
        SDL_FreeSurface(overlay_);
        overlay_ = nullptr;
    }
}

VisLevel FogOfWar::get(int regionId) const {
    if (!enabled) return VisLevel::FULL;
    auto it = visibility_.find(regionId);
    if (it != visibility_.end()) return it->second;
    return VisLevel::NONE;
}

bool FogOfWar::isDivisionVisible(const Division& div) const {
    return get(div.region) >= VisLevel::LIMITED;
}

void FogOfWar::clearVisibility() {
    visibility_.clear();
    dirty_ = true;
}

void FogOfWar::recalculate(const std::string& countryName, GameState& gs) {
    if (!enabled) return;

    auto oldVis = visibility_;
    std::unordered_map<int, VisLevel> newVis;
    std::set<int> fullRegions;

    Country* country = gs.getCountry(countryName);
    if (!country) return;


    for (int rid : country->regions) {
        fullRegions.insert(rid);
    }


    for (auto& div : country->divisions) {
        if (div) {
            fullRegions.insert(div->region);
        }
    }


    if (!country->faction.empty()) {
        Faction* fac = gs.getFaction(country->faction);
        if (fac) {
            for (auto& memberName : fac->members) {
                if (memberName == countryName) continue;

                Country* member = gs.getCountry(memberName);
                if (!member) continue;

                for (int rid : member->regions) {
                    fullRegions.insert(rid);
                }
                for (auto& div : member->divisions) {
                    if (div) {
                        fullRegions.insert(div->region);
                    }
                }
            }
        }
    }


    for (int rid : fullRegions) {
        newVis[rid] = VisLevel::FULL;
    }


    auto& rd = RegionData::instance();
    for (int fullRid : fullRegions) {
        const auto& connections = rd.getConnections(fullRid);
        for (int neighbor : connections) {
            if (fullRegions.count(neighbor) == 0) {
                newVis[neighbor] = VisLevel::LIMITED;
            }
        }
    }

    visibility_ = newVis;


    if (newVis != oldVis) {
        dirty_ = true;
    }
}

void FogOfWar::buildOverlay(SDL_Surface* regionsMap) {
    if (!regionsMap) return;

    int width = regionsMap->w;
    int height = regionsMap->h;


    if (overlay_) {
        SDL_FreeSurface(overlay_);
    }


    overlay_ = SDL_CreateRGBSurfaceWithFormat(0, width, height, 32, SDL_PIXELFORMAT_RGBA8888);
    if (!overlay_) return;


    SDL_FillRect(overlay_, nullptr, SDL_MapRGBA(overlay_->format, 0, 0, 0, 160));


    SDL_LockSurface(overlay_);
    SDL_LockSurface(regionsMap);


    SDL_UnlockSurface(regionsMap);
    SDL_UnlockSurface(overlay_);

    dirty_ = false;
}
