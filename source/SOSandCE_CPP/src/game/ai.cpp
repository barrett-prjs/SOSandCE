#include "game/ai.h"
#include "game/game_state.h"
#include "game/country.h"
#include "game/division.h"
#include "game/faction.h"
#include "game/buildings.h"
#include "game/helpers.h"
#include "data/region_data.h"

namespace {

bool isCapitalRegion(const Country* country, int region);
bool isStrategicCity(int region);

int currentHourStamp(const GameState& gs) {
    return gs.time.totalDays * 24 + gs.time.hour;
}

bool containsRegion(const std::vector<int>& regions, int region) {
    return std::find(regions.begin(), regions.end(), region) != regions.end();
}

float regionDistanceScore(int fromRegion, int toRegion) {
    auto& rd = RegionData::instance();
    Vec2 a = rd.getLocation(fromRegion);
    Vec2 b = rd.getLocation(toRegion);
    float dx = a.x - b.x;
    float dy = a.y - b.y;
    return dx * dx + dy * dy;
}

int countFriendlyStacks(const GameState& gs, const std::string& countryName, int region) {
    int strength = 0;
    auto it = gs.divisionsByRegion.find(region);
    if (it == gs.divisionsByRegion.end()) return 0;
    for (auto* div : it->second) {
        if (div && div->country == countryName && !div->fighting) {
            strength += std::max(1, div->divisionStack);
        }
    }
    return strength;
}

int countEnemyStacks(const GameState& gs, const Country* country, int region) {
    if (!country) return 0;
    int strength = 0;
    auto it = gs.divisionsByRegion.find(region);
    if (it == gs.divisionsByRegion.end()) return 0;
    for (auto* div : it->second) {
        if (!div || div->country == country->name) continue;
        if (std::find(country->atWarWith.begin(), country->atWarWith.end(), div->country) != country->atWarWith.end()) {
            strength += std::max(1, div->divisionStack);
        }
    }
    return strength;
}

int countFriendlySupportForTarget(const GameState& gs, const Country* country, int targetRegion) {
    if (!country) return 0;
    auto& rd = RegionData::instance();
    int support = 0;
    for (int conn : rd.getConnections(targetRegion)) {
        support += countFriendlyStacks(gs, country->name, conn);
    }
    return support;
}

int committedFriendlyStacks(const GameState& gs, const Country* country, int region) {
    if (!country) return 0;
    int strength = 0;
    for (const auto& div : country->divisions) {
        if (!div) continue;
        if (div->region == region) {
            strength += std::max(1, div->divisionStack);
            continue;
        }
        if (!div->commands.empty() && div->commands.back() == region) {
            strength += std::max(1, div->divisionStack);
        }
    }
    return strength;
}

int enemyPressureAgainstRegion(const GameState& gs, const Country* country, int region) {
    if (!country) return 0;
    auto& rd = RegionData::instance();
    int pressure = 0;
    for (int conn : rd.getConnections(region)) {
        if (std::find(country->atWarWith.begin(), country->atWarWith.end(), rd.getOwner(conn)) != country->atWarWith.end()) {
            pressure += std::max(1, countEnemyStacks(gs, country, conn));
        }
    }
    return pressure;
}

int frontlineDesiredStrength(const GameState& gs, const Country* country, int region) {
    if (!country) return 1;
    auto& rd = RegionData::instance();
    int enemyPressure = enemyPressureAgainstRegion(gs, country, region);
    int hostileNeighbors = 0;
    for (int conn : rd.getConnections(region)) {
        if (std::find(country->atWarWith.begin(), country->atWarWith.end(), rd.getOwner(conn)) != country->atWarWith.end()) {
            hostileNeighbors++;
        }
    }

    int desired = std::max(2, enemyPressure + hostileNeighbors);
    if (isCapitalRegion(country, region)) desired += 4;
    else if (isStrategicCity(region)) desired += 2;
    return std::clamp(desired, 2, 18);
}

bool shouldHoldCurrentFront(const GameState& gs, const Country* country, const Division* div) {
    if (!country || !div) return false;
    int friendlyHere = committedFriendlyStacks(gs, country, div->region);
    int pressure = enemyPressureAgainstRegion(gs, country, div->region);
    if (isCapitalRegion(country, div->region)) {
        return friendlyHere <= pressure + 1;
    }
    if (isStrategicCity(div->region)) {
        return friendlyHere <= pressure;
    }
    return friendlyHere <= std::max(1, pressure);
}

int militaryAccessCost(const Country* country, const Country* target) {
    if (!country || !target) return 50;
    int cost = 50;
    if (!country->faction.empty() && country->faction == target->faction) cost -= 25;
    if (country->ideologyName != target->ideologyName) cost += 25;
    return std::max(0, cost);
}

bool isDiplomaticActionReady(int lastHour, int cooldownHours, const GameState& gs) {
    return lastHour < 0 || currentHourStamp(gs) - lastHour >= cooldownHours;
}

int countDivisionStrength(const Country* country) {
    if (!country) return 0;
    int strength = 0;
    for (const auto& div : country->divisions) {
        if (div) strength += std::max(1, div->divisionStack);
    }
    return strength;
}

bool isCapitalRegion(const Country* country, int region) {
    if (!country || country->capital.empty()) return false;
    return RegionData::instance().getCityRegion(country->capital) == region;
}

bool isStrategicCity(int region) {
    return !RegionData::instance().getCity(region).empty();
}

bool recentlyOrdered(const Division* div, const GameState& gs, int cooldownHours) {
    if (!div) return false;
    return div->lastAIOrderHour >= 0 && currentHourStamp(gs) - div->lastAIOrderHour < cooldownHours;
}

void issueAiOrder(Division* div, int region, GameState& gs, bool ignoreEnemy, bool ignoreWater, int iterations) {
    if (!div) return;
    div->command(region, gs, ignoreEnemy, ignoreWater, iterations);
    if (!div->commands.empty()) {
        div->lastAIOrderHour = currentHourStamp(gs);
        div->aiObjectiveRegion = region;
    }
}

bool rebalanceIdleStacks(Country* country, GameState& gs) {
    if (!country) return false;

    std::unordered_map<int, std::vector<Division*>> byRegion;
    for (auto& div : country->divisions) {
        if (!div || div->fighting || div->locked || !div->commands.empty()) continue;
        byRegion[div->region].push_back(div.get());
    }

    for (auto& [region, divs] : byRegion) {
        std::sort(divs.begin(), divs.end(), [](Division* a, Division* b) {
            return a->divisionStack < b->divisionStack;
        });

        int desiredStacks = 1;
        if (containsRegion(country->battleBorder, region)) {
            desiredStacks = std::clamp(1 + enemyPressureAgainstRegion(gs, country, region) / 4, 2, 4);
        }
        if (static_cast<int>(divs.size()) > desiredStacks) {
            country->mergeDivisions(gs, {divs[0], divs[1]});
            return true;
        }

        if (containsRegion(country->battleBorder, region)) {
            auto biggest = std::max_element(divs.begin(), divs.end(), [](Division* a, Division* b) {
                return a->divisionStack < b->divisionStack;
            });
            if (biggest != divs.end() && (*biggest)->divisionStack > 4 && static_cast<int>(divs.size()) < desiredStacks) {
                country->divideDivision(gs, *biggest);
                return true;
            }
        }
    }

    return false;
}

}

