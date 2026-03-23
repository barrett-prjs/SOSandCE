#include "game/division.h"
#include "game/game_state.h"
#include "game/country.h"
#include "game/faction.h"
#include "game/helpers.h"
#include "game/pathfinding.h"
#include "data/region_data.h"
#include "data/country_data.h"
#include "core/engine.h"

static bool hasFriendlyLandAccess(const Country* countryObj, const std::string& owner, const GameState& gs) {
    if (!countryObj) return false;
    if (owner.empty()) return false;
    return countryObj->hasLandAccessTo(owner, gs);
}

static bool regionIsUnderAttack(const Division* div, const GameState& gs) {
    if (!div) return false;
    for (const auto& battle : gs.battles) {
        if (!battle || battle->finished || !battle->attacker || !battle->defender) continue;
        if (battle->defender->region == div->region && battle->defender->country == div->country &&
            battle->attacker->country != div->country) {
            return true;
        }
    }
    return false;
}

static bool canTraverseStep(const Division* div, int nextRegion, const GameState& gs) {
    if (!div) return false;
    auto* myCountry = gs.getCountry(div->country);
    if (!myCountry) return false;

    auto& rd = RegionData::instance();
    std::string currentOwner = rd.getOwner(div->region);
    std::string nextOwner = rd.getOwner(nextRegion);

    if (hasFriendlyLandAccess(myCountry, nextOwner, gs)) {
        return true;
    }

    if (!nextOwner.empty() && !div->commandIgnoreEnemy &&
        std::find(myCountry->atWarWith.begin(), myCountry->atWarWith.end(), nextOwner) != myCountry->atWarWith.end()) {
        return true;
    }

    if (div->commandIgnoreWater) {
        return false;
    }

    bool currentIsPort = std::find(gs.ports.begin(), gs.ports.end(), div->region) != gs.ports.end();
    bool currentIsCanal = std::find(gs.canals.begin(), gs.canals.end(), div->region) != gs.canals.end();
    bool nextIsCanal = std::find(gs.canals.begin(), gs.canals.end(), nextRegion) != gs.canals.end();
    bool nextIsWater = nextOwner.empty() && rd.getPopulation(nextRegion) == 0;
    bool currentIsWater = currentOwner.empty() && rd.getPopulation(div->region) == 0;

    if (currentIsPort && nextIsWater) return true;
    if (currentIsWater && nextIsWater) return true;
    if (currentIsWater && nextIsCanal) return true;
    if (currentIsCanal && nextIsCanal) return true;
    if (currentIsCanal && nextIsWater) return true;
    return false;
}

Division::Division(const std::string& country, int divisions, int region, Color color, GameState& gs)
    : country(country), region(region), color(color), currentColor(color),
      divisionStack(divisions), gs_(gs)
{
    maxUnits = 10000.0f * divisionStack;
    units = maxUnits;
    maxResources = 100.0f * divisionStack;
    resources = maxResources;
    lastCountry = country;

    auto& rd = RegionData::instance();
    location = rd.getLocation(region);
    xBlit = location.x;
    yBlit = location.y;


    auto* c = gs.getCountry(country);
    int armsCount = 0;
    if (c) armsCount = c->buildingManager.getBuildingCount("arms_factory");
    combatStats = CombatStats(divisions, armsCount);
    attack = combatStats.attack;
    defense = combatStats.defense;
    movementSpeed = combatStats.speed;
    movement = movementSpeed;


    gs.divisionsByRegion[region].push_back(this);

    reloadSprite(gs);
}

Division::~Division() {

    auto it = gs_.divisionsByRegion.find(region);
    if (it != gs_.divisionsByRegion.end()) {
        auto& vec = it->second;
        vec.erase(std::remove(vec.begin(), vec.end(), this), vec.end());
    }

    if (sprite) {
        SDL_DestroyTexture(sprite);
        sprite = nullptr;
    }
}

