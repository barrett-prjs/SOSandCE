#include "game/politics.h"
#include "game/game_state.h"
#include "game/country.h"
#include "game/helpers.h"
#include "data/data_loader.h"


static const std::unordered_map<std::string, std::string> CULTURE_TO_REGION = {
    {"Russian", "Eastern_European"}, {"Ukrainian", "Eastern_European"},
    {"Polish", "Eastern_European"}, {"Czech", "Eastern_European"},
    {"Serbian", "Eastern_European"}, {"Romanian", "Eastern_European"},
    {"Hungarian", "Eastern_European"}, {"Bulgarian", "Eastern_European"},
    {"French", "Western_European"}, {"German", "Western_European"},
    {"British", "Western_European"}, {"Dutch", "Western_European"},
    {"Belgian", "Western_European"}, {"Austrian", "Western_European"},
    {"Swiss", "Western_European"},
    {"Italian", "Southern_European"}, {"Spanish", "Southern_European"},
    {"Portuguese", "Southern_European"}, {"Greek", "Southern_European"},
    {"Swedish", "Nordic"}, {"Norwegian", "Nordic"},
    {"Danish", "Nordic"}, {"Finnish", "Nordic"},
    {"Chinese", "East_Asian"}, {"Japanese", "East_Asian"},
    {"Korean", "East_Asian"},
    {"Vietnamese", "Southeast_Asian"}, {"Thai", "Southeast_Asian"},
    {"Indonesian", "Southeast_Asian"}, {"Filipino", "Southeast_Asian"},
    {"Indian", "South_Asian"}, {"Pakistani", "South_Asian"},
    {"Bengali", "South_Asian"},
    {"Turkish", "Middle_Eastern"}, {"Persian", "Middle_Eastern"},
    {"Arabian", "Middle_Eastern"}, {"Egyptian", "Middle_Eastern"},
    {"Iraqi", "Middle_Eastern"},
    {"Nigerian", "African"}, {"Ethiopian", "African"},
    {"South_African", "African"}, {"Congolese", "African"},
    {"Kenyan", "African"},
    {"American", "North_American"}, {"Canadian", "North_American"},
    {"Mexican", "Latin_American"}, {"Brazilian", "Latin_American"},
    {"Argentinian", "Latin_American"}, {"Colombian", "Latin_American"},
    {"Chilean", "Latin_American"}, {"Cuban", "Latin_American"},
};

static const std::vector<std::string> FALLBACK_FIRST = {
    "Alex", "Jordan", "Morgan", "Taylor", "Casey", "Robin", "Kim", "Lee"
};
static const std::vector<std::string> FALLBACK_LAST = {
    "Smith", "Jones", "Brown", "Wilson", "Clark", "Hall", "King", "Wright"
};

struct TraitDef {
    std::string name;
    float economy;
    float military;
    float diplomacy;
    float stability;
};

static const std::vector<TraitDef> TRAIT_POOL = {
    {"Industrialist",   0.10f,  0.0f,   0.0f,   0.0f},
    {"War Hawk",        0.0f,   0.15f, -0.05f, -0.05f},
    {"Diplomat",        0.0f,   0.0f,   0.15f,  0.05f},
    {"Populist",       -0.05f,  0.0f,   0.0f,   0.15f},
    {"Reformer",        0.05f,  0.0f,   0.05f,  0.05f},
    {"Authoritarian",   0.0f,   0.10f, -0.10f,  0.10f},
    {"Free Trader",     0.15f, -0.05f,  0.05f,  0.0f},
    {"Isolationist",    0.05f,  0.05f, -0.15f,  0.10f},
    {"Visionary",       0.05f,  0.05f,  0.05f,  0.0f},
    {"Corrupt",        -0.10f,  0.0f,   0.0f,  -0.10f},
};


static json& leaderNamesData() {
    static json data = DataLoader::getLeaderNames();
    return data;
}

