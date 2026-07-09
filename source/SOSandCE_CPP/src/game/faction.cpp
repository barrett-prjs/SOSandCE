#include "game/faction.h"
#include "game/game_state.h"
#include "game/country.h"
#include "game/helpers.h"
#include "data/country_data.h"
#include "data/region_data.h"

Faction::Faction(const std::string& name, const std::vector<std::string>& countriesInFaction, GameState& gs)
    : name(name), gs_(gs)
{
    auto& cd = CountryData::instance();


    std::string validLeader;
    for (auto& c : countriesInFaction) {
        if (gs.getCountry(c)) { validLeader = c; break; }
    }

    if (validLeader.empty()) {
        color = {128, 128, 128};
        ideology = "nonaligned";
        factionLeader = countriesInFaction.empty() ? "" : countriesInFaction[0];
        flag = name;
        return;
    }

    auto* leaderCountry = gs.getCountry(validLeader);
    color = cd.getColor(validLeader);
    ideology = leaderCountry ? Helpers::getIdeologyName(leaderCountry->ideology[0], leaderCountry->ideology[1]) : "nonaligned";
    factionLeader = validLeader;
    if (leaderCountry) leaderCountry->factionLeader = true;
    flag = validLeader;

    for (auto& c : countriesInFaction) {
        if (gs.getCountry(c)) addCountry(c, gs, false, false);
    }
}

Faction::~Faction() = default;

void Faction::update(GameState& gs) {
    if (std::find(gs.countryList.begin(), gs.countryList.end(), factionLeader) == gs.countryList.end()) {

        std::string largest;
        int maxRegions = 0;
        for (auto& m : members) {
            auto* c = gs.getCountry(m);
            if (c && static_cast<int>(c->regions.size()) > maxRegions) {
                largest = m;
                maxRegions = static_cast<int>(c->regions.size());
            }
        }
        if (!largest.empty()) {
            factionLeader = largest;
            auto* c = gs.getCountry(largest);
            if (c) c->factionLeader = true;
        }
    }
    if (members.empty()) kill(gs);
}

void Faction::addCountry(const std::string& country, GameState& gs, bool sendPrompt, bool createPopup) {
    auto* c = gs.getCountry(country);
    if (!c) return;


    if (!c->faction.empty() && c->faction != name) {
        auto* oldFaction = gs.getFaction(c->faction);
        if (oldFaction) oldFaction->removeCountry(country, gs);
    }

    c->faction = name;
    c->factionColor = color;
    if (std::find(members.begin(), members.end(), country) == members.end()) {
        members.push_back(country);
    }


    if (!factionWar.empty()) {

        for (auto& member : members) {
            if (std::find(c->militaryAccess.begin(), c->militaryAccess.end(), member) == c->militaryAccess.end()) {
                c->militaryAccess.push_back(member);
            }
        }
    }

    for (auto& fw : factionWar) {
        auto* enemyFaction = gs.getFaction(fw);
        if (enemyFaction) {
            for (auto& enemy : enemyFaction->members) {
                c->declareWar(enemy, gs, true, false);
            }
        }
    }


    reloadDivisionColors(gs);
}

void Faction::removeCountry(const std::string& country, GameState& gs, bool ignoreFill, bool doPopup) {
    auto it = std::find(members.begin(), members.end(), country);
    if (it == members.end()) return;

    auto* c = gs.getCountry(country);
    if (c) {
        c->factionColor = {255, 255, 255};
        c->faction.clear();
    }

    members.erase(it);

    if (members.empty()) kill(gs);
}

void Faction::declareWar(const std::string& faction, GameState& gs) {
    auto* enemyFaction = gs.getFaction(faction);
    if (!enemyFaction) return;

    if (std::find(factionWar.begin(), factionWar.end(), faction) == factionWar.end()) {
        factionWar.push_back(faction);
    }
    if (std::find(enemyFaction->factionWar.begin(), enemyFaction->factionWar.end(), name) == enemyFaction->factionWar.end()) {
        enemyFaction->factionWar.push_back(name);
    }


    std::vector<std::string> myMembers(members);
    std::vector<std::string> enemyMembers(enemyFaction->members);


    for (auto& m1 : myMembers) {
        for (auto& m2 : myMembers) {
            auto* c2 = gs.getCountry(m2);
            if (c2 && std::find(c2->militaryAccess.begin(), c2->militaryAccess.end(), m1) == c2->militaryAccess.end()) {
                c2->militaryAccess.push_back(m1);
            }
        }
    }
    for (auto& m1 : enemyMembers) {
        for (auto& m2 : enemyMembers) {
            auto* c2 = gs.getCountry(m2);
            if (c2 && std::find(c2->militaryAccess.begin(), c2->militaryAccess.end(), m1) == c2->militaryAccess.end()) {
                c2->militaryAccess.push_back(m1);
            }
        }
    }


    for (auto& m1 : myMembers) {
        for (auto& enemy : enemyMembers) {
            auto* c1 = gs.getCountry(m1);
            if (c1) c1->declareWar(enemy, gs, true, false);
        }
    }
}

void Faction::reloadDivisionColors(GameState& gs) {
    for (auto& m : members) {
        auto* c = gs.getCountry(m);
        if (!c) continue;
        if (std::find(members.begin(), members.end(), gs.controlledCountry) != members.end() && m != gs.controlledCountry) {
            c->resetDivColor({0, 0, 255});
        } else if (m != gs.controlledCountry) {
            c->resetDivColor({0, 0, 0});
        }
    }
}

std::vector<int> Faction::getBattleBorder(GameState& gs) const {
    auto& rd = RegionData::instance();
    std::set<std::string> enemies;
    std::set<int> totalRegions;

    for (auto& m : members) {
        auto* c = gs.getCountry(m);
        if (!c) continue;
        for (auto& e : c->atWarWith) enemies.insert(e);
        for (int r : c->regions) totalRegions.insert(r);
    }

    std::vector<int> border;
    for (int r : totalRegions) {
        for (int conn : rd.getConnections(r)) {
            if (enemies.count(rd.getOwner(conn))) {
                border.push_back(r);
                break;
            }
        }
    }
    return border;
}

void Faction::kill(GameState& gs) {
    for (auto& m : std::vector<std::string>(members)) {
        removeCountry(m, gs);
    }
    auto it = std::find(gs.factionList.begin(), gs.factionList.end(), name);
    if (it != gs.factionList.end()) gs.factionList.erase(it);
}
