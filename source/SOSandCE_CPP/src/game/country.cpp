#include "game/country.h"
#include "game/game_state.h"
#include "game/division.h"
#include "game/faction.h"
#include "game/focus_tree.h"
#include "game/helpers.h"
#include "game/pathfinding.h"
#include "data/region_data.h"
#include "data/country_data.h"
#include "map/map_functions.h"
#include "core/audio.h"
#include "core/engine.h"
#include "ui/toast.h"

namespace {

std::array<float, RESOURCE_COUNT> trainingResourceCost(int divCount) {
    std::array<float, RESOURCE_COUNT> cost{};
    cost.fill(0.0f);
    cost[static_cast<int>(Resource::Steel)] = 36.0f * divCount;
    cost[static_cast<int>(Resource::Oil)] = 10.0f * divCount;
    cost[static_cast<int>(Resource::Rubber)] = 8.0f * divCount;
    cost[static_cast<int>(Resource::Aluminum)] = 6.0f * divCount;
    return cost;
}

std::array<float, RESOURCE_COUNT> deploymentResourceCost(int divCount) {
    std::array<float, RESOURCE_COUNT> cost{};
    cost.fill(0.0f);
    cost[static_cast<int>(Resource::Steel)] = 10.0f * divCount;
    cost[static_cast<int>(Resource::Oil)] = 4.0f * divCount;
    return cost;
}

bool hasResources(const ResourceManager& manager, const std::array<float, RESOURCE_COUNT>& cost) {
    for (int i = 0; i < RESOURCE_COUNT; ++i) {
        if (manager.stockpile[i] + 0.001f < cost[i]) {
            return false;
        }
    }
    return true;
}

void spendResources(ResourceManager& manager, const std::array<float, RESOURCE_COUNT>& cost) {
    for (int i = 0; i < RESOURCE_COUNT; ++i) {
        manager.stockpile[i] = std::max(0.0f, manager.stockpile[i] - cost[i]);
    }
}

}

Country::Country(const std::string& name, const std::vector<int>& startRegions, GameState& gs)
    : name(name), gs_(gs)
{
    auto& cd = CountryData::instance();
    color = cd.getColor(name);
    culture = cd.getCulture(name);
    coreRegions = cd.getClaims(name);
    ideology = cd.getIdeologyName(name);
    ideologyName = Helpers::getIdeologyName(ideology[0], ideology[1]);
    lastIdeology = ideologyName;
    baseStability = static_cast<float>(cd.getBaseStability(name));
    stability = baseStability;
    lastStability = baseStability;


    leader = generateLeader(culture, ideologyName);
    cabinet = generateCabinet(culture);
    electionSystem = ElectionSystem(ideologyName);
    combatStats = CombatStats(1, 0);


    FocusTreeLoader loader;
    auto treeNodes = loader.loadForCountry(name, gs);
    focusTreeEngine.loadTree(treeNodes);
    if (!treeNodes.empty()) {
        printf("[Country] %s: loaded %d focus nodes\n", name.c_str(), (int)treeNodes.size());
        fflush(stdout);
    }


    for (auto& node : treeNodes) {
        std::vector<std::variant<int, float, std::string, bool, std::vector<std::string>>> nodeData;
        nodeData.push_back(node.days);
        nodeData.push_back(node.description);
        nodeData.push_back(node.prerequisites);
        decisionTree[node.name] = nodeData;
    }


    for (int r : startRegions) {
        addRegion(r, gs, true, -1, false);
    }

    militaryAccess.push_back(name);
}

Country::~Country() = default;