void Division::reloadSprite(GameState& gs, Color bgColor) {
    if (bgColor.a == 0) {
        auto* c = gs.getCountry(country);
        bgColor = c ? c->divisionColor : Color{0, 0, 0};
    }

    auto& engine = Engine::instance();
    float ui = engine.uiScale;


    auto* c = gs.getCountry(country);
    Color cc = c ? c->color : Color{128, 128, 128};


    std::string lowerCountry = country;
    std::transform(lowerCountry.begin(), lowerCountry.end(), lowerCountry.begin(), ::tolower);
    std::string flagPath = engine.assetsPath + "flags/" + lowerCountry + "_flag.png";
    SDL_Surface* flagSurf = IMG_Load(flagPath.c_str());

    int flagW, flagH;
    if (flagSurf) {

        float targetH = ui * 0.35f;
        float scale = targetH / flagSurf->h;
        flagW = static_cast<int>(flagSurf->w * scale);
        flagH = static_cast<int>(targetH);
    } else {
        flagW = static_cast<int>(ui * 0.55f);
        flagH = static_cast<int>(ui * 0.35f);
    }

    int stackTextW = static_cast<int>(ui * 0.35f);
    int border = std::max(1, (int)(flagH * 0.06f));
    int barH = std::max(1, (int)(ui * 0.04f));
    int w = flagW + stackTextW + border * 2;
    int h = flagH + barH * 2 + border * 2;

    SDL_Surface* surf = SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_RGBA32);
    if (!surf) { if (flagSurf) SDL_FreeSurface(flagSurf); return; }


    SDL_FillRect(surf, nullptr, SDL_MapRGBA(surf->format, bgColor.r, bgColor.g, bgColor.b, 255));


    SDL_Rect inner = {border, border, w - border * 2, flagH};
    SDL_FillRect(surf, &inner, SDL_MapRGBA(surf->format, 0, 0, 0, 255));


    if (flagSurf) {
        SDL_Rect flagDst = {border, border, flagW, flagH};
        SDL_Surface* scaledFlag = SDL_CreateRGBSurfaceWithFormat(0, flagW, flagH, 32, SDL_PIXELFORMAT_RGBA32);
        if (scaledFlag) {
            SDL_BlitScaled(flagSurf, nullptr, scaledFlag, nullptr);
            SDL_BlitSurface(scaledFlag, nullptr, surf, &flagDst);
            SDL_FreeSurface(scaledFlag);
        }
        SDL_FreeSurface(flagSurf);
    } else {

        SDL_Rect flagRect = {border, border, flagW, flagH};
        SDL_FillRect(surf, &flagRect, SDL_MapRGBA(surf->format, cc.r, cc.g, cc.b, 255));
    }


    float healthPct = std::clamp(units / std::max(1.0f, maxUnits), 0.0f, 1.0f);
    int healthW = static_cast<int>((w - border * 2) * healthPct);
    SDL_Rect healthBarR = {border, flagH + border, healthW, barH};
    SDL_FillRect(surf, &healthBarR, SDL_MapRGBA(surf->format, 34, 177, 76, 255));


    float resPct = std::clamp(resources / std::max(1.0f, maxResources), 0.0f, 1.0f);
    int resW = static_cast<int>((w - border * 2) * resPct);
    SDL_Rect resBarR = {border, flagH + border + barH, resW, barH};
    SDL_FillRect(surf, &resBarR, SDL_MapRGBA(surf->format, 185, 122, 87, 255));


    if (sprite) SDL_DestroyTexture(sprite);
    sprite = SDL_CreateTextureFromSurface(engine.renderer, surf);
    spriteW = w;
    spriteH = h;
    SDL_FreeSurface(surf);

    currentColor = bgColor;
}