AIController::AIController(const std::string& name, GameState& gs)
    : countryName(name)
{

    Country* c = gs.getCountry(name);
    if (c) {
        std::string ideology = getIdeologyName(c->ideology[0], c->ideology[1]);
        if (ideology == "communist") {
            personality = {0.6f, 0.5f, 0.3f, 0.7f, 0.6f, 0.8f};
        } else if (ideology == "nationalist") {
            personality = {0.8f, 0.4f, 0.2f, 0.4f, 0.8f, 0.7f};
        } else if (ideology == "liberal") {
            personality = {0.3f, 0.5f, 0.7f, 0.7f, 0.3f, 0.5f};
        } else if (ideology == "monarchist") {
            personality = {0.5f, 0.6f, 0.5f, 0.5f, 0.5f, 0.6f};
        } else {
            personality = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
        }
    }
}

AIController::~AIController() = default;

Country* AIController::getCountry(GameState& gs) const {
    return gs.getCountry(countryName);
}


void AIController::update(GameState& gs) {
    Country* country = getCountry(gs);
    if (!country) return;

    atWar = !country->atWarWith.empty();
    int hourStamp = currentHourStamp(gs);
    int actionInterval = atWar ? warActionIntervalHours : peaceActionIntervalHours;
    if (lastActionHourStamp >= 0 && hourStamp - lastActionHourStamp < actionInterval) return;
    lastActionHourStamp = hourStamp;

    enemies = country->atWarWith;


    evaluateThreats(gs);
    evaluateEconomy(gs);
    evaluatePolitics(gs);


    if (atWar) {
        assignFrontline(gs);
        manageDivisions(gs);
    } else {
        evaluateWar(gs);
        evaluateAlliances(gs);
    }
    manageDivisionTraining(gs);


    manageBuildQueue(gs);
    manageTrade(gs);
    manageFocus(gs);
}


