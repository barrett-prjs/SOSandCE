#include "game/game_state.h"
#include "game/country.h"
#include "game/division.h"
#include "game/faction.h"
#include "game/battle.h"
#include "game/puppet.h"
#include "game/economy.h"
#include "game/ai.h"
#include "game/fog_of_war.h"
#include "game/event_manager.h"
#include "game/politics.h"
#include "game/peace_conference.h"
#include "core/audio.h"
#include "core/engine.h"
#include "data/country_data.h"
#include "data/region_data.h"
#include "game/helpers.h"


GameState::GameState()
    : eventManager(std::make_unique<EventManager>()),
      polEventManager(std::make_unique<PoliticalEventManager>()),
      fogOfWar(std::make_unique<FogOfWar>())
{
    time = GameTime{};
    tick = 0;
    speed = 0;
    inGame = false;
    mapViewOnly = false;
    controlledCountry = "";
    mapName = "Modern Day";


    canals = {1802, 1868, 2567, 1244, 1212, 1485, 1502, 418};
}

GameState::~GameState() = default;


Country* GameState::getCountry(const std::string& name) {
    auto it = countries.find(name);
    if (it != countries.end()) {
        return it->second.get();
    }
    return nullptr;
}

const Country* GameState::getCountry(const std::string& name) const {
    auto it = countries.find(name);
    if (it != countries.end()) {
        return it->second.get();
    }
    return nullptr;
}

Faction* GameState::getFaction(const std::string& name) {
    auto it = factions.find(name);
    if (it != factions.end()) {
        return it->second.get();
    }
    return nullptr;
}

const Faction* GameState::getFaction(const std::string& name) const {
    auto it = factions.find(name);
    if (it != factions.end()) {
        return it->second.get();
    }
    return nullptr;
}

void GameState::registerCountry(const std::string& name, std::unique_ptr<Country> c) {
    countries[name] = std::move(c);

    if (std::find(countryList.begin(), countryList.end(), name) == countryList.end()) {
        countryList.push_back(name);
    }
    if (aiControllers.find(name) == aiControllers.end()) {
        aiControllers[name] = std::make_unique<AIController>(name, *this);
    }
}

void GameState::registerFaction(const std::string& name, std::unique_ptr<Faction> f) {
    factions[name] = std::move(f);
    if (std::find(factionList.begin(), factionList.end(), name) == factionList.end()) {
        factionList.push_back(name);
    }
}

void GameState::removeCountry(const std::string& name) {
    auto itCountry = countries.find(name);
    if (itCountry == countries.end()) return;

    Country* doomed = itCountry->second.get();
    auto eraseName = [&](std::vector<std::string>& values) {
        values.erase(std::remove(values.begin(), values.end(), name), values.end());
    };

    auto& rd = RegionData::instance();
    for (int rid : doomed->regions) {
        if (rd.getOwner(rid) == name) {
            rd.updateOwner(rid, "");
        }
    }

    if (pendingPeaceConference) {
        eraseName(pendingPeaceConference->winners);
        eraseName(pendingPeaceConference->losers);
        eraseName(pendingPeaceConference->originalWinners);
        eraseName(pendingPeaceConference->originalLosers);
        pendingPeaceConference->warScoreBudget.erase(name);
        pendingPeaceConference->demands.erase(
            std::remove_if(pendingPeaceConference->demands.begin(), pendingPeaceConference->demands.end(),
                [&](const PeaceDemand& demand) {
                    return demand.demander == name || demand.target == name || demand.releaseName == name;
                }),
            pendingPeaceConference->demands.end());
    }

    if (!doomed->faction.empty()) {
        if (auto* fac = getFaction(doomed->faction)) {
            fac->removeCountry(name, *this);
        }
    }

    for (auto& [otherName, other] : countries) {
        if (!other || otherName == name) continue;
        eraseName(other->atWarWith);
        eraseName(other->militaryAccess);
        if (other->lastAttackedBy == name) {
            other->lastAttackedBy.clear();
        }
        if (other->puppetTo == name) {
            other->puppetTo.clear();
        }
    }

    puppetStates.erase(
        std::remove_if(puppetStates.begin(), puppetStates.end(),
            [&](const PuppetState& ps) {
                return ps.overlord == name || ps.puppet == name;
            }),
        puppetStates.end());

    battles.erase(
        std::remove_if(battles.begin(), battles.end(),
            [&](const std::unique_ptr<Battle>& battle) {
                if (!battle) return true;
                if (!battle->attacker || !battle->defender) return true;
                return battle->attacker->country == name || battle->defender->country == name;
            }),
        battles.end());

    if (controlledCountry == name) {
        controlledCountry.clear();
    }

    countryList.erase(std::remove(countryList.begin(), countryList.end(), name), countryList.end());
    aiControllers.erase(name);

    if (!controlledCountry.empty()) {
        if (auto* player = getCountry(controlledCountry)) {
            if (player->atWarWith.empty()) {
                auto& audio = Audio::instance();
                if (audio.currentMusic.find("warMusic") != std::string::npos) {
                    std::string gamePath = Engine::instance().assetsPath + "music/gameMusic.mp3";
                    if (fs::exists(gamePath)) {
                        audio.playMusic(gamePath, true, 2000);
                    }
                }
            }
        }
    }

    countries.erase(itCountry);
}