void Division::update(GameState& gs) {
    if (units <= 0) {
        kill(gs);
        return;
    }

    auto& rd = RegionData::instance();
    if (!commands.empty()) {
        int cmd = commands[0];


        auto* myCountry = gs.getCountry(country);
        bool divInRegion = false;
        bool holdPosition = regionIsUnderAttack(this, gs);

        if (trackedCommandRegion != cmd) {
            trackedCommandRegion = cmd;
            trackedCommandOwner = rd.getOwner(cmd);
        }

        std::string liveTargetOwner = rd.getOwner(cmd);
        bool targetOwnerChanged = trackedCommandRegion == cmd && trackedCommandOwner != liveTargetOwner;
        bool targetStillAccessible = false;
        if (!liveTargetOwner.empty()) {
            targetStillAccessible = hasFriendlyLandAccess(myCountry, liveTargetOwner, gs);
        } else {
            targetStillAccessible = !commandIgnoreWater && rd.getPopulation(cmd) == 0;
        }
        if (targetOwnerChanged && !targetStillAccessible) {
            commands.clear();
            aiObjectiveRegion = -1;
            movement = movementSpeed;
            trackedCommandRegion = -1;
            trackedCommandOwner.clear();
        } else if (targetOwnerChanged) {
            trackedCommandOwner = liveTargetOwner;
        }

        if (!commands.empty() && myCountry && !myCountry->atWarWith.empty() && !rd.getOwner(cmd).empty()) {
            std::set<std::string> atWarSet(myCountry->atWarWith.begin(), myCountry->atWarWith.end());
            auto it = gs.divisionsByRegion.find(cmd);
            if (it != gs.divisionsByRegion.end()) {
                for (auto* enemyDiv : it->second) {
                    if (!enemyDiv) continue;
                    if (atWarSet.count(enemyDiv->country)) {
                        divInRegion = true;
                        if (!enemyDiv->fighting && !fighting && resources >= maxResources * 0.5f) {
                            gs.battles.push_back(std::make_unique<Battle>(this, enemyDiv, gs));
                            commands.clear();
                            aiObjectiveRegion = -1;
                            trackedCommandRegion = -1;
                            trackedCommandOwner.clear();
                        }
                        break;
                    }
                }
            }
        }


        if (!commands.empty() && !divInRegion && !holdPosition) {
            float infraBonus = 1.0f;
            if (myCountry) {
                auto rEffects = myCountry->buildingManager.getRegionEffects(region);
                auto it = rEffects.find("movement_speed");
                if (it != rEffects.end()) infraBonus += it->second;
            }

            std::string cmdOwner = rd.getOwner(commands[0]);
            bool hasAccess = hasFriendlyLandAccess(myCountry, cmdOwner, gs);

            bool isWater = cmdOwner.empty() && rd.getPopulation(commands[0]) == 0;

            if (hasAccess || isWater) {
                movement -= gs.speed / 2.0f / 5.0f * (myCountry ? myCountry->transportMultiplier : 1.0f) * infraBonus;
            } else {
                movement -= gs.speed / 10.0f / 5.0f * (myCountry ? myCountry->transportMultiplier : 1.0f) * infraBonus;
            }
            movement = std::max(0.0f, movement);

            if (movement <= 0 && !commands.empty()) {
                int dest = commands[0];
                std::string destOwner = rd.getOwner(dest);
                bool provinceFlipped = trackedCommandRegion == dest && trackedCommandOwner != destOwner;
                bool nextStepLost = provinceFlipped && !destOwner.empty() && !hasFriendlyLandAccess(myCountry, destOwner, gs);
                if (nextStepLost) {
                    commands.clear();
                    aiObjectiveRegion = -1;
                    movement = movementSpeed;
                    trackedCommandRegion = -1;
                    trackedCommandOwner.clear();
                } else {
                    movement = movementSpeed;
                    commands.erase(commands.begin());
                    move(dest, gs);
                    if (commands.empty()) {
                        aiObjectiveRegion = -1;
                        trackedCommandRegion = -1;
                        trackedCommandOwner.clear();
                    } else {
                        trackedCommandRegion = commands[0];
                        trackedCommandOwner = rd.getOwner(commands[0]);
                    }
                }
            }
        } else if (holdPosition) {
            movement = movementSpeed;
        }
    }


    auto* myCountry = gs.getCountry(country);
    if (resources < maxResources && !fighting && myCountry &&
        myCountry->money > gs.speed / 5.0f * divisionStack / 2.0f * resourceUse) {
        resources += gs.speed / 5.0f * divisionStack / 2.0f;
        myCountry->money -= gs.speed / 5.0f * divisionStack / 2.0f * resourceUse * 2.5f;
        resources = std::clamp(resources, maxResources / 2.0f, maxResources);
    }

    updateLocation(gs);
}

void Division::updateLocation(GameState& gs) {
    float renderX = location.x;
    float renderY = location.y;

    if (!commands.empty() && movementSpeed > 0.0f && !fighting) {
        auto& rd = RegionData::instance();
        Vec2 target = rd.getLocation(commands[0]);
        float progress = std::clamp((movementSpeed - movement) / movementSpeed, 0.0f, 1.0f);
        float mapW = gs.regionsMapSurf ? static_cast<float>(gs.regionsMapSurf->w) : 1275.0f;

        float dx = target.x - location.x;
        if (mapW > 0.0f && std::fabs(dx) > mapW * 0.5f) {
            dx += (dx > 0.0f) ? -mapW : mapW;
        }

        renderX = location.x + dx * progress;
        renderY = location.y + (target.y - location.y) * progress;

        if (mapW > 0.0f) {
            while (renderX < 0.0f) renderX += mapW;
            while (renderX >= mapW) renderX -= mapW;
        }
    }

    xBlit = renderX;
    yBlit = renderY;
}