void Country::update(GameState& gs) {


    constexpr float kLegacyContinuousRatePerHour = 1.0f / 6.0f;


    if (resetRegionsBeforeWar && atWarWith.empty()) {
        regionsBeforeWar = regions;
        resetRegionsBeforeWar = false;
    }


    ideologyName = Helpers::getIdeologyName(ideology[0], ideology[1]);
    if (lastIdeology != ideologyName) {

    }
    lastIdeology = ideologyName;

    auto& rd = RegionData::instance();


    if (lastDayTrained != gs.time.totalDays) {

        for (auto& cycle : training) {
            if (cycle[1] > 0) cycle[1]--;
        }


        if (focus.has_value()) {
            auto& [fName, fDays, fEffects] = *focus;
            fDays--;
            if (fDays <= 0) {
                std::string focusName = fName;
                focus.reset();
                focusTreeEngine.completedFocuses.insert(focusName);
                focusTreeEngine.completeFocus(focusName, gs, name);
                if (name == gs.controlledCountry) {
                    toasts().show("Focus completed: " + focusName, 3000);
                }
            }
        }


        auto justBuilt = buildingManager.tick(resourceManager.getProductionPenalty(), this);


        if (!justBuilt.empty()) {
            factories = buildingManager.countAll(BuildingType::CivilianFactory) +
                        buildingManager.countAll(BuildingType::MilitaryFactory);

            for (auto& jb : justBuilt) {

                if (jb.type == BuildingType::Dockyard || jb.type == BuildingType::Port) {
                    if (std::find(gs.ports.begin(), gs.ports.end(), jb.regionId) == gs.ports.end()) {
                        gs.ports.push_back(jb.regionId);
                    }
                }


                if (gs.industryMapSurf) {
                    Vec2 loc = rd.getLocation(jb.regionId);
                    if (gs.regionsMapSurf) {
                        MapFunc::fillRegionMask(gs.industryMapSurf, gs.regionsMapSurf, jb.regionId, loc.x, loc.y, buildingMapColor(jb.type));
                    } else {
                        MapFunc::fill(gs.industryMapSurf, loc.x, loc.y, buildingMapColor(jb.type));
                    }
                    gs.mapDirty = true;
                }
            }
        }

        lastDayTrained = gs.time.totalDays;
    }


    totalMilitary = 0;
    for (auto& div : divisions) {
        totalMilitary += div->units;
    }

    if (totalMilitary < usedManPower) {
        usedManPower -= static_cast<float>(regions.size()) * gs.speed / 10.0f;
    }
    usedManPower = std::max(0.0f, usedManPower);


    manPower = std::max(0.0f,
        (std::pow(1.00249688279f, static_cast<float>(militarySize)) - 1.0f) * population - usedManPower);


    stability = baseStability - std::log(static_cast<float>(atWarWith.size()) + 1.0f) * 3.0f;
    if (!faction.empty()) stability += 5;
    stability = std::clamp(stability, 0.0f, 100.0f);
    baseStability = std::clamp(baseStability, 0.0f, 100.0f);


    if (lastStability > stability && stability < 50.0f && name == gs.controlledCountry) {
        float chance = 100.0f - stability * 2.0f;
        float roll = randFloat(0, 100);
        if (chance > roll) {
            auto& cd = CountryData::instance();
            auto possibleEnemies = cd.getAllCountries(culture);
            std::string enemy;
            for (auto& pe : possibleEnemies) {
                if (!gs.getCountry(pe)) { enemy = pe; break; }
            }
            if (enemy.empty()) enemy = "Anarchist_State";
            civilWar(enemy, gs);
        }
    }
    lastStability = stability;


    politicalPower += 0.25f * kLegacyContinuousRatePerHour * politicalMultiplier;


    int civCount = buildingManager.getBuildingCount("civilian_factory");
    float baseIncome = 5000.0f * (civCount + 1);
    money += baseIncome * kLegacyContinuousRatePerHour * moneyMultiplier;


    float totalStacks = 0;
    for (auto& d : divisions) totalStacks += d->divisionStack;
    float divUpkeep = totalStacks * gs.DIVISION_UPKEEP_PER_DAY * kLegacyContinuousRatePerHour;
    money -= divUpkeep;
    money = std::clamp(money, 0.0f, static_cast<float>(regions.size()) * 2500000.0f);


    ideology[0] = std::clamp(ideology[0], -1.0f, 1.0f);
    ideology[1] = std::clamp(ideology[1], -1.0f, 1.0f);


    if (gs.speed > 0 && !regions.empty()) {
        resourceManager.update(gs, name);
    }


    float totalStacks2 = 0;
    for (auto& d : divisions) totalStacks2 += d->divisionStack;
    int armsCount = buildingManager.getBuildingCount("arms_factory");
    int totalStacksInt = static_cast<int>(totalStacks2);
    combatStats.recalculate(totalStacksInt, armsCount);


    for (auto& div : divisions) {
        if (!div) continue;
        div->combatStats.recalculate(div->divisionStack, armsCount);
        div->attack = div->combatStats.attack;
        div->defense = div->combatStats.defense;
        div->movementSpeed = div->combatStats.speed;
    }


    moneyMultiplier = 1.0f;
    attackMultiplier = 1.0f;
    politicalMultiplier = 1.0f;


    if (leader.isAlive) {
        moneyMultiplier += leader.economyBonus;
        attackMultiplier += leader.militaryBonus;
    }


    for (auto& m : cabinet.members) {
        if (m.role == "economy") moneyMultiplier += m.skill;
        else if (m.role == "military") attackMultiplier += m.skill;
        else if (m.role == "diplomacy") politicalMultiplier += m.skill;
    }


    moneyMultiplier = std::max(1.0f, moneyMultiplier);
    attackMultiplier = std::max(1.0f, attackMultiplier);
    politicalMultiplier = std::max(1.0f, politicalMultiplier);

    for (auto& div : divisions) {
        if (!div) continue;
        div->attack = div->combatStats.attack * attackMultiplier;
        div->defense = div->combatStats.defense * attackMultiplier;
        div->movementSpeed = div->combatStats.speed;
        div->movement = std::min(div->movement, div->movementSpeed);
    }


    electionSystem.update(gs, name);


    if (!capitulated && !atWarWith.empty() &&
        (regions.empty() || divisions.empty() || cities.empty()) &&
        regions.size() < regionsBeforeWar.size()) {
        capitulated = true;

        for (size_t i = 0; i < divisions.size(); ) {
            Division* div = divisions[i].get();
            if (div && std::find(regions.begin(), regions.end(), div->region) != regions.end()) {
                div->kill(gs);
                continue;
            }
            i++;
        }


        if (!atWarWith.empty()) {
            std::string annexer = lastAttackedBy.empty() ? atWarWith[0] : lastAttackedBy;
            auto* annexerCountry = gs.getCountry(annexer);
            if (annexerCountry) {
                annexerCountry->annexCountry(culture, gs, name);
            }
        }


        if (!gs.controlledCountry.empty()) {
            toasts().show(replaceAll(name, "_", " ") + " has capitulated!", 3000);
        }

        if (atWarWith.empty()) {
            kill(gs);
        }
    }


    if (!atWarWith.empty()) {
        bool allCapitulated = true;
        for (auto& enemy : atWarWith) {
            auto* c = gs.getCountry(enemy);
            if (c && !c->capitulated) { allCapitulated = false; break; }
        }
        if (allCapitulated) {
            gs.peaceTreaty(name);
        }
    }


    if (gs.controlledCountry != name && gs.speed > 0 && !gs.aiControllers.count(name)) {
        runAI(gs);
    }


    if (checkBattleBorder && !atWarWith.empty()) {
        auto oldBorder = battleBorder;
        battleBorder = getBattleBorder(gs);
        if (std::set<int>(oldBorder.begin(), oldBorder.end()) !=
            std::set<int>(battleBorder.begin(), battleBorder.end())) {
            for (auto& div : divisions) {
                div->locked = false;
                div->navalLocked = false;
            }
        }
        checkBattleBorder = false;
    }
    if (checkBordering) {
        auto oldBordering = bordering;
        bordering = getBorderCountries(gs);
        if (std::set<std::string>(oldBordering.begin(), oldBordering.end()) !=
            std::set<std::string>(bordering.begin(), bordering.end())) {
            for (auto& div : divisions) {
                div->locked = false;
                div->navalLocked = false;
            }
        }
        checkBordering = false;
    }


    for (size_t di = 0; di < divisions.size(); ) {
        if (divisions[di]) {
            size_t prevSize = divisions.size();
            divisions[di]->update(gs);

            if (divisions.size() < prevSize) continue;
        }
        di++;
    }
}

