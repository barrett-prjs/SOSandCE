#include "game/event_manager.h"
#include "game/game_state.h"
#include "game/country.h"
#include "game/faction.h"
#include "game/helpers.h"
#include "data/country_data.h"
#include "ui/toast.h"


EventManager::EventManager()
    : globalTension(0.0f),
      eventFrequency(30),
      daysSinceLastEvent(0),
      eventChanceMultiplier(1.0f)
{}

EventManager::~EventManager() = default;


void EventManager::update(GameState& gs) {
    daysSinceLastEvent++;
    tickCooldowns();
    decayTension();

    if (daysSinceLastEvent < eventFrequency) return;

    daysSinceLastEvent = 0;


    if (canFireEvent("localWar"))       localWar(gs);
    if (canFireEvent("civilWar"))       civilWar(gs);
    if (canFireEvent("independence"))    independence(gs);
    if (canFireEvent("createFaction"))   createFaction(gs);
    if (canFireEvent("joinFaction"))     joinFaction(gs);
    if (canFireEvent("factionWar"))      factionWar(gs);
    if (canFireEvent("borderConflict"))  borderConflict(gs);
    if (canFireEvent("countryEvent"))    countryEvent(gs);
}


void EventManager::localWar(GameState& gs) {
    float prob = getEventProbability("localWar");
    if (randFloat(0.0f, 1.0f) > prob) return;


    auto& cl = gs.countryList;
    if (cl.size() < 2) return;

    int maxAttempts = 20;
    for (int attempt = 0; attempt < maxAttempts; ++attempt) {
        std::string a = cl[randInt(0, static_cast<int>(cl.size()) - 1)];
        Country* ca = gs.getCountry(a);
        if (!ca || ca->bordering.empty()) continue;

        std::string b = ca->bordering[randInt(0, static_cast<int>(ca->bordering.size()) - 1)];
        Country* cb = gs.getCountry(b);
        if (!cb) continue;


        bool alreadyAtWar = false;
        for (auto& e : ca->atWarWith) {
            if (e == b) { alreadyAtWar = true; break; }
        }
        if (alreadyAtWar) continue;


        if (a == gs.controlledCountry || b == gs.controlledCountry) continue;

        if (ca->politicalPower < 25.0f) continue;

        ca->politicalPower -= 25.0f;
        ca->declareWar(b, gs, false, false);
        addTension(5.0f);
        setCooldown("localWar", 90);
        eventHistory.push_back("localWar: " + a + " vs " + b);
        return;
    }
}

void EventManager::civilWar(GameState& gs) {
    float prob = getEventProbability("civilWar");
    if (randFloat(0.0f, 1.0f) > prob) return;

    auto& cl = gs.countryList;
    if (cl.empty()) return;

    for (int attempt = 0; attempt < 10; ++attempt) {
        std::string name = cl[randInt(0, static_cast<int>(cl.size()) - 1)];
        if (name == gs.controlledCountry) continue;

        Country* c = gs.getCountry(name);
        if (!c) continue;
        if (c->regions.size() < 4) continue;
        if (c->stability > 30.0f) continue;

        c->civilWar(name, gs, false);
        addTension(10.0f);
        setCooldown("civilWar", 360);
        eventHistory.push_back("civilWar: " + name);
        return;
    }
}

void EventManager::independence(GameState& gs) {
    float prob = getEventProbability("independence");
    if (randFloat(0.0f, 1.0f) > prob) return;

    auto& cd = CountryData::instance();


    for (auto& [name, country] : gs.countries) {
        if (!country || country->regions.size() < 10) continue;
        if (!country->atWarWith.empty()) continue;


        for (auto& [cultureName, cultureRegions] : country->cultures) {
            if (cultureName == country->culture) continue;
            if (cultureRegions.size() < 3) continue;


            std::string potentialCountry = cd.getCountryType(cultureName);
            if (potentialCountry.empty()) continue;
            if (gs.getCountry(potentialCountry)) continue;


            country->independence(potentialCountry, gs);
            toasts().show(replaceAll(potentialCountry, "_", " ") + " has declared independence!", 3000);
            setCooldown("independence", 180);
            return;
        }
    }

    setCooldown("independence", 90);
}

void EventManager::createFaction(GameState& gs) {
    float prob = getEventProbability("createFaction");
    if (randFloat(0.0f, 1.0f) > prob) return;


    for (auto& [name, country] : gs.countries) {
        if (!country) continue;
        if (name == gs.controlledCountry) continue;
        if (!country->faction.empty()) continue;
        if (!country->canMakeFaction) continue;

        if (country->politicalPower < 100.0f) continue;

        std::string factionName = name + " Alliance";
        auto faction = std::make_unique<Faction>(factionName, std::vector<std::string>{name}, gs);
        gs.registerFaction(factionName, std::move(faction));

        country->faction = factionName;
        country->factionLeader = true;
        country->politicalPower -= 100.0f;

        addTension(3.0f);
        setCooldown("createFaction", 180);
        eventHistory.push_back("createFaction: " + factionName);
        return;
    }
}