void AIController::evaluateThreats(GameState& gs) {
    Country* country = getCountry(gs);
    if (!country) return;

    enemies.clear();
    potentialAllies.clear();

    for (auto& neighbor : country->bordering) {
        if (neighbor == countryName) continue;

        Country* n = gs.getCountry(neighbor);
        if (!n) continue;

        float threat = threatLevel(neighbor, gs);

        if (threat > 50.0f) {
            enemies.push_back(neighbor);
        }


        std::string myIdeology = getIdeologyName(country->ideology[0], country->ideology[1]);
        std::string theirIdeology = getIdeologyName(n->ideology[0], n->ideology[1]);
        if (myIdeology == theirIdeology && threat < 30.0f) {
            potentialAllies.push_back(neighbor);
        }
    }
}

void AIController::evaluateAlliances(GameState& gs) {
    considerMilitaryAccess(gs);
    considerJoinFaction(gs);
    considerCreateFaction(gs);
    considerAlliance(gs);
}

void AIController::evaluateWar(GameState& gs) {
    considerDeclareWar(gs);
}

void AIController::evaluateEconomy(GameState& ) {

}

void AIController::evaluatePolitics(GameState& ) {

}


void AIController::manageDivisions(GameState& gs) {
    Country* country = getCountry(gs);
    if (!country) return;

    if (rebalanceIdleStacks(country, gs)) return;

    for (auto& div : country->divisions) {
        if (!div) continue;
        if (div->fighting || div->locked) continue;
        if (!div->commands.empty()) continue;
        if (recentlyOrdered(div.get(), gs, 12)) continue;

        microDivision(div.get(), gs);
    }
}

void AIController::assignFrontline(GameState& gs) {
    Country* country = getCountry(gs);
    if (!country) return;

    if (country->atWarWith.empty()) return;
    if (country->battleBorder.empty()) return;

    struct FrontNeed {
        int region = -1;
        int desired = 0;
        int committed = 0;
        float priority = 0.0f;
    };

    std::unordered_map<int, int> desiredByRegion;
    std::unordered_map<int, int> committedByRegion;
    std::unordered_map<int, float> priorityByRegion;
    std::vector<FrontNeed> needs;
    for (int region : country->battleBorder) {
        int enemyPressure = enemyPressureAgainstRegion(gs, country, region);
        int desired = frontlineDesiredStrength(gs, country, region);
        int committed = committedFriendlyStacks(gs, country, region);
        desiredByRegion[region] = desired;
        committedByRegion[region] = committed;
        float priority = enemyPressure * 5.0f;
        if (isCapitalRegion(country, region)) priority += 28.0f;
        else if (isStrategicCity(region)) priority += 16.0f;
        priority += std::max(0, desired - committed) * 9.0f;
        priorityByRegion[region] = priority;
        needs.push_back({region, desired, committed, priority});
    }

    std::sort(needs.begin(), needs.end(), [](const FrontNeed& a, const FrontNeed& b) {
        return a.priority > b.priority;
    });

    float highestPriority = needs.empty() ? 0.0f : needs.front().priority;
    std::vector<Division*> available;
    for (auto& div : country->divisions) {
        if (!div) continue;
        if (div->fighting || div->locked) continue;
        if (recentlyOrdered(div.get(), gs, 4)) continue;

        int assignedRegion = div->region;
        if (div->aiObjectiveRegion >= 0) assignedRegion = div->aiObjectiveRegion;
        else if (!div->commands.empty()) assignedRegion = div->commands.back();

        bool assignedToFront = containsRegion(country->battleBorder, assignedRegion);
        if (!assignedToFront) {
            available.push_back(div.get());
            continue;
        }

        int desiredHere = desiredByRegion[assignedRegion];
        int committedHere = committedByRegion[assignedRegion];
        int stack = std::max(1, div->divisionStack);
        float assignedPriority = priorityByRegion[assignedRegion];
        bool hardSurplus = committedHere - stack >= desiredHere;
        bool softSurplus = committedHere - stack >= std::max(1, desiredHere - 2);
        if (hardSurplus || (highestPriority >= assignedPriority + 18.0f && softSurplus &&
                            !shouldHoldCurrentFront(gs, country, div.get()))) {
            available.push_back(div.get());
        }
    }

    if (available.empty()) return;

    for (auto& need : needs) {
        while (need.committed < need.desired && !available.empty()) {
            auto bestIt = std::min_element(available.begin(), available.end(), [&](Division* a, Division* b) {
                return regionDistanceScore(a->region, need.region) < regionDistanceScore(b->region, need.region);
            });
            if (bestIt == available.end()) break;
            Division* div = *bestIt;
            int donorRegion = div->region;
            int stack = std::max(1, div->divisionStack);
            if (containsRegion(country->battleBorder, donorRegion)) {
                committedByRegion[donorRegion] = std::max(0, committedByRegion[donorRegion] - stack);
            }
            if (div->region != need.region) {
                issueAiOrder(div, need.region, gs, true, true, 100);
            }
            available.erase(bestIt);
            need.committed += stack;
            committedByRegion[need.region] += stack;
        }
    }
}