void Country::addRegion(int id, GameState& gs, bool ignoreFill, int divRegion, bool ignoreResources) {
    checkBordering = true;
    auto& rd = RegionData::instance();
    auto& cd = CountryData::instance();

    if (std::find(regions.begin(), regions.end(), id) != regions.end()) return;

    regions.push_back(id);
    std::string oldOwner = rd.getOwner(id);

    if (!oldOwner.empty() && oldOwner != name) {
        auto* oc = gs.getCountry(oldOwner);
        if (oc) {
            oc->removeRegion(id);

            oc->population -= rd.getPopulation(id);
            if (oc->population < 0) oc->population = 0;
        }
    }

    rd.updateOwner(id, name);


    Vec2 loc = rd.getLocation(id);
    std::string regionCulture = culture;
    if (gs.cultureMapSurf) {
        auto& cd = CountryData::instance();
        int px = static_cast<int>(std::round(loc.x));
        int py = static_cast<int>(std::round(loc.y));
        if (px >= 0 && px < gs.cultureMapSurf->w && py >= 0 && py < gs.cultureMapSurf->h) {
            SDL_LockSurface(gs.cultureMapSurf);
            Uint32* pixels = static_cast<Uint32*>(gs.cultureMapSurf->pixels);
            Uint32 pixel = pixels[py * gs.cultureMapSurf->w + px];
            SDL_UnlockSurface(gs.cultureMapSurf);
            Uint8 cr, cg, cb;
            SDL_GetRGB(pixel, gs.cultureMapSurf->format, &cr, &cg, &cb);
            std::string cultureOwner = cd.colorToCountry({cr, cg, cb});
            if (!cultureOwner.empty()) {
                std::string lookupCulture = cd.getCulture(cultureOwner);
                if (!lookupCulture.empty()) regionCulture = lookupCulture;
            }
        }
    }
    cultures[regionCulture].push_back(id);


    if (!oldOwner.empty() && oldOwner != name) {
        auto* oc = gs.getCountry(oldOwner);
        if (oc) {
            auto cit = oc->cultures.find(regionCulture);
            if (cit != oc->cultures.end()) {
                auto rit = std::find(cit->second.begin(), cit->second.end(), id);
                if (rit != cit->second.end()) cit->second.erase(rit);
            }
        }
    }


    std::string city = rd.getCity(id);
    if (!city.empty()) {
        if (std::find(cities.begin(), cities.end(), city) == cities.end()) {
            cities.push_back(city);
        }
        if (!oldOwner.empty() && oldOwner != name) {
            auto* oc = gs.getCountry(oldOwner);
            if (oc) {
                auto cit = std::find(oc->cities.begin(), oc->cities.end(), city);
                if (cit != oc->cities.end()) oc->cities.erase(cit);
                if (oc->capital == city) {
                    oc->capital.clear();
                    if (!oc->cities.empty()) oc->capital = oc->cities[randInt(0, static_cast<int>(oc->cities.size()) - 1)];
                }
            }
        }
        std::string cityCulture = rd.getCityCulture(city);
        if (cityCulture == culture) {
            capital = city;
        } else if (capital.empty()) {
            capital = city;
        }
        if (deployRegion.empty() || std::find(cities.begin(), cities.end(), deployRegion) == cities.end()) {
            deployRegion = capital.empty() ? (cities.empty() ? "" : cities[0]) : capital;
        }
    }


    if (!ignoreResources) {
        population += std::abs(rd.getPopulation(id));


        auto& rawRes = rd.getResources(id);
        if (!rawRes.empty()) {
            std::unordered_map<Resource, float> resMap;
            for (auto& [rName, rAmount] : rawRes) {
                resMap[resourceFromString(rName)] = rAmount;
            }
            resourceManager.addRegionResources(id, resMap);
        }


        if (!oldOwner.empty() && oldOwner != name) {
            auto* oc = gs.getCountry(oldOwner);
            if (oc) oc->resourceManager.removeRegionResources(id);
        }
    }


    if (!ignoreFill) {
        Vec2 loc = rd.getLocation(id);
        float px = loc.x, py = loc.y;


        if (gs.politicalMapSurf) {
            if (gs.regionsMapSurf) {
                MapFunc::fillRegionMask(gs.politicalMapSurf, gs.regionsMapSurf, id, px, py, color);
            } else {
                MapFunc::fillFixBorder(gs.politicalMapSurf, px, py, color);
            }
        }

        if (gs.ideologyMapSurf) {
            Color ideColor = Helpers::getIdeologyColor(ideology[0], ideology[1]);
            if (gs.regionsMapSurf) {
                MapFunc::fillRegionMask(gs.ideologyMapSurf, gs.regionsMapSurf, id, px, py, ideColor);
            } else {
                MapFunc::fillWithBorder(gs.ideologyMapSurf, gs.politicalMapSurf, px, py, ideColor);
            }
        }

        if (gs.factionMapSurf) {
            if (gs.regionsMapSurf) {
                MapFunc::fillRegionMask(gs.factionMapSurf, gs.regionsMapSurf, id, px, py, factionColor);
            } else {
                MapFunc::fillWithBorder(gs.factionMapSurf, gs.politicalMapSurf, px, py, factionColor);
            }
        }

        gs.mapDirty = true;
    }
}

void Country::addRegions(const std::vector<int>& regionList, GameState& gs, bool ignoreFill, bool ignoreResources) {
    for (int r : regionList) {
        addRegion(r, gs, ignoreFill, -1, ignoreResources);
    }
}

void Country::removeRegion(int id) {
    auto it = std::find(regions.begin(), regions.end(), id);
    if (it != regions.end()) regions.erase(it);
}

void Country::spawnDivisions(GameState& gs) {
    while (militarySize < 4 && manPower < 10000) {
        militarySize++;
        manPower = (std::pow(1.00249688279f, static_cast<float>(militarySize)) - 1.0f) * population - usedManPower;
    }

    float totalUnits = (std::pow(1.00249688279f, static_cast<float>(militarySize)) - 1.0f) * population * 2.0f / 3.0f;
    int totalDivCount = static_cast<int>(std::sqrt(static_cast<float>(regions.size()) * 100.0f) / 10.0f);
    if (totalUnits < 10000) totalUnits = 10000;

    int maxDivs = std::min(
        static_cast<int>(std::floor(totalUnits / 10000.0f)),
        static_cast<int>(std::floor(manPower / 10000.0f))
    );

    std::vector<int> divSizes(std::max(1, maxDivs), 1);


    while (static_cast<int>(divSizes.size()) > totalDivCount && divSizes.size() > 1) {
        int idx = randInt(0, static_cast<int>(divSizes.size()) - 2);
        divSizes[idx] += divSizes[idx + 1];
        divSizes.erase(divSizes.begin() + idx + 1);
    }

    for (int ds : divSizes) {
        addDivision(gs, ds);
    }
}

void Country::addDivision(GameState& gs, int divCount, int region, bool ignoreManpower, bool ignoreTotalMilitary) {
    if (!ignoreManpower && manPower < divCount * 10000.0f) return;
    if (regions.empty()) return;

    if (region < 0) {
        region = regions[randInt(0, static_cast<int>(regions.size()) - 1)];
    }

    divisions.push_back(std::make_unique<Division>(name, divCount, region, divisionColor, gs));

    if (!ignoreManpower) {
        usedManPower += divCount * 10000.0f;
    }
}

void Country::trainDivision(GameState& gs, int divCount) {
    if (divCount <= 0) return;
    if (manPower < divCount * 10000.0f) return;
    if (!canAffordTrainingResources(divCount)) return;


    int armsCount = buildingManager.getBuildingCount("arms_factory");
    float scale = 1.0f + (float)divisions.size() * 0.05f;
    float discount = std::max(0.5f, 1.0f - armsCount * 0.05f);
    float trainCost = gs.TRAINING_COST_PER_DIV * divCount * scale * discount;
    if (money < trainCost) return;

    money -= trainCost;
    spendTrainingResources(divCount);
    int baseDays = 14;
    int reduced = std::max(3, static_cast<int>(baseDays * std::max(0.25f, 1.0f - armsCount * 0.15f)));
    training.push_back({divCount, reduced});
    usedManPower += divCount * 10000.0f;
    totalMilitary += divCount * 10000.0f;
}