void GameState::removeFaction(const std::string& name) {
    factions.erase(name);
    auto it = std::find(factionList.begin(), factionList.end(), name);
    if (it != factionList.end()) {
        factionList.erase(it);
    }
}


void GameState::clear() {
    countries.clear();
    factions.clear();
    battles.clear();
    countryList.clear();
    factionList.clear();
    tradeContracts.clear();
    puppetStates.clear();
    ports.clear();
    divisionsByRegion.clear();
    aiControllers.clear();

    time = GameTime{};
    tick = 0;
    speed = 0;
    inGame = false;
    mapViewOnly = false;
    controlledCountry = "";
    mapName = "Modern Day";
    pendingPeaceConference.reset();

    politicalMapSurf = nullptr;
    ideologyMapSurf = nullptr;
    factionMapSurf = nullptr;
    industryMapSurf = nullptr;
    cultureMapSurf = nullptr;
    regionsMapSurf = nullptr;

    canals = {1802, 1868, 2567, 1244, 1212, 1485, 1502, 418};

    eventManager = std::make_unique<EventManager>();
    polEventManager = std::make_unique<PoliticalEventManager>();
    fogOfWar = std::make_unique<FogOfWar>();
}

void GameState::updateTime() {


    time.hour += 1;

    if (time.hour > 24) {
        time.hour = 1;
        time.day += 1;
        time.totalDays += 1;

        int monthLength = getMonthLength(time.month);

        if (time.day > monthLength) {
            time.day = 1;
            time.month += 1;

            if (time.month > 12) {
                time.month = 1;
                time.year += 1;
            }
        }
    }
}

void GameState::tickAll() {
    if (mapViewOnly || speed == 0) return;

    float speedVal = static_cast<float>(speed);

    for (auto& [name, country] : countries) {
        if (!country) continue;
        country->resourceManager.tradeImports.fill(0.0f);
        country->resourceManager.tradeExports.fill(0.0f);
    }


    updateTime();


    std::vector<std::string> countryNames;
    countryNames.reserve(countries.size());
    for (auto& [n, c] : countries) {
        if (c) countryNames.push_back(n);
    }
    for (auto& cname : countryNames) {
        auto* c = getCountry(cname);
        if (c) c->update(*this);
    }


    std::vector<std::string> facNames(factionList.begin(), factionList.end());
    for (auto& fname : facNames) {
        auto* f = getFaction(fname);
        if (f) f->update(*this);
    }


    for (int i = static_cast<int>(battles.size()) - 1; i >= 0; --i) {
        if (i < static_cast<int>(battles.size()) && battles[i]) {
            battles[i]->update(*this);
        }
    }

    battles.erase(
        std::remove_if(battles.begin(), battles.end(),
            [](const std::unique_ptr<Battle>& b) { return !b || b->finished; }),
        battles.end());


    for (auto& tc : tradeContracts) {
        tc.update(*this);
    }

    tradeContracts.erase(
        std::remove_if(tradeContracts.begin(), tradeContracts.end(),
                        [](const TradeContract& tc) { return !tc.active; }),
        tradeContracts.end());


    for (auto& ps : puppetStates) {
        ps.update(*this);
    }

    puppetStates.erase(
        std::remove_if(puppetStates.begin(), puppetStates.end(),
                        [](const PuppetState& ps) { return !ps.active; }),
        puppetStates.end());


    for (auto& [name, ai] : aiControllers) {
        if (ai && name != controlledCountry) {
            ai->update(*this);
        }
    }


    if (time.hour == 1 && eventManager) {
        eventManager->update(*this);
    }


    if (polEventManager) {
        polEventManager->update(*this);
    }


    if (fogOfWar && !controlledCountry.empty()) {
        fogOfWar->recalculate(controlledCountry, *this);
    }


}