void Division::move(int newRegion, GameState& gs) {
    auto& rd = RegionData::instance();

    lastCountry = rd.getOwner(region);
    std::string oldOwner = rd.getOwner(newRegion);


    auto it = gs.divisionsByRegion.find(region);
    if (it != gs.divisionsByRegion.end()) {
        auto& vec = it->second;
        vec.erase(std::remove(vec.begin(), vec.end(), this), vec.end());
    }


    region = newRegion;
    gs.divisionsByRegion[region].push_back(this);
    location = rd.getLocation(region);


    auto* myCountry = gs.getCountry(country);
    if (myCountry && !oldOwner.empty() &&
        std::find(myCountry->atWarWith.begin(), myCountry->atWarWith.end(), oldOwner) !=
        myCountry->atWarWith.end()) {

        auto* oc = gs.getCountry(oldOwner);
        int prevRegion = region;

        if (myCountry->faction.empty()) {

            myCountry->addRegion(region, gs, false, prevRegion);
            if (oc) oc->lastAttackedBy = country;
        } else {

            auto* fac = gs.getFaction(myCountry->faction);
            bool given = false;
            if (fac) {
                for (size_t mi = 0; mi < fac->members.size(); mi++) {
                    std::string memberName = fac->members[mi];
                    auto* mc = gs.getCountry(memberName);
                    if (mc && std::find(mc->regionsBeforeWar.begin(), mc->regionsBeforeWar.end(), region)
                              != mc->regionsBeforeWar.end()) {
                        mc->addRegion(region, gs, false, prevRegion);
                        if (oc) oc->lastAttackedBy = memberName;
                        given = true;
                        break;
                    }
                }
            }

            if (!given && !lastCountry.empty()) {
                auto* lc = gs.getCountry(lastCountry);
                if (lc && std::find(lc->atWarWith.begin(), lc->atWarWith.end(), oldOwner) != lc->atWarWith.end()) {
                    lc->addRegion(region, gs, false, prevRegion);
                    if (oc) oc->lastAttackedBy = lastCountry;
                    given = true;
                }
            }

            if (!given) {
                myCountry->addRegion(region, gs, false, prevRegion);
                if (oc) oc->lastAttackedBy = country;
            }
        }
    }
}

void Division::command(int targetRegion, GameState& gs, bool ignoreEnemy, bool ignoreWater, int iterations) {
    if (targetRegion == region) return;

    auto* myCountry = gs.getCountry(country);
    if (!myCountry) return;

    std::set<std::string> access;
    for (const auto& countryName : gs.countryList) {
        if (myCountry->hasLandAccessTo(countryName, gs)) {
            access.insert(countryName);
        }
    }
    std::set<std::string> wars(myCountry->atWarWith.begin(), myCountry->atWarWith.end());

    commands = pathfind(region, targetRegion, ignoreEnemy, ignoreWater, access, wars, iterations, gs);
    commandIgnoreEnemy = ignoreEnemy;
    commandIgnoreWater = ignoreWater;
    if (!commands.empty()) {
        attempts = 0;
        trackedCommandRegion = commands[0];
        trackedCommandOwner = RegionData::instance().getOwner(commands[0]);
    } else {
        trackedCommandRegion = -1;
        trackedCommandOwner.clear();
    }
}

void Division::kill(GameState& gs) {
    for (auto& battle : gs.battles) {
        if (!battle) continue;

        bool involvesThisDivision = (battle->attacker == this || battle->defender == this);
        if (!involvesThisDivision) continue;

        Division* other = (battle->attacker == this) ? battle->defender : battle->attacker;
        if (other) {
            other->fighting = false;
            other->locked = false;
            other->recovering = true;
        }

        battle->finished = true;
        if (battle->attacker == this) battle->attacker = nullptr;
        if (battle->defender == this) battle->defender = nullptr;
    }


    auto it = gs.divisionsByRegion.find(region);
    if (it != gs.divisionsByRegion.end()) {
        auto& vec = it->second;
        vec.erase(std::remove(vec.begin(), vec.end(), this), vec.end());
    }


    auto* c = gs.getCountry(country);
    if (c) {
        auto dit = std::find_if(c->divisions.begin(), c->divisions.end(),
            [this](auto& d) { return d.get() == this; });
        if (dit != c->divisions.end()) c->divisions.erase(dit);
    }
}
