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


    void repaintRegionBorders(SDL_Surface* surface, SDL_Surface* maskSurface,
                              int regionId, float x, float y);


    SDL_Surface* fixBorders(SDL_Surface* mapSurface,
                            const std::vector<Color>& toChange = {{0,0,0}},
                            const std::vector<Color>& toIgnore = {{105,118,132},{126,142,158}});


    SDL_Surface* buildTerrainSurface(SDL_Surface* biomeMap, SDL_Surface* grainTex = nullptr,
                                     int scale = 2);


    SDL_Surface* stylizeForDisplay(SDL_Surface* mapSurface,
                                   SDL_Surface* regionsMask = nullptr,
                                   int scale = 1);


    void expandFillToLines(SDL_Surface* overlay, SDL_Surface* mask);
}