bool Country::deployTrainingDivision(GameState& gs, int trainingIndex) {
    if (trainingIndex < 0 || trainingIndex >= static_cast<int>(training.size())) return false;
    int divCount = training[trainingIndex][0];
    if (training[trainingIndex][1] > 0) return false;
    if (!canAffordDeploymentResources(divCount)) return false;

    int deployReg = -1;
    if (!deployRegion.empty()) {
        deployReg = RegionData::instance().getCityRegion(deployRegion);
    }

    spendDeploymentResources(divCount);
    addDivision(gs, divCount, deployReg, true);
    training.erase(training.begin() + trainingIndex);
    return true;
}

std::array<float, RESOURCE_COUNT> Country::trainingCostBundle(int divCount) {
    return trainingResourceCost(divCount);
}

std::array<float, RESOURCE_COUNT> Country::deploymentCostBundle(int divCount) {
    return deploymentResourceCost(divCount);
}

void Country::divideDivision(GameState& gs, Division* div) {
    if (!div || div->fighting || div->divisionStack <= 1) return;
    int half = div->divisionStack / 2;
    int remainder = div->divisionStack % 2;

    addDivision(gs, half + remainder, div->region, true, true);
    addDivision(gs, half, div->region, true, true);


    if (divisions.size() >= 2) {
        auto& d1 = divisions[divisions.size() - 2];
        auto& d2 = divisions[divisions.size() - 1];
        d1->units = div->units * (half + remainder) / div->divisionStack;
        d2->units = div->units * half / div->divisionStack;
        d1->resources = div->resources / 2;
        d2->resources = div->resources / 2;
    }


    auto it = std::find_if(divisions.begin(), divisions.end(),
        [div](auto& d) { return d.get() == div; });
    if (it != divisions.end()) divisions.erase(it);
}

void Country::mergeDivisions(GameState& gs, std::vector<Division*> divs) {
    std::unordered_map<int, std::vector<Division*>> byRegion;
    for (auto* d : divs) {
        if (d && !d->fighting) byRegion[d->region].push_back(d);
    }

    for (auto& [reg, regionDivs] : byRegion) {
        if (regionDivs.size() < 2) continue;
        int totalStack = 0;
        float totalUnits = 0, totalRes = 0;
        for (auto* d : regionDivs) {
            totalStack += d->divisionStack;
            totalUnits += d->units;
            totalRes += d->resources;
        }

        addDivision(gs, totalStack, reg, true, true);
        auto& merged = divisions.back();
        merged->units = totalUnits;
        merged->resources = totalRes;


        for (auto* d : regionDivs) {
            auto it = std::find_if(divisions.begin(), divisions.end(),
                [d](auto& ptr) { return ptr.get() == d; });
            if (it != divisions.end()) divisions.erase(it);
        }
    }
}

void Country::resetDivColor(Color col) {
    divisionColor = col;
    for (auto& d : divisions) {
        d->color = col;
        d->reloadSprite(gs_);
    }
}

std::vector<std::string> Country::getBorderCountries(GameState& gs) const {
    auto& rd = RegionData::instance();
    std::set<std::string> bordered;
    for (int r : regions) {
        for (int conn : rd.getConnections(r)) {
            std::string owner = rd.getOwner(conn);
            if (!owner.empty() && owner != name) bordered.insert(owner);
        }
    }
    return {bordered.begin(), bordered.end()};
}

std::vector<int> Country::getBattleBorder(GameState& gs, bool ignoreAccess) const {
    auto& rd = RegionData::instance();
    std::set<std::string> warSet(atWarWith.begin(), atWarWith.end());
    std::set<int> borderSet;

    auto toCheck = ignoreAccess ? regions : getAccess(gs);
    for (int r : toCheck) {
        for (int conn : rd.getConnections(r)) {
            std::string owner = rd.getOwner(conn);
            if (warSet.count(owner)) {
                borderSet.insert(r);
                break;
            }
        }
    }
    return {borderSet.begin(), borderSet.end()};
}

bool Country::hasLandAccessTo(const std::string& otherCountry, const GameState& gs) const {
    if (otherCountry.empty()) return false;
    if (otherCountry == name) return true;

    if (std::find(militaryAccess.begin(), militaryAccess.end(), otherCountry) != militaryAccess.end()) {
        return true;
    }

    if (!faction.empty()) {
        const auto* fac = gs.getFaction(faction);
        if (fac && std::find(fac->members.begin(), fac->members.end(), otherCountry) != fac->members.end()) {
            return true;
        }
    }

    const auto* other = gs.getCountry(otherCountry);
    if (!other) return false;


    return puppetTo == otherCountry || other->puppetTo == name;
}

std::vector<int> Country::getAccess(GameState& gs) const {
    std::set<int> uniqueRegions;
    std::vector<int> total;
    for (const auto& countryName : gs.countryList) {
        if (!hasLandAccessTo(countryName, gs)) continue;
        auto* c = gs.getCountry(countryName);
        if (!c) continue;
        for (int rid : c->regions) {
            if (uniqueRegions.insert(rid).second) {
                total.push_back(rid);
            }
        }
    }
    return total;
}

void Country::declareWar(const std::string& enemy, GameState& gs, bool ignoreFaction, bool popup) {
    auto* ec = gs.getCountry(enemy);
    if (!ec || std::find(atWarWith.begin(), atWarWith.end(), enemy) != atWarWith.end()) return;

    checkBordering = true;
    ec->checkBordering = true;
    checkBattleBorder = true;
    ec->checkBattleBorder = true;
    resetRegionsBeforeWar = true;
    ec->resetRegionsBeforeWar = true;


    if (gs.controlledCountry == name && !faction.empty() && ec->faction == faction) {
        auto* fac = gs.getFaction(faction);
        if (fac) fac->removeCountry(name, gs);
    }


    auto it = std::find(militaryAccess.begin(), militaryAccess.end(), enemy);
    if (it != militaryAccess.end()) militaryAccess.erase(it);
    auto it2 = std::find(ec->militaryAccess.begin(), ec->militaryAccess.end(), name);
    if (it2 != ec->militaryAccess.end()) ec->militaryAccess.erase(it2);

    atWarWith.push_back(enemy);
    ec->atWarWith.push_back(name);


    if (gs.controlledCountry == name) {
        ec->resetDivColor({255, 0, 0});
    } else if (gs.controlledCountry == enemy) {
        resetDivColor({255, 0, 0});
    }


    if (!ignoreFaction && !faction.empty() && gs.controlledCountry == name && ec->faction != faction) {
        auto* myFac = gs.getFaction(faction);
        if (myFac && !ec->faction.empty()) {
            myFac->declareWar(ec->faction, gs);
        }
    }


    if (popup && (gs.controlledCountry == name || gs.controlledCountry == enemy)) {
        std::string msg = replaceAll(name, "_", " ") + " declared war on " + replaceAll(enemy, "_", " ") + "!";
        toasts().show(msg, 3000);


        auto& audio = Audio::instance();
        if (audio.currentMusic.find("warMusic") == std::string::npos) {
            std::string warPath = Engine::instance().assetsPath + "music/warMusic.mp3";
            if (fs::exists(warPath)) {
                audio.playMusic(warPath, true, 2000);
            }
        }
    }
}

