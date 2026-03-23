#pragma once
#include "core/common.h"
#include "game/combat.h"

class GameState;
class Country;


class Division {
public:
    Division(const std::string& country, int divisions, int region, Color color, GameState& gs);
    ~Division();


    std::string country;
    std::string lastCountry;
    std::string type = "division";
    int         region;
    Vec2        location;
    Color       color;
    Color       currentColor;


    int   divisionStack;
    float maxUnits;
    float units;
    float maxResources;
    float resources;
    float attack;
    float defense;
    float movementSpeed;
    float movement;
    float resourceUse = 1000;
    CombatStats combatStats;


    SDL_Texture* sprite = nullptr;
    float xBlit = 0, yBlit = 0;
    int   spriteW = 0, spriteH = 0;
    float healthSize   = -1;
    float resourceSize = -1;


    std::vector<int> commands;
    bool commandIgnoreEnemy = false;
    bool commandIgnoreWater = true;
    int trackedCommandRegion = -1;
    std::string trackedCommandOwner;
    bool fighting   = false;
    bool recovering = false;
    bool locked     = false;
    bool navalLocked = false;
    int  attempts   = 0;
    int  lastAIOrderHour = -1;
    int  aiObjectiveRegion = -1;


    void update(GameState& gs);
    void updateLocation(GameState& gs);
    void move(int region, GameState& gs);
    void command(int targetRegion, GameState& gs, bool ignoreEnemy = false, bool ignoreWater = true, int iterations = 100);
    void reloadSprite(GameState& gs, Color bgColor = {0, 0, 0, 0});
    void kill(GameState& gs);

private:
    GameState& gs_;
};
