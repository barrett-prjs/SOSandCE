#include "game/puppet.h"
#include "game/game_state.h"
#include "game/country.h"

namespace {
void revokeMutualAccess(Country* a, Country* b) {
    if (!a || !b) return;
    a->militaryAccess.erase(std::remove(a->militaryAccess.begin(), a->militaryAccess.end(), b->name), a->militaryAccess.end());
    b->militaryAccess.erase(std::remove(b->militaryAccess.begin(), b->militaryAccess.end(), a->name), b->militaryAccess.end());
}
}


static constexpr float AUTONOMY_GAIN_PER_TICK   = 0.01f;
static constexpr float REVOLT_THRESHOLD          = 95.0f;
static constexpr float ANNEX_THRESHOLD           = 5.0f;
static constexpr float TRIBUTE_BASE_PCT          = 0.25f;
static constexpr float TRIBUTE_AUTONOMY_SCALE    = 0.003f;
static constexpr float REDUCE_AUTONOMY_PP_COST   = 50.0f;


void PuppetState::update(GameState& gs) {
    if (!active) return;


    float speedVal = static_cast<float>(gs.speed);
    autonomy = std::min(100.0f, autonomy + autonomyGrowth * speedVal);


    if (autonomy >= REVOLT_THRESHOLD) {
        release(gs);
        return;
    }


    if (autonomy <= ANNEX_THRESHOLD) {
        annex(gs);
        return;
    }


    Country* overlordObj = gs.getCountry(overlord);
    Country* puppetObj = gs.getCountry(puppet);
    if (!overlordObj || !puppetObj) return;

    float tributeFraction = tributeRate - autonomy * TRIBUTE_AUTONOMY_SCALE;
    tributeFraction = std::max(0.0f, std::min(1.0f, tributeFraction));


    for (int i = 0; i < RESOURCE_COUNT; ++i) {
        float tribute = puppetObj->resourceManager.production[i] * tributeFraction;
        if (tribute > 0.0f) {
            overlordObj->resourceManager.tradeImports[i] += tribute;
            puppetObj->resourceManager.tradeExports[i] += tribute;
        }
    }
}

void PuppetState::release(GameState& gs) {
    active = false;

    Country* overlordObj = gs.getCountry(overlord);
    Country* puppetObj = gs.getCountry(puppet);
    revokeMutualAccess(overlordObj, puppetObj);
    if (puppetObj) {
        puppetObj->puppetTo.clear();
    }
}

void PuppetState::annex(GameState& gs) {
    active = false;

    Country* overlordObj = gs.getCountry(overlord);
    Country* puppetObj = gs.getCountry(puppet);
    if (!overlordObj || !puppetObj) return;


    std::vector<int> regionsToTransfer = puppetObj->regions;
    for (int rid : regionsToTransfer) {
        overlordObj->addRegion(rid, gs);
    }

    gs.removeCountry(puppet);
}

void PuppetState::changeAutonomy(float delta) {
    autonomy = std::max(0.0f, std::min(100.0f, autonomy + delta));
}

bool PuppetState::reduceAutonomy(float amount, GameState& gs) {
    Country* overlordObj = gs.getCountry(overlord);
    if (!overlordObj) return false;

    float cost = REDUCE_AUTONOMY_PP_COST * (amount / 10.0f);
    if (overlordObj->politicalPower < cost) return false;

    overlordObj->politicalPower -= cost;
    autonomy = std::max(0.0f, autonomy - amount);
    return true;
}

bool PuppetState::isFullyAutonomous() const {
    return autonomy >= 100.0f;
}

float PuppetState::getTributeAmount(GameState& ) const {
    float tributeFraction = tributeRate - autonomy * TRIBUTE_AUTONOMY_SCALE;
    return std::max(0.0f, std::min(1.0f, tributeFraction));
}