void Country::makePeace(const std::string& enemy, GameState& gs, bool popup) {
    auto* ec = gs.getCountry(enemy);
    if (!ec) return;
    if (std::find(atWarWith.begin(), atWarWith.end(), enemy) == atWarWith.end()) return;


    if (gs.controlledCountry == name) {
        ec->resetDivColor({0, 0, 0});
    } else if (gs.controlledCountry == enemy) {
        resetDivColor({0, 0, 0});
    }

    auto it = std::find(atWarWith.begin(), atWarWith.end(), enemy);
    if (it != atWarWith.end()) atWarWith.erase(it);
    auto it2 = std::find(ec->atWarWith.begin(), ec->atWarWith.end(), name);
    if (it2 != ec->atWarWith.end()) ec->atWarWith.erase(it2);

    checkBordering = true;
    ec->checkBordering = true;
    checkBattleBorder = true;
    ec->checkBattleBorder = true;
    resetRegionsBeforeWar = true;
    ec->resetRegionsBeforeWar = true;


    if (popup && !gs.controlledCountry.empty() &&
        (gs.controlledCountry == name || gs.controlledCountry == enemy)) {
        std::string otherName = (gs.controlledCountry == name) ? enemy : name;
        toasts().show(replaceAll(otherName, "_", " ") + " has made peace.", 3000);
    }

    auto* player = gs.getCountry(gs.controlledCountry);
    if (player && player->atWarWith.empty()) {
        auto& audio = Audio::instance();
        if (audio.currentMusic.find("warMusic") != std::string::npos) {
            std::string gamePath = Engine::instance().assetsPath + "music/gameMusic.mp3";
            if (fs::exists(gamePath)) {
                audio.playMusic(gamePath, true, 2000);
            }
        }
    }
}

void Country::kill(GameState& gs, bool ignorePopup) {
    auto& rd = RegionData::instance();

    for (int r : regions) {
        if (rd.getOwner(r) == name) rd.updateOwner(r, "");
    }

    for (auto& enemy : atWarWith) {
        auto* ec = gs.getCountry(enemy);
        if (ec) {
            auto it = std::find(ec->atWarWith.begin(), ec->atWarWith.end(), name);
            if (it != ec->atWarWith.end()) ec->atWarWith.erase(it);
        }
    }

    if (!faction.empty()) {
        auto* f = gs.getFaction(faction);
        if (f) {
            auto it = std::find(f->members.begin(), f->members.end(), name);
            if (it != f->members.end()) f->members.erase(it);
        }
    }

    auto it = std::find(gs.countryList.begin(), gs.countryList.end(), name);
    if (it != gs.countryList.end()) gs.countryList.erase(it);

    if (gs.controlledCountry == name) {
        gs.controlledCountry.clear();
    }
}

void Country::runAI(GameState& gs) {
    if (!atWarWith.empty()) {
        micro(gs);
    }

    auto& rd = RegionData::instance();


    if (!regions.empty() && atWarWith.empty()) {
        int ownedPorts = 0;
        for (int p : gs.ports) {
            if (std::find(regions.begin(), regions.end(), p) != regions.end()) ownedPorts++;
        }
        if (ownedPorts == 0 || static_cast<float>(ownedPorts) / regions.size() < 0.01f) {
            for (int r : regions) {
                bool isCoastal = false;
                for (int conn : rd.getConnections(r)) {
                    if (rd.getPopulation(conn) == 0) { isCoastal = true; break; }
                }
                if (isCoastal && buildingManager.canBuild(r, BuildingType::Dockyard)) {
                    buildingManager.startConstruction(r, "dockyard", this);
                    break;
                }
            }
        }
    }


    auto& bm = buildingManager;
    int civCount = bm.getBuildingCount("civilian_factory");
    int queueLen = static_cast<int>(bm.queue.size());
    if (queueLen < civCount / 2 + 1 && static_cast<int>(regions.size()) > queueLen + civCount && atWarWith.empty()) {
        if (!regions.empty()) {
            int buildRegion = regions[randInt(0, static_cast<int>(regions.size()) - 1)];
            if (bm.canBuild(buildRegion, "civilian_factory")) {
                bm.startConstruction(buildRegion, "civilian_factory", this);
            } else {
                for (int attempt = 0; attempt < 5; attempt++) {
                    int altRegion = regions[randInt(0, static_cast<int>(regions.size()) - 1)];
                    if (bm.canBuild(altRegion, "civilian_factory")) {
                        bm.startConstruction(altRegion, "civilian_factory", this);
                        break;
                    }
                }
            }
        }
    }


    if (!focus.has_value() && politicalPower > 0) {
        std::vector<std::string> available;
        for (auto& n : focusTreeEngine.nodes) {
            if (focusTreeEngine.canStartFocus(n.name, this)) {
                available.push_back(n.name);
            }
        }
        if (!available.empty()) {
            std::string chosen = available[randInt(0, static_cast<int>(available.size()) - 1)];
            auto* node = focusTreeEngine.getNode(chosen);
            if (node && politicalPower >= static_cast<float>(node->cost)) {
                politicalPower -= static_cast<float>(node->cost);
                focusTreeEngine.completedFocuses.insert(chosen);
                std::vector<std::string> effectStrs;
                for (auto& eff : node->effects) {
                    if (auto* s = std::get_if<std::string>(&eff)) effectStrs.push_back(*s);
                }
                focus = std::make_tuple(chosen, node->days, effectStrs);
            }
        }
    }


    for (int i = static_cast<int>(training.size()) - 1; i >= 0; --i) {
        if (training[i][1] == 0) {
            deployTrainingDivision(gs, i);
        }
    }


    if (atWarWith.empty() && manPower >= 10000.0f && money > gs.TRAINING_COST_PER_DIV && training.empty()) {
        trainDivision(gs, 1);
    }
}

