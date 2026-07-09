#include "game/battle.h"
#include "game/division.h"
#include "game/game_state.h"
#include "game/country.h"
#include "game/combat.h"
#include "data/region_data.h"
#include "core/engine.h"
#include "ui/theme.h"

Battle::Battle(Division* atk, Division* def, GameState& gs)
    : attacker(atk), defender(def)
{
    if (!attacker || !defender) {
        finished = true;
        return;
    }

    location = {(atk->location.x + def->location.x) / 2.0f,
                (atk->location.y + def->location.y) / 2.0f};

    atk->fighting = true;
    def->fighting = true;

    reloadImage(gs);
}

void Battle::reloadImage(GameState& gs) {
    if (!attacker || !defender) return;

    auto& engine = Engine::instance();


    int size = std::max(16, static_cast<int>(engine.uiScale * 0.65f));

    SDL_Surface* surf = SDL_CreateRGBSurfaceWithFormat(0, size, size, 32, SDL_PIXELFORMAT_RGBA32);
    if (!surf) return;
    SDL_FillRect(surf, nullptr, SDL_MapRGBA(surf->format, 255, 0, 255, 255));


    Color c = {50, 50, 50};
    if (gs.controlledCountry == defender->country) {
        c = progress ? Color{215, 0, 0} : Color{0, 215, 0};
    } else if (gs.controlledCountry == attacker->country) {
        c = progress ? Color{0, 215, 0} : Color{215, 0, 0};
    }


    int cx = size / 2, cy = size / 2, r = size / 2;
    for (int y = 0; y < size; y++) {
        for (int x = 0; x < size; x++) {
            int dx = x - cx, dy = y - cy;
            float dist = std::sqrt(static_cast<float>(dx * dx + dy * dy));
            if (dist < r - 2) {
                setPixel(surf, x, y, c);
            } else if (dist < r) {
                setPixel(surf, x, y, {0, 0, 0});
            }
        }
    }

    if (image) SDL_DestroyTexture(image);
    SDL_SetColorKey(surf, SDL_TRUE, SDL_MapRGB(surf->format, 255, 0, 255));
    image = SDL_CreateTextureFromSurface(engine.renderer, surf);
    imageW = size;
    imageH = size;
    SDL_FreeSurface(surf);
}

int Battle::countFronts(GameState& gs) const {
    int fronts = 0;
    for (auto& b : gs.battles) {
        if (b && !b->finished && b->defender && defender && b->defender == defender && b.get() != this) fronts++;
    }
    return fronts;
}

void Battle::update(GameState& gs) {
    if (finished) return;
    if (!attacker || !defender) {
        finished = true;
        return;
    }
    auto* aCountry = gs.getCountry(attacker->country);
    auto* dCountry = gs.getCountry(defender->country);
    if (!aCountry || !dCountry) { finished = true; return; }


    float aAtk = attacker->combatStats.attack * aCountry->attackMultiplier;
    float dDef = defender->combatStats.defense * dCountry->defenseMultiplier;


    float atkBiome = attackerBiome[1];
    float defBiome = defenderBiome[2];


    int extraFronts = countFronts(gs);
    float frontPenalty = std::max(0.25f, 1.0f - extraFronts * MULTI_FRONT_PENALTY);


    float pierceMult = (attacker->combatStats.piercing >= defender->combatStats.armor) ? 1.0f : 0.5f;
    float defPierceMult = (defender->combatStats.piercing >= attacker->combatStats.armor) ? 1.0f : 0.5f;

    float tick = gs.speed / 25.0f;


    float atkDamage = attacker->units * aAtk * atkBiome * pierceMult * COMBAT_SCALE * tick;
    float defDamage = defender->units * dDef * defBiome * defPierceMult * frontPenalty * COMBAT_SCALE * tick;


    attacker->units -= defDamage;
    defender->units -= atkDamage;


    attacker->resources -= defDamage * ORG_DRAIN_RATE / std::max(1.0f, attacker->maxResources) * 10.0f;
    defender->resources -= atkDamage * ORG_DRAIN_RATE / std::max(1.0f, defender->maxResources) * 10.0f;

    progress = attacker->resources > defender->resources ? 1.0f : 0.0f;

    if (gs.controlledCountry == attacker->country || gs.controlledCountry == defender->country) {
        reloadImage(gs);
    }


    bool battleOver = false;
    if (defender->resources <= 0) { attackerWin(gs); battleOver = true; }
    else if (attacker->resources <= 0) { defenderWin(gs); battleOver = true; }
    else if (attacker->units <= attacker->maxUnits * 0.05f) { defenderWin(gs); battleOver = true; }
    else if (defender->units <= defender->maxUnits * 0.05f) { attackerWin(gs); battleOver = true; }

    if (battleOver) {

        Division* a = attacker;
        Division* d = defender;


        finished = true;
        attacker = nullptr;
        defender = nullptr;

        if (!a || !d) return;


        if (randFloat(0, 6) < 1.0f) {
            auto* defCountry = gs.getCountry(d->country);
            if (defCountry) defCountry->destroy(gs, d->region);
        }

        a->fighting = false;
        d->fighting = false;
        a->locked = false;
        d->locked = false;
        a->recovering = true;
        d->recovering = true;
        a->commands.clear();
        d->commands.clear();


        a->reloadSprite(gs, a->color);
        d->reloadSprite(gs, d->color);


        if (a->units <= 0) a->kill(gs);
        if (d->units <= 0) d->kill(gs);
    }
}

void Battle::attackerWin(GameState& gs) {
    auto& rd = RegionData::instance();
    int oldRegion = defender->region;


    auto* dCountry = gs.getCountry(defender->country);
    int toRetreat = -1;
    int maxConns = 0;

    if (dCountry) {
        auto access = dCountry->getAccess(gs);
        std::set<int> accessSet(access.begin(), access.end());

        for (int conn : rd.getConnections(defender->region)) {
            if (accessSet.count(conn)) {
                int conns = 0;
                for (int c2 : rd.getConnections(conn)) {
                    if (accessSet.count(c2)) conns++;
                }
                if (conns > maxConns) { toRetreat = conn; maxConns = conns; }
            }
        }
    }

    if (toRetreat >= 0) {
        defender->move(toRetreat, gs);
        defender->movement = defender->movementSpeed;
    } else {

        defender->units = 0;
    }


    bool divisionInRegion = false;
    auto* aCountry = gs.getCountry(attacker->country);
    if (aCountry && !aCountry->atWarWith.empty()) {
        for (auto& enemyName : aCountry->atWarWith) {
            auto* ec = gs.getCountry(enemyName);
            if (!ec) continue;
            for (auto& div : ec->divisions) {
                if (div->region == oldRegion) {
                    divisionInRegion = true;
                    break;
                }
            }
            if (divisionInRegion) break;
        }
    }

    if (!divisionInRegion) {
        attacker->move(oldRegion, gs);
    }
}

void Battle::defenderWin(GameState& gs) {

}

void Battle::draw(SDL_Renderer* r, float camx, float camy, float zoom, int W, int H) {
    if (!image || finished) return;
    float sx = (location.x + camx - 0.5f) * zoom + W / 2.0f;
    float sy = (location.y + camy - 0.5f) * zoom + H / 2.0f;
    SDL_Rect dst = {static_cast<int>(sx - imageW / 2), static_cast<int>(sy - imageH / 2), imageW, imageH};
    SDL_RenderCopy(r, image, nullptr, &dst);
}