void AIController::orderAttack(GameState& gs) {
    Country* country = getCountry(gs);
    if (!country) return;

    for (auto& div : country->divisions) {
        if (!div || div->fighting || div->locked) continue;

        int target = findBestTarget(div.get(), gs);
        if (target >= 0) {
            div->command(target, gs, false, true, 200);
        }
    }
}

void AIController::orderDefend(GameState& gs) {

    assignFrontline(gs);
}

void AIController::orderRetreat(GameState& gs) {
    Country* country = getCountry(gs);
    if (!country) return;

    for (auto& div : country->divisions) {
        if (!div || !div->fighting) continue;

        if (shouldRetreat(div.get(), gs)) {
            int safe = findSafeRegion(div.get(), gs);
            if (safe >= 0) {
                div->command(safe, gs, true, true, 100);
            }
        }
    }
}

void AIController::manageDivisionTraining(GameState& gs) {
    Country* country = getCountry(gs);
    if (!country) return;


    for (int i = static_cast<int>(country->training.size()) - 1; i >= 0; --i) {
        if (country->training[i][1] == 0) {
            country->deployTrainingDivision(gs, i);
        }
    }

    int committedStacks = 0;
    for (auto& div : country->divisions) {
        if (div) committedStacks += std::max(1, div->divisionStack);
    }
    int desiredStacks = std::max(2, static_cast<int>(country->battleBorder.size()) * 4);
    int reserveStacks = atWar ? 2 : 1;
    int trainQueueStacks = 0;
    for (const auto& entry : country->training) {
        trainQueueStacks += entry[0];
    }

    if (country->manPower >= 10000.0f &&
        country->money > gs.TRAINING_COST_PER_DIV &&
        trainQueueStacks < reserveStacks &&
        committedStacks + trainQueueStacks < desiredStacks) {
        country->trainDivision(gs, 1);
    }
}

void AIController::microDivision(Division* div, GameState& gs) {
    if (!div) return;

    Country* country = getCountry(gs);
    if (!country) return;

    if (div->recovering) return;
    if (shouldRetreat(div, gs)) {
        int safe = findSafeRegion(div, gs);
        if (safe >= 0 && safe != div->region) {
            issueAiOrder(div, safe, gs, true, true, 100);
        }
        return;
    }

    if (!atWar) return;
    if (!containsRegion(country->battleBorder, div->region)) return;
    if (shouldHoldCurrentFront(gs, country, div)) return;
    if (div->maxUnits > 0 && div->units / div->maxUnits < 0.6f) return;
    if (div->maxResources > 0 && div->resources / div->maxResources < 0.45f) return;

    int target = findBestTarget(div, gs);
    if (target >= 0) {
        issueAiOrder(div, target, gs, false, true, 200);
    }
}

int AIController::findBestTarget(Division* div, GameState& gs) const {
    if (!div) return -1;

    Country* country = getCountry(gs);
    if (!country) return -1;

    auto& rd = RegionData::instance();
    int bestTarget = -1;
    float bestScore = -1000.0f;
    float healthRatio = div->maxUnits > 0.0f ? div->units / div->maxUnits : 1.0f;
    float supplyRatio = div->maxResources > 0.0f ? div->resources / div->maxResources : 1.0f;
    if (healthRatio < 0.55f || supplyRatio < 0.4f) return -1;

    for (int rid : rd.getConnections(div->region)) {
        std::string owner = rd.getOwner(rid);
        if (std::find(country->atWarWith.begin(), country->atWarWith.end(), owner) == country->atWarWith.end()) {
            continue;
        }

        Country* enemyCountry = gs.getCountry(owner);
        int enemyStrength = countEnemyStacks(gs, country, rid);
        int friendlySupport = countFriendlySupportForTarget(gs, country, rid);
        int currentFrontPressure = enemyPressureAgainstRegion(gs, country, div->region);
        int targetPressure = enemyPressureAgainstRegion(gs, country, rid);
        bool capital = isCapitalRegion(enemyCountry, rid);
        bool city = isStrategicCity(rid);

        if (friendlySupport <= 0) continue;
        if (currentFrontPressure > friendlySupport) continue;
        float effectiveFriendly = (friendlySupport + div->divisionStack) * (0.65f + healthRatio * 0.35f) * (0.6f + supplyRatio * 0.4f);
        float effectiveEnemy = enemyStrength + targetPressure * 0.35f;
        if (enemyStrength > 0 && effectiveFriendly < effectiveEnemy) continue;

        float score = 10.0f + effectiveFriendly * 3.0f - effectiveEnemy * 4.0f - currentFrontPressure * 1.5f;
        if (capital) score += 32.0f;
        else if (city) score += 14.0f;
        if (enemyCountry && enemyCountry->regions.size() <= 4) score += 8.0f;
        if (enemyStrength == 0) score += 6.0f;

        if (score > bestScore) {
            bestScore = score;
            bestTarget = rid;
        }
    }

    return bestScore >= 10.0f ? bestTarget : -1;
}