void Country::micro(GameState& gs) {

    auto& rd = RegionData::instance();


    std::vector<std::string> friendlyCountries;
    for (auto& cName : gs.countryList) {
        auto* c = gs.getCountry(cName);
        if (!c) continue;
        bool theyHaveMyAccess = c->hasLandAccessTo(name, gs);
        bool iHaveTheirAccess = hasLandAccessTo(cName, gs);
        if (theyHaveMyAccess && iHaveTheirAccess) friendlyCountries.push_back(cName);
    }

    int friendlyDivCount = 0, enemyDivCount = 0;
    int friendlyStackCount = 0, enemyStackCount = 0;
    for (auto& fc : friendlyCountries) {
        auto* c = gs.getCountry(fc);
        if (!c) continue;
        for (auto& d : c->divisions) {
            if (!d->locked) { friendlyDivCount += d->divisionStack; friendlyStackCount++; }
        }
    }
    for (auto& ec : atWarWith) {
        auto* c = gs.getCountry(ec);
        if (!c) continue;
        for (auto& d : c->divisions) {
            if (!d->locked) { enemyDivCount += d->divisionStack; enemyStackCount++; }
        }
    }

    int borderCount = static_cast<int>(battleBorder.size());
    int stacksPerRegion = 2;
    int divsPerRegion = 8;
    int optimalStackCount = stacksPerRegion * borderCount;
    int optimalDivCount = divsPerRegion * borderCount;


    if (friendlyDivCount < optimalDivCount) {
        while (manPower < 10000.0f && militarySize < 4) {
            militarySize++;
            manPower = std::max(0.0f,
                (std::pow(1.00249688279f, static_cast<float>(militarySize)) - 1.0f) * population - usedManPower);
        }

        int difference = optimalDivCount - friendlyDivCount;
        int divisionsToAdd = std::min(difference, static_cast<int>(manPower / 10000.0f));

        if (divisionsToAdd > 0) {
            int deployReg = -1;
            if (!deployRegion.empty()) {
                deployReg = rd.getCityRegion(deployRegion);
            }
            addDivision(gs, divisionsToAdd, deployReg);


            if (!divisions.empty()) {
                int spawnReg = divisions.back()->region;
                std::vector<Division*> toMerge;
                for (auto& d : divisions) {
                    if (d->region == spawnReg && d->commands.empty()) toMerge.push_back(d.get());
                }
                if (toMerge.size() > 1) mergeDivisions(gs, toMerge);
            }
        }
    }


    if (friendlyStackCount > optimalStackCount) {
        std::unordered_map<int, std::vector<Division*>> unitMap;
        for (auto& d : divisions) {
            if (d->commands.empty() && !d->fighting) {
                unitMap[d->region].push_back(d.get());
            }
        }
        Division* bestRegionDiv = nullptr;
        int maxStacks = 0;
        std::vector<Division*> potentiallyToMerge;
        for (auto& [reg, divList] : unitMap) {
            if (static_cast<int>(divList.size()) > maxStacks) {
                maxStacks = static_cast<int>(divList.size());
                potentiallyToMerge = divList;
            }
        }
        if (potentiallyToMerge.size() > 1) {
            std::sort(potentiallyToMerge.begin(), potentiallyToMerge.end(),
                [](Division* a, Division* b) { return a->divisionStack < b->divisionStack; });
            mergeDivisions(gs, potentiallyToMerge);
        }
    }


    if (friendlyStackCount < optimalStackCount) {
        bool borderingEnemy = false;
        for (auto& b : bordering) {
            if (std::find(atWarWith.begin(), atWarWith.end(), b) != atWarWith.end()) {
                borderingEnemy = true; break;
            }
        }
        if (borderingEnemy) {
            Division* bestDiv = nullptr;
            int bestStack = 0;
            for (auto& d : divisions) {
                if (!d->locked && !d->fighting &&
                    std::find(battleBorder.begin(), battleBorder.end(), d->region) != battleBorder.end()) {
                    if (d->divisionStack > bestStack) {
                        bestStack = d->divisionStack;
                        bestDiv = d.get();
                    }
                }
            }
            if (bestDiv && bestDiv->divisionStack > 1) {
                divideDivision(gs, bestDiv);
            }
        }
    }


    if (!battleBorder.empty() && !std::all_of(divisions.begin(), divisions.end(),
        [](auto& d) { return d->locked; })) {

        std::vector<Division*> availableDivs;
        for (auto& d : divisions) {
            if (!d->locked && !d->fighting && d->commands.empty())
                availableDivs.push_back(d.get());
        }

        std::unordered_map<int, int> priorityMap;
        for (int reg : battleBorder) {
            priorityMap[reg] = 2;
            if (std::find(regions.begin(), regions.end(), reg) != regions.end())
                priorityMap[reg] += 1;
        }


        for (int reg : regions) {
            if (std::find(gs.ports.begin(), gs.ports.end(), reg) != gs.ports.end())
                priorityMap[reg] = 1;
        }


        for (auto& fc : friendlyCountries) {
            auto* c = gs.getCountry(fc);
            if (!c) continue;
            for (auto& d : c->divisions) {
                auto pit = priorityMap.find(d->region);
                if (pit != priorityMap.end()) {
                    pit->second -= 1;

                    auto ait = std::find(availableDivs.begin(), availableDivs.end(), d.get());
                    if (ait != availableDivs.end()) availableDivs.erase(ait);
                } else if (!d->commands.empty()) {
                    int dest = d->commands.back();
                    auto pit2 = priorityMap.find(dest);
                    if (pit2 != priorityMap.end()) pit2->second -= 1;
                }
            }
        }

        for (auto* div : availableDivs) {
            if (priorityMap.empty()) break;
            auto best = std::max_element(priorityMap.begin(), priorityMap.end(),
                [](auto& a, auto& b) { return a.second < b.second; });
            if (best == priorityMap.end() || best->second <= 0) break;
            best->second -= 1;
            div->command(best->first, gs, true, true);
            if (div->commands.empty()) {
                div->attempts++;
                if (div->attempts > 10) div->locked = true;
            }
        }


        for (auto& div : divisions) {
            if (div->fighting || !div->commands.empty()) continue;
            bool onBorder = std::find(battleBorder.begin(), battleBorder.end(), div->region) != battleBorder.end();
            if (!onBorder) continue;

            std::vector<int> optimalRegions;
            int optimalCount = 0;
            for (int conn : rd.getConnections(div->region)) {
                if (std::find(atWarWith.begin(), atWarWith.end(), rd.getOwner(conn)) != atWarWith.end()) {
                    int thisCount = 0;
                    for (int close : rd.getConnections(conn)) {
                        if (std::find(regions.begin(), regions.end(), close) != regions.end())
                            thisCount++;
                    }
                    if (thisCount > optimalCount) {
                        optimalCount = thisCount;
                        optimalRegions = {conn};
                    } else if (thisCount == optimalCount) {
                        optimalRegions.push_back(conn);
                    }
                }
            }
            if (!optimalRegions.empty()) {
                div->command(optimalRegions[randInt(0, static_cast<int>(optimalRegions.size()) - 1)], gs, false, true);
            }
        }
        return;
    }


    if (friendlyDivCount > enemyDivCount) {
        if (toNavalInvade.empty()) {
            for (auto& enemy : atWarWith) {
                auto* ec = gs.getCountry(enemy);
                if (!ec) continue;
                for (int reg : ec->regions) {
                    for (int close : rd.getConnections(reg)) {
                        if (rd.getPopulation(close) == 0) {
                            toNavalInvade.push_back(reg);
                            break;
                        }
                    }
                }
            }
            return;
        }

        bool allIdle = std::all_of(divisions.begin(), divisions.end(),
            [](auto& d) { return d->commands.empty(); });
        bool allNavalLocked = std::all_of(divisions.begin(), divisions.end(),
            [](auto& d) { return d->navalLocked; });

        if (allIdle && !allNavalLocked && !toNavalInvade.empty()) {
            int toMove = toNavalInvade[randInt(0, static_cast<int>(toNavalInvade.size()) - 1)];
            for (auto& div : divisions) {
                if (div->navalLocked) continue;
                div->command(toMove, gs, false, false, 300);
                if (div->commands.empty()) div->navalLocked = true;
            }
            toNavalInvade.erase(std::find(toNavalInvade.begin(), toNavalInvade.end(), toMove));
        }
    }
}

