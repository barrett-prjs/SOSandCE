#pragma once
#include "core/common.h"

class GameState;


struct PuppetState {
    std::string overlord;
    std::string puppet;
    float       autonomy = 50.0f;
    float       autonomyGrowth = 0.01f;
    float       tributeRate = 0.25f;
    bool        canDeclareWar = false;
    bool        canDiplomacy = false;
    bool        active = true;

    void update(GameState& gs);
    void release(GameState& gs);
    void annex(GameState& gs);
    void changeAutonomy(float delta);
    bool reduceAutonomy(float amount, GameState& gs);
    bool isFullyAutonomous() const;
    float getTributeAmount(GameState& gs) const;
};