void GameState::spawnCountry(const std::string& name, const std::vector<int>& spawnRegions) {
    if (countries.count(name)) {

        auto* c = getCountry(name);
        if (c) c->addRegions(spawnRegions, *this);
    } else {
        auto& cd = CountryData::instance();
        std::vector<int> regs = spawnRegions;
        if (regs.empty()) regs = cd.getClaims(name);
        auto newC = std::make_unique<Country>(name, regs, *this);
        countries[name] = std::move(newC);
    }
}

void GameState::spawnFaction(const std::vector<std::string>& members, const std::string& facName) {
    std::string name = facName;
    if (name.empty() && !members.empty()) {
        name = getFactionName(members[0], *this);
    }

    std::replace(name.begin(), name.end(), ' ', '_');

    if (!factions.count(name)) {
        auto fac = std::make_unique<Faction>(name, members, *this);
        factions[name] = std::move(fac);
    }
}

void GameState::destroyCanal(const std::vector<int>& canalRegions) {
    for (int c : canalRegions) {
        auto it = std::find(canals.begin(), canals.end(), c);
        if (it != canals.end()) canals.erase(it);
    }
}

void GameState::peaceTreaty(const std::string& mainCountry) {
    Country* main = getCountry(mainCountry);
    if (!main) return;


    std::vector<std::string> combatants(main->atWarWith.begin(), main->atWarWith.end());


    std::vector<std::string> victors;
    for (auto& [cname, country] : countries) {
        if (!country) continue;
        for (auto& enemy : country->atWarWith) {
            if (std::find(combatants.begin(), combatants.end(), enemy) != combatants.end() &&
                std::find(combatants.begin(), combatants.end(), cname) == combatants.end()) {
                victors.push_back(cname);
                break;
            }
        }
    }

    std::vector<std::string> involved;
    involved.insert(involved.end(), victors.begin(), victors.end());
    involved.insert(involved.end(), combatants.begin(), combatants.end());


    auto& rd = RegionData::instance();
    for (auto& cname : involved) {
        auto* c = getCountry(cname);
        if (!c) continue;
        for (int r : c->regionsBeforeWar) {
            if (std::find(c->regions.begin(), c->regions.end(), r) == c->regions.end()) {
                std::string currentOwner = rd.getOwner(r);
                if (std::find(involved.begin(), involved.end(), currentOwner) != involved.end()) {
                    c->addRegion(r, *this);
                }
            }
        }
    }

    auto pickAiAnnexer = [&](const std::string& enemyName) -> Country* {
        Country* enemy = getCountry(enemyName);
        if (!enemy) return nullptr;

        Country* best = nullptr;
        int bestScore = std::numeric_limits<int>::min();
        for (const auto& victorName : victors) {
            if (victorName == controlledCountry) continue;
            Country* victor = getCountry(victorName);
            if (!victor) continue;
            if (std::find(victor->atWarWith.begin(), victor->atWarWith.end(), enemyName) == victor->atWarWith.end()) continue;

            int score = 0;
            if (!enemy->lastAttackedBy.empty() && enemy->lastAttackedBy == victorName) score += 2000;
            score += static_cast<int>(victor->regions.size()) * 5;
            score += static_cast<int>(victor->warScore);
            if (score > bestScore) {
                bestScore = score;
                best = victor;
            }
        }
        return best;
    };

    bool playerVictor = std::find(victors.begin(), victors.end(), controlledCountry) != victors.end();
    if (!playerVictor) {
        for (const auto& enemy : combatants) {
            Country* annexer = pickAiAnnexer(enemy);
            if (!annexer) continue;
            annexer->annexCountry(annexer->culture, *this, enemy);
        }
    }


    for (auto& victor : victors) {
        auto* v = getCountry(victor);
        if (!v) continue;
        for (auto& enemy : combatants) {
            v->makePeace(enemy, *this, false);
        }
    }


    if (playerVictor) {
        pendingPeaceConference = std::make_unique<PeaceConference>();
        pendingPeaceConference->start(victors, combatants, *this);
        return;
    }

    finalizePeaceResolution(victors, combatants);
}