void Country::setIdeology(const std::array<float, 2>& ideo) {
    ideology = ideo;
    ideologyName = Helpers::getIdeologyName(ideology[0], ideology[1]);
    didChangeIdeology = false;
}

void Country::addFactory(GameState& gs, int region) {
    if (region < 0 && !regions.empty()) {
        region = regions[randInt(0, static_cast<int>(regions.size()) - 1)];
    }
    if (region >= 0 && std::find(regions.begin(), regions.end(), region) != regions.end()) {
        buildingManager.addBuilding(region, BuildingType::CivilianFactory);
        factories = buildingManager.countAll(BuildingType::CivilianFactory) +
                    buildingManager.countAll(BuildingType::MilitaryFactory);


        if (gs.industryMapSurf) {
            auto& rd = RegionData::instance();
            Vec2 loc = rd.getLocation(region);
            if (gs.regionsMapSurf) {
                MapFunc::fillRegionMask(gs.industryMapSurf, gs.regionsMapSurf, region, loc.x, loc.y, buildingMapColor(BuildingType::CivilianFactory));
            } else {
                MapFunc::fill(gs.industryMapSurf, loc.x, loc.y, buildingMapColor(BuildingType::CivilianFactory));
            }
            gs.mapDirty = true;
        }
    }
}

void Country::addPort(GameState& gs, int region) {
    auto& rd = RegionData::instance();

    if (region < 0 && !regions.empty()) {

        for (int r : regions) {
            bool coastal = false;
            for (int conn : rd.getConnections(r)) {
                if (rd.getPopulation(conn) == 0 && rd.getOwner(conn).empty()) {
                    coastal = true;
                    break;
                }
            }
            if (coastal && std::find(gs.ports.begin(), gs.ports.end(), r) == gs.ports.end()) {
                region = r;
                break;
            }
        }
    }
    if (region < 0) return;
    if (std::find(regions.begin(), regions.end(), region) == regions.end()) return;


    bool isCoastal = false;
    for (int conn : rd.getConnections(region)) {
        if (rd.getPopulation(conn) == 0 && rd.getOwner(conn).empty()) {
            isCoastal = true;
            break;
        }
    }
    if (!isCoastal) return;


    if (std::find(gs.ports.begin(), gs.ports.end(), region) != gs.ports.end()) return;

    buildingManager.addBuilding(region, BuildingType::Port);
    gs.ports.push_back(region);


    if (gs.industryMapSurf) {
        Vec2 loc = rd.getLocation(region);
        if (gs.regionsMapSurf) {
            MapFunc::fillRegionMask(gs.industryMapSurf, gs.regionsMapSurf, region, loc.x, loc.y, buildingMapColor(BuildingType::Port));
        } else {
            MapFunc::fill(gs.industryMapSurf, loc.x, loc.y, buildingMapColor(BuildingType::Port));
        }
        gs.mapDirty = true;
    }
}

void Country::callToArms(const std::string& country, GameState& gs) {
    auto* ally = gs.getCountry(country);
    if (!ally) return;
    for (auto& enemy : ally->atWarWith) {
        if (std::find(atWarWith.begin(), atWarWith.end(), enemy) == atWarWith.end()) {
            declareWar(enemy, gs);
        }
    }

    if (!faction.empty()) {
        auto* fac = gs.getFaction(faction);
        if (fac) {
            for (auto& member : fac->members) {
                if (std::find(militaryAccess.begin(), militaryAccess.end(), member) == militaryAccess.end()) {
                    militaryAccess.push_back(member);
                }
            }
        }
    }
    if (std::find(ally->militaryAccess.begin(), ally->militaryAccess.end(), name) == ally->militaryAccess.end()) {
        ally->militaryAccess.push_back(name);
    }
}

void Country::annexCountry(const std::string& cult, GameState& gs, const std::string& country) {
    if (!country.empty()) {
        auto* target = gs.getCountry(country);
        if (target) {
            manPower += target->totalMilitary + target->manPower;
            std::vector<int> targetRegions = target->regions;
            addRegions(targetRegions, gs);
        }
    } else {
        std::vector<std::string> countryNames(gs.countryList.begin(), gs.countryList.end());
        for (auto& cName : countryNames) {
            auto* c = gs.getCountry(cName);
            if (c && c->culture == cult) {
                manPower += c->totalMilitary + c->manPower;
                std::vector<int> targetRegions = c->regions;
                addRegions(targetRegions, gs);
                return;
            }
        }
    }
}


void Country::independence(const std::string& countryName, GameState& gs) {

    auto* existing = gs.getCountry(countryName);
    if (!existing) {
        auto& cd = CountryData::instance();
        auto claims = cd.getClaims(countryName);

        std::vector<int> grantRegions;
        for (int r : claims) {
            if (std::find(regions.begin(), regions.end(), r) != regions.end()) {
                grantRegions.push_back(r);
            }
        }
        if (!grantRegions.empty()) {
            auto newCountry = std::make_unique<Country>(countryName, grantRegions, gs);
            newCountry->spawnDivisions(gs);
            gs.registerCountry(countryName, std::move(newCountry));
            militaryAccess.push_back(countryName);
        }
    }
    checkBattleBorder = true;
    checkBordering = true;
}

void Country::civilWar(const std::string& countryName, GameState& gs, bool popup) {

    if (regions.empty()) return;

    auto& rd = RegionData::instance();


    float cx = 0, cy = 0;
    for (int r : regions) {
        Vec2 loc = rd.getLocation(r);
        cx += loc.x; cy += loc.y;
    }
    cx /= regions.size(); cy /= regions.size();


    float angle = randFloat(0, 6.28318f);
    float dx = std::cos(angle), dy = std::sin(angle);


    std::vector<int> rebelRegions;
    for (int r : regions) {
        Vec2 loc = rd.getLocation(r);
        float dot = (loc.x - cx) * dx + (loc.y - cy) * dy;
        if (dot > 0) rebelRegions.push_back(r);
    }


    for (auto& div : divisions) {
        auto it = std::find(rebelRegions.begin(), rebelRegions.end(), div->region);
        if (it != rebelRegions.end()) rebelRegions.erase(it);
    }

    if (rebelRegions.empty()) return;


    auto rebel = std::make_unique<Country>(countryName, rebelRegions, gs);
    rebel->spawnDivisions(gs);
    gs.registerCountry(countryName, std::move(rebel));


    declareWar(countryName, gs, true, popup);
}