static std::pair<std::vector<std::string>, std::vector<std::string>> getNamePool(const std::string& culture) {
    auto& data = leaderNamesData();


    auto regionIt = CULTURE_TO_REGION.find(culture);
    std::string region = regionIt != CULTURE_TO_REGION.end() ? regionIt->second : "";


    for (const auto& key : {region, culture}) {
        if (key.empty()) continue;
        if (data.contains(key) && data[key].is_object()) {
            auto& pool = data[key];
            std::vector<std::string> firsts, lasts;
            if (pool.contains("first_names"))
                firsts = pool["first_names"].get<std::vector<std::string>>();
            else if (pool.contains("first"))
                firsts = pool["first"].get<std::vector<std::string>>();
            if (pool.contains("last_names"))
                lasts = pool["last_names"].get<std::vector<std::string>>();
            else if (pool.contains("last"))
                lasts = pool["last"].get<std::vector<std::string>>();
            if (!firsts.empty() && !lasts.empty()) return {firsts, lasts};
        }
    }
    return {FALLBACK_FIRST, FALLBACK_LAST};
}

static std::string generateName(const std::string& culture) {
    auto [firsts, lasts] = getNamePool(culture);
    return firsts[randInt(0, static_cast<int>(firsts.size()) - 1)] + " " +
           lasts[randInt(0, static_cast<int>(lasts.size()) - 1)];
}


std::string Leader::getFullTitle() const {
    if (title.empty()) return name;
    return title + " " + name;
}


CabinetMember* Cabinet::getByRole(const std::string& role) {
    for (auto& m : members) {
        if (m.role == role) return &m;
    }
    return nullptr;
}

void Cabinet::add(const CabinetMember& m) {

    for (auto& existing : members) {
        if (existing.role == m.role) {
            existing = m;
            return;
        }
    }
    members.push_back(m);
}

void Cabinet::remove(const std::string& role) {
    members.erase(
        std::remove_if(members.begin(), members.end(),
                        [&](const CabinetMember& m) { return m.role == role; }),
        members.end());
}

void Cabinet::clear() {
    members.clear();
}

float Cabinet::getTotalSkill() const {
    float total = 0.0f;
    for (auto& m : members) {
        total += m.skill;
    }
    return total;
}


ElectionSystem::ElectionSystem() {

    parties.push_back({"Democratic Party",  "liberal",     0.30f, {154, 237, 151}});
    parties.push_back({"Communist Party",   "communist",   0.15f, {255, 117, 117}});
    parties.push_back({"Nationalist Party", "nationalist", 0.10f, {66, 170, 255}});
    parties.push_back({"Monarchist Party",  "monarchist",  0.15f, {192, 154, 236}});
    parties.push_back({"Independent",       "nonaligned",  0.30f, {255, 255, 255}});
}

void ElectionSystem::update(GameState& gs, const std::string& countryName) {
    if (!electionsEnabled) return;

    int currentMonth = gs.time.year * 12 + gs.time.month;
    if (lastProcessedMonth < 0) {
        lastProcessedMonth = currentMonth;
        return;
    }
    if (currentMonth == lastProcessedMonth) return;

    int monthDelta = std::max(1, currentMonth - lastProcessedMonth);
    monthsSinceElection += monthDelta;
    lastProcessedMonth = currentMonth;

    if (monthsSinceElection >= electionFrequency) {
        holdElection(gs, countryName);
    }
}

void ElectionSystem::holdElection(GameState& gs, const std::string& countryName) {
    monthsSinceElection = 0;

    PoliticalParty* winner = getLeadingParty();
    if (!winner) return;

    Country* country = gs.getCountry(countryName);
    if (!country) return;


    country->leader = generateLeader(country->culture, winner->ideology);
    country->leader.party = winner->name;
}

void ElectionSystem::addParty(const PoliticalParty& party) {
    parties.push_back(party);
}

void ElectionSystem::removeParty(const std::string& name) {
    parties.erase(
        std::remove_if(parties.begin(), parties.end(),
                        [&](const PoliticalParty& p) { return p.name == name; }),
        parties.end());
    normalizeSupport();
}