int AIController::findSafeRegion(Division* div, GameState& gs) const {
    if (!div) return -1;

    Country* country = getCountry(gs);
    if (!country) return -1;

    if (country->regions.empty()) return -1;

    int bestRegion = div->region;
    float bestScore = -1000.0f;
    for (int rid : country->regions) {
        float score = 0.0f;
        if (!containsRegion(country->battleBorder, rid)) score += 12.0f;
        if (isCapitalRegion(country, rid)) score += 8.0f;
        else if (isStrategicCity(rid)) score += 4.0f;
        score += countFriendlyStacks(gs, country->name, rid) * 2.0f;
        score -= regionDistanceScore(div->region, rid) / 25000.0f;
        if (score > bestScore) {
            bestScore = score;
            bestRegion = rid;
        }
    }

    return bestRegion;
}

bool AIController::shouldRetreat(Division* div, GameState& gs) const {
    if (!div) return false;
    if (div->maxUnits > 0 && div->units / div->maxUnits < 0.3f) return true;
    if (div->maxResources > 0 && div->resources / div->maxResources < 0.2f) return true;
    Country* country = getCountry(gs);
    if (!country) return false;

    int localFriendly = countFriendlySupportForTarget(gs, country, div->region);
    int localEnemy = countEnemyStacks(gs, country, div->region);
    if (localEnemy > 0 && div->maxUnits > 0 && div->units / div->maxUnits < 0.45f && localEnemy > localFriendly) {
        return true;
    }
    return false;
}


void AIController::considerJoinFaction(GameState& gs) {
    Country* country = getCountry(gs);
    if (!country || !country->faction.empty()) return;
    if (country->politicalPower < 25.0f) return;
    if (!isDiplomaticActionReady(lastFactionActionHourStamp, factionActionCooldownHours, gs)) return;

    for (auto& fName : gs.factionList) {
        Faction* f = gs.getFaction(fName);
        if (!f) continue;

        Country* leader = gs.getCountry(f->factionLeader);
        if (!leader) continue;

        float desirability = allianceDesirability(f->factionLeader, gs);
        if (desirability > 0.6f && randFloat(0.0f, 1.0f) < 0.1f) {
            country->politicalPower -= 25.0f;
            f->addCountry(countryName, gs, false, false);
            lastFactionActionHourStamp = currentHourStamp(gs);
            return;
        }
    }
}

void AIController::considerCreateFaction(GameState& gs) {
    Country* country = getCountry(gs);
    if (!country || !country->faction.empty() || !country->canMakeFaction) return;
    if (country->politicalPower < 100.0f) return;
    if (!isDiplomaticActionReady(lastFactionActionHourStamp, factionActionCooldownHours, gs)) return;

    if (personality.diplomacy > 0.5f && randFloat(0.0f, 1.0f) < 0.05f) {
        std::string factionName = countryName + " Pact";
        auto faction = std::make_unique<Faction>(factionName, std::vector<std::string>{countryName}, gs);
        gs.registerFaction(factionName, std::move(faction));
        country->faction = factionName;
        country->factionLeader = true;
        country->politicalPower -= 100.0f;
        lastFactionActionHourStamp = currentHourStamp(gs);
    }
}