void Country::revolution(const std::string& ideology, GameState& gs) {
    auto& cd = CountryData::instance();
    std::string newCountryName = cd.getCountryType(culture, ideology);
    if (newCountryName.empty()) return;

    hasChangedCountry = true;


    auto* existing = gs.getCountry(newCountryName);
    if (existing) {

        civilWar(newCountryName, gs);
    } else {

        auto newCountry = std::make_unique<Country>(newCountryName, std::vector<int>{}, gs);


        newCountry->totalMilitary = totalMilitary;
        newCountry->population = population;
        newCountry->manPower = manPower;
        newCountry->usedManPower = usedManPower;
        newCountry->politicalPower = politicalPower;
        newCountry->money = money;
        newCountry->factories = factories;
        newCountry->baseStability = baseStability;
        newCountry->stability = stability;
        newCountry->training = training;
        newCountry->politicalMultiplier = politicalMultiplier;
        newCountry->moneyMultiplier = moneyMultiplier;
        newCountry->defenseMultiplier = defenseMultiplier;
        newCountry->attackMultiplier = attackMultiplier;
        newCountry->transportMultiplier = transportMultiplier;
        newCountry->canMakeFaction = canMakeFaction;
        newCountry->hasChangedCountry = true;
        newCountry->expandedInvitations = expandedInvitations;
        newCountry->puppetTo = puppetTo;


        newCountry->addRegions(regions, gs, true, false);


        for (auto& div : divisions) {
            div->country = newCountryName;
            newCountry->divisions.push_back(std::move(div));
        }
        divisions.clear();


        for (auto& enemy : atWarWith) {
            newCountry->declareWar(enemy, gs, true, false);
        }


        if (gs.controlledCountry == name) {
            gs.controlledCountry = newCountryName;
        }

        gs.countries[newCountryName] = std::move(newCountry);


        atWarWith.clear();
        regions.clear();
    }
}
void Country::replaceCountry(const std::string& newName, GameState& gs) {

    auto newC = std::make_unique<Country>(newName, std::vector<int>{}, gs);


    newC->totalMilitary = totalMilitary;
    newC->population = population;
    newC->manPower = manPower;
    newC->usedManPower = usedManPower;
    newC->politicalPower = politicalPower;
    newC->money = money;
    newC->factories = factories;
    newC->baseStability = baseStability;
    newC->stability = stability;
    newC->training = training;
    newC->politicalMultiplier = politicalMultiplier;
    newC->moneyMultiplier = moneyMultiplier;
    newC->buildSpeed = buildSpeed;
    newC->defenseMultiplier = defenseMultiplier;
    newC->attackMultiplier = attackMultiplier;
    newC->transportMultiplier = transportMultiplier;
    newC->canMakeFaction = canMakeFaction;
    newC->hasChangedCountry = true;
    newC->expandedInvitations = expandedInvitations;
    newC->puppetTo = puppetTo;


    newC->addRegions(regions, gs, true, false);


    for (auto& div : divisions) {
        div->country = newName;
        newC->divisions.push_back(std::move(div));
    }
    divisions.clear();


    for (auto& enemy : atWarWith) {
        newC->declareWar(enemy, gs, true, false);
    }


    if (gs.controlledCountry == name) {
        gs.controlledCountry = newName;
    }

    gs.registerCountry(newName, std::move(newC));
    kill(gs, true);
}

void Country::changeDeployment(GameState& gs) {

    if (cities.empty()) return;

    if (deployRegion.empty()) {
        deployRegion = cities[0];
    } else {
        auto it = std::find(cities.begin(), cities.end(), deployRegion);
        if (it != cities.end() && ++it != cities.end()) {
            deployRegion = *it;
        } else {
            deployRegion = cities[0];
        }
    }
}

bool Country::canAffordTrainingResources(int divCount) const {
    return hasResources(resourceManager, trainingResourceCost(divCount));
}

bool Country::canAffordDeploymentResources(int divCount) const {
    return hasResources(resourceManager, deploymentResourceCost(divCount));
}

void Country::spendTrainingResources(int divCount) {
    spendResources(resourceManager, trainingResourceCost(divCount));
}

void Country::spendDeploymentResources(int divCount) {
    spendResources(resourceManager, deploymentResourceCost(divCount));
}
void Country::build(GameState& gs, const std::string& n, int days, int region) {

    if (region < 0 && !regions.empty()) {
        region = regions[randInt(0, static_cast<int>(regions.size()) - 1)];
    }
    if (region < 0 || std::find(regions.begin(), regions.end(), region) == regions.end()) return;


    BuildingType type = buildingTypeFromString(n);
    if (n == "factory") type = BuildingType::CivilianFactory;


    int actualDays = std::max(1, static_cast<int>(days * buildSpeed));

    buildingManager.queueConstruction(type, region, actualDays);


    if (gs.industryMapSurf) {
        auto& rd = RegionData::instance();
        Vec2 loc = rd.getLocation(region);
        Color bc = buildingMapColor(type);

        Color constructColor = {
            static_cast<uint8_t>(bc.r / 2),
            static_cast<uint8_t>(bc.g / 2),
            static_cast<uint8_t>(bc.b / 2)
        };
        if (gs.regionsMapSurf) {
            MapFunc::fillRegionMask(gs.industryMapSurf, gs.regionsMapSurf, region, loc.x, loc.y, constructColor);
        } else {
            MapFunc::fill(gs.industryMapSurf, loc.x, loc.y, constructColor);
        }
        gs.mapDirty = true;
    }


    if (name == gs.controlledCountry) {
        Audio::instance().playSound("buildSound");
    }
}

void Country::destroy(GameState& gs, int region) {
    if (region < 0 || std::find(regions.begin(), regions.end(), region) == regions.end()) return;


    auto blist = buildingManager.getInRegion(region);
    if (!blist.empty()) {
        buildingManager.removeBuilding(region, blist[0].type);
        factories = buildingManager.countAll(BuildingType::CivilianFactory) +
                    buildingManager.countAll(BuildingType::MilitaryFactory);


        if (gs.industryMapSurf) {
            auto& rd = RegionData::instance();
            Vec2 loc = rd.getLocation(region);
            auto remaining = buildingManager.getInRegion(region);
            Color paintColor = remaining.empty() ? Color{255, 255, 255} : buildingMapColor(remaining[0].type);
            if (gs.regionsMapSurf) {
                MapFunc::fillRegionMask(gs.industryMapSurf, gs.regionsMapSurf, region, loc.x, loc.y, paintColor);
            } else {
                MapFunc::fill(gs.industryMapSurf, loc.x, loc.y, paintColor);
            }
            gs.mapDirty = true;
        }
    }
}
void Country::reloadCultureMap(GameState& gs) {


}
