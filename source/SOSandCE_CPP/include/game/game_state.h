#pragma once
#include "core/common.h"
#include "game/battle.h"

class Country;
class Faction;
class PeaceConference;
struct PuppetState;
struct TradeContract;
class AIController;
class FogOfWar;
class EventManager;
class PoliticalEventManager;


struct GameTime {
    int hour = 1, day = 1, month = 1, year = 2026;
    int startHour = 1, startDay = 1, startMonth = 1, startYear = 2026;
    int totalDays = 0;
};


class GameState {
public:
    GameState();
    ~GameState();


    std::unordered_map<std::string, std::unique_ptr<Country>> countries;
    std::unordered_map<std::string, std::unique_ptr<Faction>> factions;
    std::vector<std::unique_ptr<Battle>> battles;
    std::vector<std::string> countryList;
    std::vector<std::string> factionList;


    std::vector<TradeContract> tradeContracts;
    std::vector<PuppetState> puppetStates;


    std::vector<int> ports;
    std::vector<int> canals;
    GameTime time;
    float tick = 0;


    std::string controlledCountry;
    std::string mapName = "Modern Day";
    int speed = 0;
    bool inGame = false;
    bool mapViewOnly = false;


    std::unordered_map<int, std::vector<class Division*>> divisionsByRegion;


    std::unique_ptr<EventManager> eventManager;
    std::unique_ptr<PoliticalEventManager> polEventManager;
    std::unique_ptr<FogOfWar> fogOfWar;
    std::unique_ptr<PeaceConference> pendingPeaceConference;
    std::unordered_map<std::string, std::unique_ptr<AIController>> aiControllers;


    SDL_Surface* politicalMapSurf = nullptr;
    SDL_Surface* ideologyMapSurf = nullptr;
    SDL_Surface* factionMapSurf = nullptr;
    SDL_Surface* industryMapSurf = nullptr;
    SDL_Surface* cultureMapSurf = nullptr;
    SDL_Surface* regionsMapSurf = nullptr;
    bool mapDirty = false;


    float DIVISION_UPKEEP_PER_DAY = 200.0f;
    float TRAINING_COST_PER_DIV   = 25000.0f;


    Country* getCountry(const std::string& name);
    const Country* getCountry(const std::string& name) const;
    Faction* getFaction(const std::string& name);
    const Faction* getFaction(const std::string& name) const;
    void registerCountry(const std::string& name, std::unique_ptr<Country> c);
    void registerFaction(const std::string& name, std::unique_ptr<Faction> f);
    void removeCountry(const std::string& name);
    void removeFaction(const std::string& name);


    void clear();
    void updateTime();
    void tickAll();
    void spawnCountry(const std::string& name, const std::vector<int>& regions = {});
    void spawnFaction(const std::vector<std::string>& members, const std::string& name = "");
    void destroyCanal(const std::vector<int>& canalRegions);
    void peaceTreaty(const std::string& mainCountry);
    void finalizePeaceResolution(const std::vector<std::string>& victors,
                                 const std::vector<std::string>& combatants);
};
