#pragma once
#include "core/common.h"

class GameState;
class Country;
class Division;


struct AIPersonality {
    float aggressiveness  = 0.5f;
    float defensiveness   = 0.5f;
    float diplomacy       = 0.5f;
    float economicFocus   = 0.5f;
    float expansionism    = 0.5f;
    float factionLoyalty  = 0.5f;
};


class AIController {
public:
    AIController(const std::string& countryName, GameState& gs);
    ~AIController();

    std::string countryName;
    AIPersonality personality;


    int lastActionHourStamp = -1;
    int peaceActionIntervalHours = 12;
    int warActionIntervalHours = 1;
    int lastWarActionHourStamp = -1;
    int lastFactionActionHourStamp = -1;
    int lastAccessActionHourStamp = -1;
    int warActionCooldownHours = 24 * 45;
    int factionActionCooldownHours = 24 * 30;
    int accessActionCooldownHours = 24 * 14;


    bool  atWar = false;
    bool  wantsWar = false;
    std::string targetCountry;
    std::vector<std::string> enemies;
    std::vector<std::string> potentialAllies;


    void update(GameState& gs);


    void evaluateThreats(GameState& gs);
    void evaluateAlliances(GameState& gs);
    void evaluateWar(GameState& gs);
    void evaluateEconomy(GameState& gs);
    void evaluatePolitics(GameState& gs);


    void manageDivisions(GameState& gs);
    void assignFrontline(GameState& gs);
    void orderAttack(GameState& gs);
    void orderDefend(GameState& gs);
    void orderRetreat(GameState& gs);
    void manageDivisionTraining(GameState& gs);


    void microDivision(Division* div, GameState& gs);
    int  findBestTarget(Division* div, GameState& gs) const;
    int  findSafeRegion(Division* div, GameState& gs) const;
    bool shouldRetreat(Division* div, GameState& gs) const;


    void considerJoinFaction(GameState& gs);
    void considerCreateFaction(GameState& gs);
    void considerMilitaryAccess(GameState& gs);
    void considerDeclareWar(GameState& gs);
    void considerMakePeace(GameState& gs);
    void considerAlliance(GameState& gs);


    void manageBuildQueue(GameState& gs);
    void manageTrade(GameState& gs);
    void manageFocus(GameState& gs);


    Country* getCountry(GameState& gs) const;
    float    threatLevel(const std::string& otherCountry, GameState& gs) const;
    float    allianceDesirability(const std::string& otherCountry, GameState& gs) const;
    bool     canAffordWar(GameState& gs) const;
};