void GameState::finalizePeaceResolution(const std::vector<std::string>& victors,
                                        const std::vector<std::string>& combatants) {
    std::vector<std::string> involved;
    involved.insert(involved.end(), victors.begin(), victors.end());
    involved.insert(involved.end(), combatants.begin(), combatants.end());

    auto& rd = RegionData::instance();
    std::vector<std::string> countriesToRemove;
    auto nearestOwnedRegion = [&](Country* c, int fromRegion) -> int {
        if (!c || c->regions.empty()) return -1;
        Vec2 fromLoc = rd.getLocation(fromRegion);
        int bestRegion = c->regions.front();
        float bestDist = std::numeric_limits<float>::max();
        for (int rid : c->regions) {
            Vec2 loc = rd.getLocation(rid);
            float dx = loc.x - fromLoc.x;
            float dy = loc.y - fromLoc.y;
            float distSq = dx * dx + dy * dy;
            if (distSq < bestDist) {
                bestDist = distSq;
                bestRegion = rid;
            }
        }
        return bestRegion;
    };


    for (auto& cname : involved) {
        auto* c = getCountry(cname);
        if (!c) continue;


        if (c->regions.empty()) {
            countriesToRemove.push_back(cname);
            continue;
        }


        if (c->capitulated) {
            c->capitulated = false;
            c->usedManPower = 0;
        }


        c->battleBorder = c->getBattleBorder(*this);
        c->checkBattleBorder = false;
        c->checkBordering = true;

        std::set<int> accessibleRegions;
        for (int rid : c->getAccess(*this)) {
            accessibleRegions.insert(rid);
        }


        for (auto& div : c->divisions) {
            if (!div) continue;

            div->commands.clear();
            div->fighting = false;
            div->locked = false;
            div->navalLocked = false;
            div->recovering = false;

            if (accessibleRegions.count(div->region) == 0) {
                int fallbackRegion = nearestOwnedRegion(c, div->region);
                if (fallbackRegion >= 0 && fallbackRegion != div->region) {
                    div->move(fallbackRegion, *this);
                }
            }
        }
    }

    for (const auto& cname : countriesToRemove) {
        removeCountry(cname);
    }


    for (int i = static_cast<int>(battles.size()) - 1; i >= 0; --i) {
        if (!battles[i]) continue;
        if (battles[i]->finished || !battles[i]->attacker || !battles[i]->defender) {
            battles.erase(battles.begin() + i);
            continue;
        }
        Division* attacker = battles[i]->attacker;
        Division* defender = battles[i]->defender;
        bool involved1 = attacker && std::find(involved.begin(), involved.end(), attacker->country) != involved.end();
        bool involved2 = defender && std::find(involved.begin(), involved.end(), defender->country) != involved.end();
        if (involved1 || involved2) {
            if (attacker) attacker->fighting = false;
            if (defender) defender->fighting = false;
            battles.erase(battles.begin() + i);
        }
    }
}