void EventManager::joinFaction(GameState& gs) {
    float prob = getEventProbability("joinFaction");
    if (randFloat(0.0f, 1.0f) > prob) return;

    if (gs.factionList.empty()) return;


    for (auto& [name, country] : gs.countries) {
        if (!country) continue;
        if (name == gs.controlledCountry) continue;
        if (!country->faction.empty()) continue;


        for (auto& fName : gs.factionList) {
            Faction* f = gs.getFaction(fName);
            if (!f) continue;


            Country* leader = gs.getCountry(f->factionLeader);
            if (!leader) continue;

            std::string leaderIdeology = getIdeologyName(leader->ideology[0], leader->ideology[1]);
            std::string countryIdeology = getIdeologyName(country->ideology[0], country->ideology[1]);

            if (leaderIdeology == countryIdeology || countryIdeology == "nonaligned") {
                if (country->politicalPower < 25.0f) continue;
                country->politicalPower -= 25.0f;
                f->addCountry(name, gs, false, false);
                addTension(2.0f);
                setCooldown("joinFaction", 90);
                eventHistory.push_back("joinFaction: " + name + " -> " + fName);
                return;
            }
        }
    }
}

void EventManager::factionWar(GameState& gs) {
    float prob = getEventProbability("factionWar");
    if (randFloat(0.0f, 1.0f) > prob) return;

    if (gs.factionList.size() < 2) return;


    for (size_t i = 0; i < gs.factionList.size(); ++i) {
        for (size_t j = i + 1; j < gs.factionList.size(); ++j) {
            Faction* f1 = gs.getFaction(gs.factionList[i]);
            Faction* f2 = gs.getFaction(gs.factionList[j]);
            if (!f1 || !f2) continue;


            bool alreadyAtWar = false;
            for (auto& fw : f1->factionWar) {
                if (fw == gs.factionList[j]) { alreadyAtWar = true; break; }
            }
            if (alreadyAtWar) continue;


            if (globalTension < 30.0f) continue;


            if (randFloat(0.0f, 1.0f) < 0.1f) {
                f1->declareWar(gs.factionList[j], gs);
                addTension(20.0f);
                setCooldown("factionWar", 720);
                eventHistory.push_back("factionWar: " + gs.factionList[i] + " vs " + gs.factionList[j]);
                return;
            }
        }
    }
}

void EventManager::borderConflict(GameState& gs) {
    float prob = getEventProbability("borderConflict");
    if (randFloat(0.0f, 1.0f) > prob) return;


    addTension(2.0f);
    setCooldown("borderConflict", 60);
    eventHistory.push_back("borderConflict");
}

void EventManager::countryEvent(GameState& gs) {
    float prob = getEventProbability("countryEvent");
    if (randFloat(0.0f, 1.0f) > prob) return;


    auto& cl = gs.countryList;
    if (cl.empty()) return;

    std::string name = cl[randInt(0, static_cast<int>(cl.size()) - 1)];
    if (name == gs.controlledCountry) return;

    Country* c = gs.getCountry(name);
    if (!c) return;


    int eventType = randInt(0, 3);
    switch (eventType) {
        case 0:
            c->money += 5000000.0f;
            break;
        case 1:
            c->money = std::max(0.0f, c->money * 0.9f);
            break;
        case 2:
            c->stability = std::min(100.0f, c->stability + 5.0f);
            break;
        case 3:
            c->stability = std::max(0.0f, c->stability - 5.0f);
            break;
    }

    setCooldown("countryEvent", 30);
    eventHistory.push_back("countryEvent: " + name);
}


bool EventManager::canFireEvent(const std::string& type) const {
    auto it = eventCooldowns.find(type);
    if (it != eventCooldowns.end() && it->second > 0) return false;
    return true;
}

void EventManager::setCooldown(const std::string& type, int days) {
    eventCooldowns[type] = days;
}

void EventManager::tickCooldowns() {
    for (auto& [type, cd] : eventCooldowns) {
        if (cd > 0) cd--;
    }
}

float EventManager::getEventProbability(const std::string& type) const {
    float base = 0.05f;

    if (type == "localWar")       base = 0.08f + globalTension * 0.002f;
    else if (type == "civilWar")  base = 0.02f + globalTension * 0.001f;
    else if (type == "independence") base = 0.03f;
    else if (type == "createFaction") base = 0.05f;
    else if (type == "joinFaction")   base = 0.06f;
    else if (type == "factionWar")    base = 0.02f + globalTension * 0.003f;
    else if (type == "borderConflict") base = 0.10f;
    else if (type == "countryEvent")   base = 0.15f;

    return base * eventChanceMultiplier;
}

void EventManager::addTension(float amount) {
    globalTension = std::min(100.0f, globalTension + amount);
}

void EventManager::decayTension(float rate) {
    globalTension = std::max(0.0f, globalTension - rate);
}