void AIController::considerMilitaryAccess(GameState& gs) {
    Country* country = getCountry(gs);
    if (!country) return;
    if (!isDiplomaticActionReady(lastAccessActionHourStamp, accessActionCooldownHours, gs)) return;

    std::string preferredEnemy;
    if (!country->atWarWith.empty()) preferredEnemy = country->atWarWith.front();
    else if (!targetCountry.empty()) preferredEnemy = targetCountry;

    std::string bestTarget;
    float bestScore = -1000.0f;
    for (const auto& otherName : gs.countryList) {
        if (otherName == countryName) continue;
        Country* other = gs.getCountry(otherName);
        if (!other) continue;
        if (std::find(country->atWarWith.begin(), country->atWarWith.end(), otherName) != country->atWarWith.end()) continue;
        if (std::find(country->militaryAccess.begin(), country->militaryAccess.end(), otherName) != country->militaryAccess.end()) continue;

        int cost = militaryAccessCost(country, other);
        if (country->politicalPower < static_cast<float>(cost)) continue;

        float score = allianceDesirability(otherName, gs) * 100.0f - static_cast<float>(cost);
        if (!preferredEnemy.empty() &&
            std::find(other->bordering.begin(), other->bordering.end(), preferredEnemy) != other->bordering.end()) {
            score += 40.0f;
        }
        if (std::find(country->bordering.begin(), country->bordering.end(), otherName) != country->bordering.end()) {
            score += 10.0f;
        }
        if (score > bestScore) {
            bestScore = score;
            bestTarget = otherName;
        }
    }

    if (!bestTarget.empty() && bestScore >= 35.0f) {
        Country* other = gs.getCountry(bestTarget);
        int cost = militaryAccessCost(country, other);
        country->politicalPower -= static_cast<float>(cost);
        if (std::find(country->militaryAccess.begin(), country->militaryAccess.end(), bestTarget) == country->militaryAccess.end()) {
            country->militaryAccess.push_back(bestTarget);
        }
        lastAccessActionHourStamp = currentHourStamp(gs);
    }
}

void AIController::considerDeclareWar(GameState& gs) {
    Country* country = getCountry(gs);
    if (!country || !country->atWarWith.empty()) return;

    if (!canAffordWar(gs)) return;
    if (country->politicalPower < 25.0f) return;
    if (!isDiplomaticActionReady(lastWarActionHourStamp, warActionCooldownHours, gs)) return;

    float warWillingness = personality.aggressiveness;
    float declareChance = std::clamp(0.02f + warWillingness * 0.18f, 0.02f, 0.35f);
    if (randFloat(0.0f, 1.0f) > declareChance) return;

    float myStrength = static_cast<float>(countDivisionStrength(country));

    for (auto& neighbor : country->bordering) {
        Country* n = gs.getCountry(neighbor);
        if (!n) continue;
        if (n->faction == country->faction && !country->faction.empty()) continue;

        float theirStrength = static_cast<float>(countDivisionStrength(n));

        if (myStrength > theirStrength * 1.5f) {
            bool hasClaims = false;
            for (int core : country->coreRegions) {
                for (int rid : n->regions) {
                    if (core == rid) { hasClaims = true; break; }
                }
                if (hasClaims) break;
            }

            if (hasClaims || warWillingness > 0.6f) {
                wantsWar = true;
                targetCountry = neighbor;
                country->politicalPower -= 25.0f;
                country->declareWar(neighbor, gs, false, false);
                lastWarActionHourStamp = currentHourStamp(gs);
                return;
            }
        }
    }
}

void AIController::considerMakePeace(GameState& gs) {
    Country* country = getCountry(gs);
    if (!country || country->atWarWith.empty()) return;


    float myStrength = 0.0f;
    for (auto& div : country->divisions) {
        if (div) myStrength += div->divisionStack;
    }

    if (myStrength < 1.0f && country->regions.size() < 3) {

        for (auto& enemy : country->atWarWith) {
            country->makePeace(enemy, gs, false);
            return;
        }
    }
}

void AIController::considerAlliance(GameState& gs) {
    Country* country = getCountry(gs);
    if (!country || country->faction.empty() || !country->factionLeader) return;
    if (country->politicalPower < 25.0f) return;
    if (!isDiplomaticActionReady(lastFactionActionHourStamp, factionActionCooldownHours, gs)) return;

    std::string bestTarget;
    float bestScore = -1000.0f;
    for (const auto& otherName : gs.countryList) {
        if (otherName == countryName) continue;
        Country* other = gs.getCountry(otherName);
        if (!other) continue;
        if (!other->faction.empty()) continue;
        if (std::find(country->atWarWith.begin(), country->atWarWith.end(), otherName) != country->atWarWith.end()) continue;

        float score = allianceDesirability(otherName, gs) * 100.0f;
        if (country->expandedInvitations) score += 10.0f;
        if (!country->expandedInvitations &&
            std::find(country->bordering.begin(), country->bordering.end(), otherName) == country->bordering.end()) {
            score -= 25.0f;
        }
        if (score > bestScore) {
            bestScore = score;
            bestTarget = otherName;
        }
    }

    if (!bestTarget.empty() && bestScore >= 60.0f) {
        Faction* fac = gs.getFaction(country->faction);
        if (fac) {
            country->politicalPower -= 25.0f;
            fac->addCountry(bestTarget, gs, false, false);
            lastFactionActionHourStamp = currentHourStamp(gs);
        }
    }
}


