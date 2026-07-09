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

int coverageCounters(const Country* country, int region) {
    if (!country) return 0;
    int n = 0;
    for (const auto& d : country->divisions) {
        if (!d) continue;
        int dest = d->aiObjectiveRegion >= 0 ? d->aiObjectiveRegion
                   : (!d->commands.empty() ? d->commands.back() : d->region);
        if (d->region == region || dest == region) n++;
    }
    return n;
}

bool mergeIdleClusters(Country* country, GameState& gs) {
    if (!country) return false;

    std::unordered_map<int, std::vector<Division*>> byRegion;
    for (auto& div : country->divisions) {
        if (!div || div->fighting || div->locked || div->recovering || !div->commands.empty()) continue;
        byRegion[div->region].push_back(div.get());
    }

    for (auto& [region, divs] : byRegion) {
        if (divs.size() < 2) continue;
        bool border = containsRegion(country->battleBorder, region);
        int keep = border ? 2 : 1;
        if (static_cast<int>(divs.size()) > keep) {
            country->mergeDivisions(gs, {divs[0], divs[1]});
            return true;
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
        manageNavalInvasions(gs);
        considerMakePeace(gs);
    } else {
        navalTargets.clear();
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

    threateningNeighbors.clear();
    potentialAllies.clear();

    std::string myIdeology = getIdeologyName(country->ideology[0], country->ideology[1]);
    for (auto& neighbor : country->bordering) {
        if (neighbor == countryName) continue;

        Country* n = gs.getCountry(neighbor);
        if (!n) continue;

        float threat = threatLevel(neighbor, gs);

        if (threat > 50.0f) {
            threateningNeighbors.push_back(neighbor);
        }

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

    mergeIdleClusters(country, gs);

    for (auto& div : country->divisions) {
        if (!div) continue;
        if (div->fighting || div->locked || div->recovering) continue;
        if (!div->commands.empty()) continue;
        if (recentlyOrdered(div.get(), gs, 4)) continue;

        microDivision(div.get(), gs);
    }
}

void AIController::assignFrontline(GameState& gs) {
    Country* country = getCountry(gs);
    if (!country) return;

    if (country->atWarWith.empty()) return;
    if (country->battleBorder.empty()) return;

    const auto& border = country->battleBorder;

    // Stable garrison target per front region. Enemy pressure only ever RAISES
    // the target (reinforce under attack) and never lowers it below 1, so the
    // desired count doesn't flip up and down as enemy stacks shuffle around.
    auto desiredFor = [&](int region) {
        int desired = 1;
        if (enemyPressureAgainstRegion(gs, country, region) >= 3) desired += 1;
        if (isCapitalRegion(country, region)) desired += 1;
        return desired;
    };

    std::unordered_map<int, int> cover;
    for (int region : border) cover[region] = coverageCounters(country, region);

    // The reserve pool is only divisions that are NOT already usefully holding
    // the line. A division standing on (or moving to) a front region that still
    // wants it is a holder and is left completely alone — that stickiness is
    // what stops the line from constantly reshuffling. Recently-ordered units
    // are also skipped so we never re-task a division mid-repositioning.
    auto isHolder = [&](Division* d) {
        if (!containsRegion(border, d->region)) return false;
        return cover[d->region] <= desiredFor(d->region);
    };

    auto rebuildReserve = [&](std::vector<Division*>& out) {
        out.clear();
        for (int region : border) cover[region] = coverageCounters(country, region);
        for (auto& d : country->divisions) {
            if (!d || d->fighting || d->locked || d->recovering) continue;
            if (!d->commands.empty()) continue;
            if (recentlyOrdered(d.get(), gs, 12)) continue;
            if (isHolder(d.get())) continue;
            out.push_back(d.get());
        }
    };

    std::vector<Division*> reserve;
    rebuildReserve(reserve);

    // Only genuinely under-manned front regions are "needs". A region already
    // at its desired garrison generates nothing to do, so satisfied stretches
    // of the line stay quiet.
    struct Need { int region; float priority; int desired; };
    auto rebuildNeeds = [&](std::vector<Need>& needs) {
        needs.clear();
        for (int region : border) {
            int desired = desiredFor(region);
            if (cover[region] >= desired) continue;
            int pressure = enemyPressureAgainstRegion(gs, country, region);
            float priority = pressure * 5.0f;
            if (cover[region] == 0) priority += 100.0f;
            if (isCapitalRegion(country, region)) priority += 25.0f;
            else if (isStrategicCity(region)) priority += 12.0f;
            needs.push_back({region, priority, desired});
        }
        std::sort(needs.begin(), needs.end(), [](const Need& a, const Need& b) {
            return a.priority > b.priority;
        });
    };

    std::vector<Need> needs;
    rebuildNeeds(needs);

    auto unmetGaps = [&]() {
        int u = 0;
        for (const auto& n : needs) u += std::max(0, n.desired - cover[n.region]);
        return u;
    };

    // Split a big idle division only as a last resort — when we genuinely lack
    // the bodies to man every open gap.
    int splitsThisPass = 0;
    while (unmetGaps() > static_cast<int>(reserve.size()) && splitsThisPass < 6) {
        Division* biggest = nullptr;
        for (auto& d : country->divisions) {
            if (!d || d->fighting || d->locked || d->recovering) continue;
            if (!d->commands.empty()) continue;
            if (d->divisionStack < 2) continue;
            if (!biggest || d->divisionStack > biggest->divisionStack) biggest = d.get();
        }
        if (!biggest) break;
        country->divideDivision(gs, biggest);
        splitsThisPass++;
        rebuildReserve(reserve);
        rebuildNeeds(needs);
    }

    // Fill each gap with the single nearest reserve division. Nothing already
    // holding the line is touched — only reserves flow toward real gaps.
    for (auto& need : needs) {
        while (cover[need.region] < need.desired && !reserve.empty()) {
            auto bestIt = std::min_element(reserve.begin(), reserve.end(), [&](Division* a, Division* b) {
                return regionDistanceScore(a->region, need.region) < regionDistanceScore(b->region, need.region);
            });
            if (bestIt == reserve.end()) break;
            Division* d = *bestIt;
            if (d->region != need.region) {
                issueAiOrder(d, need.region, gs, true, true, 400);
            } else {
                d->aiObjectiveRegion = need.region;
                d->lastAIOrderHour = currentHourStamp(gs);
            }
            cover[need.region]++;
            reserve.erase(bestIt);
        }
    }

    // Bring rear reserves up to the line ONCE. A reserve already sitting on a
    // front region is left in place (never shuffled sideways to a "nearer"
    // region — that lateral churn is exactly what looked so messy). The 12h
    // order cooldown above guarantees this is a one-way trickle, not a per-tick
    // stampede.
    for (Division* d : reserve) {
        if (containsRegion(border, d->region)) continue;
        int nearest = -1;
        float best = std::numeric_limits<float>::max();
        for (int region : border) {
            float dist = regionDistanceScore(d->region, region);
            if (dist < best) { best = dist; nearest = region; }
        }
        if (nearest >= 0 && nearest != d->region) {
            issueAiOrder(d, nearest, gs, true, true, 400);
        }
    }
}

void AIController::manageNavalInvasions(GameState& gs) {
    Country* country = getCountry(gs);
    if (!country || country->atWarWith.empty()) return;
    if (!country->battleBorder.empty()) return;

    auto& rd = RegionData::instance();

    navalTargets.erase(
        std::remove_if(navalTargets.begin(), navalTargets.end(), [&](int reg) {
            return std::find(country->atWarWith.begin(), country->atWarWith.end(),
                             rd.getOwner(reg)) == country->atWarWith.end();
        }),
        navalTargets.end());

    if (navalTargets.empty()) {
        for (auto& enemy : country->atWarWith) {
            Country* ec = gs.getCountry(enemy);
            if (!ec) continue;
            for (int reg : ec->regions) {
                for (int conn : rd.getConnections(reg)) {
                    if (rd.getOwner(conn).empty() && rd.getPopulation(conn) == 0) {
                        navalTargets.push_back(reg);
                        break;
                    }
                }
            }
        }
        if (navalTargets.empty()) return;
    }

    for (auto& div : country->divisions) {
        if (!div || div->fighting || div->locked || div->navalLocked) continue;
        if (!div->commands.empty()) continue;
        if (recentlyOrdered(div.get(), gs, 24)) continue;
        if (div->maxUnits > 0 && div->units / div->maxUnits < 0.6f) continue;

        int target = navalTargets[randInt(0, static_cast<int>(navalTargets.size()) - 1)];
        div->command(target, gs, false, false, 400);
        if (div->commands.empty()) {
            div->attempts++;
            if (div->attempts > 5) div->navalLocked = true;
        } else {
            div->lastAIOrderHour = currentHourStamp(gs);
            div->aiObjectiveRegion = target;
        }
    }
}

void AIController::manageDivisionTraining(GameState& gs) {
    Country* country = getCountry(gs);
    if (!country) return;

    for (int i = static_cast<int>(country->training.size()) - 1; i >= 0; --i) {
        if (country->training[i][1] <= 0) {
            country->deployTrainingDivision(gs, i);
        }
    }

    if (atWar && country->manPower < 10000.0f && country->militarySize < 4) {
        country->militarySize++;
    }

    int committedStacks = 0;
    for (auto& div : country->divisions) {
        if (div) committedStacks += std::max(1, div->divisionStack);
    }
    int trainQueueStacks = 0;
    for (const auto& entry : country->training) {
        trainQueueStacks += entry[0];
    }

    int peacetimeTarget = std::clamp(static_cast<int>(country->regions.size()) / 2, 4, 60);
    int enemyTotal = 0;
    for (const auto& enemyName : country->atWarWith) {
        Country* ec = gs.getCountry(enemyName);
        if (!ec) continue;
        for (auto& d : ec->divisions) {
            if (d) enemyTotal += std::max(1, d->divisionStack);
        }
    }
    int wartimeTarget = std::max(static_cast<int>(country->battleBorder.size()) * 2 + 4,
                                 std::min(enemyTotal + 4, 120));
    int desiredStacks = atWar ? std::max(peacetimeTarget, wartimeTarget) : peacetimeTarget;
    int maxQueue = atWar ? 4 : 1;

    if (country->manPower >= 10000.0f &&
        trainQueueStacks < maxQueue &&
        committedStacks + trainQueueStacks < desiredStacks) {
        country->trainDivision(gs, 1);
    }
}

void AIController::microDivision(Division* div, GameState& gs) {
    if (!div) return;

    Country* country = getCountry(gs);
    if (!country) return;

    if (shouldRetreat(div, gs)) {
        int safe = findSafeRegion(div, gs);
        if (safe >= 0 && safe != div->region) {
            issueAiOrder(div, safe, gs, true, true, 100);
        }
        return;
    }

    if (!atWar) return;
    if (!containsRegion(country->battleBorder, div->region)) return;
    if (div->maxUnits > 0 && div->units / div->maxUnits < 0.5f) return;
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

    int R = div->region;
    std::vector<int> hostiles;
    for (int conn : rd.getConnections(R)) {
        if (std::find(country->atWarWith.begin(), country->atWarWith.end(), rd.getOwner(conn)) !=
            country->atWarWith.end()) {
            hostiles.push_back(conn);
        }
    }
    if (hostiles.empty()) return -1;

    int myStack = std::max(1, div->divisionStack);
    int heldHere = committedFriendlyStacks(gs, country, R);
    bool multiThreat = hostiles.size() >= 2;
    bool canLeave = !multiThreat || heldHere > myStack;
    if (!canLeave) return -1;

    std::vector<int> caps;
    for (const auto& en : country->atWarWith) {
        Country* ec = gs.getCountry(en);
        if (ec && !ec->capital.empty()) {
            int cap = rd.getCityRegion(ec->capital);
            if (cap >= 0) caps.push_back(cap);
        }
    }
    auto capitalBias = [&](int reg) -> float {
        if (caps.empty()) return 0.0f;
        float best = std::numeric_limits<float>::max();
        for (int cap : caps) best = std::min(best, regionDistanceScore(reg, cap));
        return -best / 300000.0f;
    };

    int bestTarget = -1;
    float bestScore = -1e9f;
    for (int T : hostiles) {
        int enemyStack = countEnemyStacks(gs, country, T);
        int support = countFriendlySupportForTarget(gs, country, T);
        float attack = static_cast<float>(myStack) + support * 0.5f;

        float score;
        if (enemyStack == 0) {
            score = 120.0f;
        } else if (attack >= static_cast<float>(enemyStack)) {
            score = 45.0f + (attack - enemyStack) * 6.0f;
        } else {
            continue;
        }

        Country* enemyCountry = gs.getCountry(rd.getOwner(T));
        if (isCapitalRegion(enemyCountry, T)) score += 30.0f;
        else if (isStrategicCity(T)) score += 12.0f;
        if (enemyCountry && enemyCountry->regions.size() <= 4) score += 8.0f;
        score += capitalBias(T);
        score += randFloat(0.0f, 1.5f);

        if (score > bestScore) {
            bestScore = score;
            bestTarget = T;
        }
    }

    return bestTarget;
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
    if (country->capitulated) return;

    float myStrength = 0.0f;
    for (auto& div : country->divisions) {
        if (div) myStrength += div->divisionStack;
    }

    bool crippled = myStrength < 1.0f && country->regions.size() < 3;
    bool losingBadly = !country->regionsBeforeWar.empty() &&
                       country->regions.size() * 3 < country->regionsBeforeWar.size() &&
                       myStrength < 3.0f;
    if (!crippled && !losingBadly) return;

    std::vector<std::string> enemiesNow = country->atWarWith;
    for (auto& enemy : enemiesNow) {
        if (enemy == gs.controlledCountry) continue;
        country->makePeace(enemy, gs, false);
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

    auto& rd = RegionData::instance();
    bool hasPort = false;
    for (int p : gs.ports) {
        if (std::find(country->regions.begin(), country->regions.end(), p) != country->regions.end()) {
            hasPort = true;
            break;
        }
    }
    bool portQueued = false;
    for (const auto& entry : bm.constructionQueue) {
        if (entry.type == BuildingType::Dockyard || entry.type == BuildingType::Port) {
            portQueued = true;
            break;
        }
    }
    if (!hasPort && !portQueued) {
        for (int r : country->regions) {
            bool coastal = false;
            for (int conn : rd.getConnections(r)) {
                if (rd.getOwner(conn).empty() && rd.getPopulation(conn) == 0) {
                    coastal = true;
                    break;
                }
            }
            if (coastal && bm.canBuild(r, BuildingType::Dockyard)) {
                if (bm.startConstruction(r, "dockyard", country)) return;
            }
        }
    }

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