PoliticalParty* ElectionSystem::getLeadingParty() {
    if (parties.empty()) return nullptr;

    PoliticalParty* best = &parties[0];
    for (auto& p : parties) {
        if (p.support > best->support) {
            best = &p;
        }
    }
    return best;
}

PoliticalParty* ElectionSystem::getPartyByName(const std::string& name) {
    for (auto& p : parties) {
        if (p.name == name) return &p;
    }
    return nullptr;
}

void ElectionSystem::shiftSupport(const std::string& partyName, float delta) {
    for (auto& p : parties) {
        if (p.name == partyName) {
            p.support = std::max(0.0f, p.support + delta);
            break;
        }
    }
    normalizeSupport();
}

void ElectionSystem::normalizeSupport() {
    float total = 0.0f;
    for (auto& p : parties) total += p.support;
    if (total > 0.0f) {
        for (auto& p : parties) p.support /= total;
    }
}


PoliticalEventManager::PoliticalEventManager() = default;
PoliticalEventManager::~PoliticalEventManager() = default;

void PoliticalEventManager::update(GameState& gs) {
    if (lastProcessedDay < 0) {
        lastProcessedDay = gs.time.totalDays;
        return;
    }
    if (gs.time.totalDays == lastProcessedDay) return;

    int dayDelta = std::max(1, gs.time.totalDays - lastProcessedDay);
    lastProcessedDay = gs.time.totalDays;


    for (auto& [key, val] : eventCooldowns) {
        if (val > 0.0f) val = std::max(0.0f, val - static_cast<float>(dayDelta));
    }

    for (auto& [name, country] : gs.countries) {
        if (!country) continue;
        if (name == gs.controlledCountry) continue;

        checkLeaderDeath(name, gs);
        checkCoup(name, gs);
        checkRevolution(name, gs);
        checkScandal(name, gs);
        checkEconomicShock(name, gs);
        checkMilitaryIncident(name, gs);
        checkPolicyChange(name, gs);
    }
}

void PoliticalEventManager::fireEvent(const std::string& ,
                                       const std::string& ,
                                       GameState& ) {

}

void PoliticalEventManager::checkCoup(const std::string& countryName, GameState& gs) {
    auto it = eventCooldowns.find("coup_" + countryName);
    if (it != eventCooldowns.end() && it->second > 0.0f) return;

    Country* c = gs.getCountry(countryName);
    if (!c) return;


    float chance = std::max(0.0f, (30.0f - c->stability) * 0.001f);
    if (randFloat(0.0f, 1.0f) < chance) {
        eventCooldowns["coup_" + countryName] = 365.0f;

        c->leader.name = generateName(c->culture);
        c->stability = std::max(0.0f, c->stability - 20.0f);
    }
}

void PoliticalEventManager::checkRevolution(const std::string& countryName, GameState& gs) {
    auto it = eventCooldowns.find("revolution_" + countryName);
    if (it != eventCooldowns.end() && it->second > 0.0f) return;

    Country* c = gs.getCountry(countryName);
    if (!c) return;

    float chance = std::max(0.0f, (20.0f - c->stability) * 0.0005f);
    if (randFloat(0.0f, 1.0f) < chance) {
        eventCooldowns["revolution_" + countryName] = 720.0f;

        c->ideology[0] = -c->ideology[0];
        c->ideology[1] = -c->ideology[1];
        c->ideologyName = getIdeologyName(c->ideology[0], c->ideology[1]);
        c->stability = std::max(0.0f, c->stability - 30.0f);
    }
}

void PoliticalEventManager::checkElection(const std::string& countryName, GameState& gs) {
    (void)countryName;
    (void)gs;

}

void PoliticalEventManager::checkPolicyChange(const std::string& ,
                                                GameState& ) {

}

