#pragma once
#include "core/common.h"

class Division;
class GameState;


enum class VisLevel { NONE = 0, LIMITED = 1, FULL = 2 };


class FogOfWar {
public:
    FogOfWar();
    ~FogOfWar();

    bool enabled = true;

    VisLevel get(int regionId) const;
    bool     isDivisionVisible(const Division& div) const;
    void     recalculate(const std::string& countryName, GameState& gs);
    void     buildOverlay(SDL_Surface* regionsMap);
    SDL_Surface* getOverlay() const { return overlay_; }

    bool isDirty() const  { return dirty_; }
    void markDirty()      { dirty_ = true; }
    void clearVisibility();

private:
    std::unordered_map<int, VisLevel> visibility_;
    SDL_Surface* overlay_ = nullptr;
    bool dirty_ = true;
};