void AIController::manageBuildQueue(GameState& gs) {
    Country* country = getCountry(gs);
    if (!country) return;

    auto& bm = country->buildingManager;


    int maxQueue = std::max(1, bm.countAll(BuildingType::CivilianFactory));
    if (static_cast<int>(bm.constructionQueue.size()) >= maxQueue) return;


    std::vector<BuildingType> buildOrder;
    bool needsResources = false;
    for (int i = 0; i < RESOURCE_COUNT; ++i) {
        if (country->resourceManager.stockpile[i] < 120.0f || country->resourceManager.deficit[i] > 0.0f) {
            needsResources = true;
            break;
        }
    }

    if (needsResources) {
        buildOrder = {BuildingType::Mine, BuildingType::OilWell, BuildingType::Refinery,
                      BuildingType::Infrastructure, BuildingType::CivilianFactory,
                      BuildingType::MilitaryFactory};
    } else if (personality.economicFocus > 0.6f) {
        buildOrder = {BuildingType::CivilianFactory, BuildingType::Infrastructure,
                      BuildingType::MilitaryFactory};
    } else if (personality.aggressiveness > 0.6f) {
        buildOrder = {BuildingType::MilitaryFactory, BuildingType::Infrastructure,
                      BuildingType::CivilianFactory};
    } else {
        buildOrder = {BuildingType::CivilianFactory, BuildingType::MilitaryFactory,
                      BuildingType::Infrastructure};
    }


    auto getDays = [](BuildingType t) -> int {
        switch (t) {
            case BuildingType::CivilianFactory:  return 120;
            case BuildingType::MilitaryFactory:  return 120;
            case BuildingType::Infrastructure:   return 90;
            case BuildingType::Port:             return 60;
            case BuildingType::Fortress:         return 150;
            default: return 120;
        }
    };

    for (auto type : buildOrder) {
        for (int rid : country->regions) {
            if (bm.canBuild(rid, type)) {

                std::string typeStr = buildingTypeToString(type);
                if (bm.startConstruction(rid, typeStr, country)) return;
            }
        }
    }
}

void AIController::manageTrade(GameState& gs) {
    Country* country = getCountry(gs);
    if (!country) return;

    for (auto& contract : gs.tradeContracts) {
        if (!contract.active) continue;
        if (contract.importer != countryName) continue;

        int ri = static_cast<int>(contract.resource);
        Country* exporter = gs.getCountry(contract.exporter);
        TradeApproval approval = evaluateTradeOffer(exporter, country, contract.resource, contract.amount, gs);
        if (!country->resourceManager.hasDeficit(contract.resource) ||
            country->money <= contract.price * contract.amount ||
            !approval.approved) {
            contract.cancelWithCleanup(gs);
            country->resourceManager.tradeImports[ri] = 0.0f;
        }
    }

    if (country->money < 10000.0f) return;

    for (int ri = 0; ri < RESOURCE_COUNT; ++ri) {
        Resource res = static_cast<Resource>(ri);
        if (!country->resourceManager.hasDeficit(res)) continue;

        bool alreadyImporting = false;
        for (auto& contract : gs.tradeContracts) {
            if (contract.active && contract.importer == countryName && contract.resource == res) {
                alreadyImporting = true;
                break;
            }
        }
        if (alreadyImporting) continue;

        Country* bestExporter = nullptr;
        float bestScore = -1000.0f;
        float bestAmount = 0.0f;
        for (const auto& otherName : gs.countryList) {
            if (otherName == countryName) continue;
            Country* other = gs.getCountry(otherName);
            if (!other || other->atWarWith.end() != std::find(other->atWarWith.begin(), other->atWarWith.end(), countryName)) {
                continue;
            }

            float available = other->resourceManager.stockpile[ri];
            if (available <= 150.0f) continue;
            float requestAmount = std::clamp(available * 0.08f, 8.0f, 24.0f);
            TradeApproval approval = evaluateTradeOffer(other, country, res, requestAmount, gs);
            if (!approval.approved) continue;
            float amount = std::min(requestAmount, approval.maxAmount);
            if (amount < 6.0f) continue;

            float score = amount;
            if (!other->faction.empty() && other->faction == country->faction) score += 8.0f;
            if (other->ideologyName == country->ideologyName) score += 4.0f;
            if (std::find(other->militaryAccess.begin(), other->militaryAccess.end(), countryName) != other->militaryAccess.end() ||
                std::find(country->militaryAccess.begin(), country->militaryAccess.end(), otherName) != country->militaryAccess.end()) {
                score += 3.0f;
            }

            if (score > bestScore) {
                bestScore = score;
                bestExporter = other;
                bestAmount = amount;
            }
        }

        if (!bestExporter) continue;

        TradeContract contract;
        contract.exporter = bestExporter->name;
        contract.importer = countryName;
        contract.resource = res;
        contract.amount = std::clamp(bestAmount, 6.0f, 24.0f);
        contract.price = 120.0f;
        gs.tradeContracts.push_back(contract);
    }
}