void PoliticalEventManager::checkLeaderDeath(const std::string& countryName, GameState& gs) {
    auto it = eventCooldowns.find("leader_death_" + countryName);
    if (it != eventCooldowns.end() && it->second > 0.0f) return;

    Country* c = gs.getCountry(countryName);
    if (!c || !c->leader.isAlive) return;

    float chance = std::max(0.0f, (static_cast<float>(c->leader.age) - 55.0f) * 0.002f);
    if (randFloat(0.0f, 1.0f) < chance) {
        eventCooldowns["leader_death_" + countryName] = 365.0f;


        Leader newLeader;
        newLeader.name = generateName(c->culture);
        newLeader.ideology = c->leader.ideology;
        newLeader.age = randInt(35, 65);
        newLeader.isAlive = true;

        c->leader = generateLeader(c->culture, c->leader.ideology);
        c->stability = std::max(0.0f, c->stability - 10.0f);
    }
}


void PoliticalEventManager::checkScandal(const std::string& countryName, GameState& gs) {
    auto it = eventCooldowns.find("scandal_" + countryName);
    if (it != eventCooldowns.end() && it->second > 0.0f) return;

    Country* c = gs.getCountry(countryName);
    if (!c) return;

    float chance = std::max(0.0f, (50.0f - c->stability) * 0.003f);
    if (randFloat(0.0f, 1.0f) < chance) {
        eventCooldowns["scandal_" + countryName] = 180.0f;
        bool major = randFloat(0.0f, 1.0f) > 0.5f;
        float stabHit = major ? -15.0f : -5.0f;
        float ppHit = major ? -30.0f : -10.0f;
        c->stability = std::max(0.0f, c->stability + stabHit);
        c->politicalPower = std::max(0.0f, c->politicalPower + ppHit);
    }
}


void PoliticalEventManager::checkEconomicShock(const std::string& countryName, GameState& gs) {
    auto it = eventCooldowns.find("economic_shock_" + countryName);
    if (it != eventCooldowns.end() && it->second > 0.0f) return;

    Country* c = gs.getCountry(countryName);
    if (!c) return;

    float chance = std::max(0.0f, 0.005f - c->money * 0.000002f);
    if (randFloat(0.0f, 1.0f) < chance) {
        eventCooldowns["economic_shock_" + countryName] = 360.0f;
        float lossPct = static_cast<float>(randInt(5, 20)) / 100.0f;
        c->money *= (1.0f - lossPct);
        c->stability = std::max(0.0f, c->stability - 5.0f);
    }
}


void PoliticalEventManager::checkMilitaryIncident(const std::string& countryName, GameState& gs) {
    auto it = eventCooldowns.find("military_incident_" + countryName);
    if (it != eventCooldowns.end() && it->second > 0.0f) return;

    Country* c = gs.getCountry(countryName);
    if (!c) return;

    float baseChance = c->atWarWith.empty() ? 0.001f : 0.003f;
    if (randFloat(0.0f, 1.0f) < baseChance) {
        eventCooldowns["military_incident_" + countryName] = 270.0f;
        if (!c->atWarWith.empty()) {
            c->stability = std::max(0.0f, c->stability - 8.0f);
            c->manPower = std::max(0.0f, c->manPower - 500.0f);
        } else {
            c->stability = std::max(0.0f, c->stability - 5.0f);
            c->politicalPower = std::max(0.0f, c->politicalPower - 10.0f);
        }
    }
}


Leader generateLeader(const std::string& culture, const std::string& ideology) {
    Leader l;
    l.name = generateName(culture);
    l.ideology = ideology;
    l.age = randInt(35, 75);
    l.isAlive = true;


    const TraitDef& trait = TRAIT_POOL[randInt(0, static_cast<int>(TRAIT_POOL.size()) - 1)];
    l.traitName = trait.name;
    l.economyBonus = trait.economy;
    l.militaryBonus = trait.military;
    l.diplomacyBonus = trait.diplomacy;
    l.stabilityBonus = trait.stability;

    return l;
}


Cabinet generateCabinet(const std::string& culture) {
    Cabinet cab;
    const char* roles[] = {"economy", "military", "diplomacy", "intelligence"};
    for (auto& role : roles) {
        CabinetMember m;
        m.name = generateName(culture);
        m.role = role;
        m.skill = static_cast<float>(static_cast<int>(randFloat(-0.05f, 0.15f) * 100.0f)) / 100.0f;
        cab.add(m);
    }
    return cab;
}
