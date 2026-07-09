#pragma once
#include "core/common.h"

class Division;
class GameState;


struct Battle {
    Division* attacker = nullptr;
    Division* defender = nullptr;
    Vec2      location;
    float     progress = 0;
    bool      finished = false;
    SDL_Texture* image = nullptr;
    int imageW = 0, imageH = 0;


    std::array<float, 4> attackerBiome = {1, 1, 1, 1};
    std::array<float, 4> defenderBiome = {1, 1, 1, 1};

    Battle(Division* atk, Division* def, GameState& gs);
    void update(GameState& gs);
    void reloadImage(GameState& gs);
    void draw(SDL_Renderer* r, float camx, float camy, float zoom, int W, int H);

    int  countFronts(GameState& gs) const;
    void attackerWin(GameState& gs);
    void defenderWin(GameState& gs);
};