void AIController::manageFocus(GameState& gs) {
    Country* country = getCountry(gs);
    if (!country) return;
    if (country->focus.has_value()) return;

    auto& fte = country->focusTreeEngine;
    const FocusNode* chosenNode = nullptr;
    float bestScore = -1000.0f;

    for (auto& node : fte.nodes) {
        if (!fte.canStartFocus(node.name, country)) continue;

        float score = 0.0f;
        if (!country->canMakeFaction && country->faction.empty()) score += 6.0f;
        if (country->atWarWith.empty()) score += personality.economicFocus * 2.0f;
        else score += personality.aggressiveness * 2.0f;

        for (const auto& effect : node.effects) {
            if (const auto* s = std::get_if<std::string>(&effect)) {
                if (s->find("canMakeFaction") != std::string::npos || s->find("can_make_faction") != std::string::npos) {
                    score += country->faction.empty() ? 24.0f : 2.0f;
                }
                if (s->find("expandedInvitations") != std::string::npos || s->find("expanded_invitations") != std::string::npos) {
                    score += country->factionLeader ? 16.0f : 3.0f;
                }
                if (s->find("politicalPower") != std::string::npos) score += 6.0f;
                if (s->find("militarySize") != std::string::npos || s->find("attack") != std::string::npos) {
                    score += country->atWarWith.empty() ? 3.0f : 12.0f;
                }
                if (s->find("money") != std::string::npos || s->find("build") != std::string::npos) {
                    score += country->atWarWith.empty() ? 10.0f : 2.0f;
                }
            }
        }

        score -= static_cast<float>(node.cost) * 0.04f;
        score += randFloat(0.0f, 2.0f);

        if (score > bestScore) {
            bestScore = score;
            chosenNode = &node;
        }
    }

    if (!chosenNode) return;

    country->politicalPower -= static_cast<float>(chosenNode->cost);
    fte.completedFocuses.insert(chosenNode->name);

    std::vector<std::string> effectStrs;
    for (auto& eff : chosenNode->effects) {
        if (auto* s = std::get_if<std::string>(&eff)) effectStrs.push_back(*s);
    }
    country->focus = std::make_tuple(chosenNode->name, chosenNode->days, effectStrs);
}


float AIController::threatLevel(const std::string& otherCountry, GameState& gs) const {
    Country* country = getCountry(gs);
    Country* other = gs.getCountry(otherCountry);
    if (!country || !other) return 0.0f;

    float threat = 0.0f;


    for (auto& e : other->atWarWith) {
        if (e == countryName) { threat += 100.0f; break; }
    }


    float myDivs = static_cast<float>(country->divisions.size());
    float theirDivs = static_cast<float>(other->divisions.size());
    if (myDivs > 0) {
        threat += std::max(0.0f, (theirDivs - myDivs) / myDivs * 30.0f);
    }


    std::string myIdeology = getIdeologyName(country->ideology[0], country->ideology[1]);
    std::string theirIdeology = getIdeologyName(other->ideology[0], other->ideology[1]);
    if (myIdeology != theirIdeology) {
        threat += 10.0f;
    }

    return threat;
}

float AIController::allianceDesirability(const std::string& otherCountry, GameState& gs) const {
    Country* country = getCountry(gs);
    Country* other = gs.getCountry(otherCountry);
    if (!country || !other) return 0.0f;

    float score = 0.0f;


    std::string myIdeology = getIdeologyName(country->ideology[0], country->ideology[1]);
    std::string theirIdeology = getIdeologyName(other->ideology[0], other->ideology[1]);
    if (myIdeology == theirIdeology) score += 0.4f;


    for (auto& e : country->atWarWith) {
        for (auto& e2 : other->atWarWith) {
            if (e == e2) { score += 0.3f; break; }
        }
    }


    bool bordering = false;
    for (auto& b : country->bordering) {
        if (b == otherCountry) { bordering = true; break; }
    }
    if (!bordering) score += 0.1f;

    return std::min(1.0f, score);
}

bool AIController::canAffordWar(GameState& gs) const {
    Country* country = getCountry(gs);
    if (!country) return false;


    return country->divisions.size() >= 3 && country->money > 500000.0f;
}
