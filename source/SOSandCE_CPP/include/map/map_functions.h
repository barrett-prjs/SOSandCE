#pragma once
#include "core/common.h"

class CountryData;

namespace MapFunc {

    void fill(SDL_Surface* surface, float x, float y, Color newColor);


    void fillRegionMask(SDL_Surface* surface, SDL_Surface* maskSurface,
                        float x, float y, Color newColor);
    void fillRegionMask(SDL_Surface* surface, SDL_Surface* maskSurface,
                        int regionId, float x, float y, Color newColor);


    void fillWithBorder(SDL_Surface* surface, SDL_Surface* referenceSurface,
                        float x, float y, Color newColor);


    void fillFixBorder(SDL_Surface* surface, float x, float y, Color newColor);


    SDL_Surface* fixBorders(SDL_Surface* mapSurface,
                            const std::vector<Color>& toChange = {{0,0,0}},
                            const std::vector<Color>& toIgnore = {{105,118,132},{126,142,158}});
}
