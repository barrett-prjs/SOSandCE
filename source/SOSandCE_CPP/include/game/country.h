#pragma once
#include "core/common.h"
#include "game/economy.h"
#include "game/buildings.h"
#include "game/combat.h"
#include "game/focus_tree.h"
#include "game/politics.h"

class Division;
class GameState;


class Country {
public:
    Country(const std::string& name, const std::vector<int>& startRegions, GameState& gs);
    ~Country();


    std::string name;
    std::string type = "country";
    Color       color;
    std::string culture;
    std::vector<int> coreRegions;


    std::vector<int> regions;
    std::vector<int> regionsBeforeWar;
    bool resetRegionsBeforeWar = true;
    std::string capital;
    std::vector<std::string> cities;
    std::unordered_map<std::string, std::vector<int>> cultures;


    float totalMilitary = 0;
    float population = 0;
    float manPower = 0;
    float usedManPower = 0;
    int   militarySize = 2;
    std::vector<std::unique_ptr<Division>> divisions;
    std::vector<std::array<int, 2>> training;
    std::string deployRegion;


    float money = 0;
    float moneyMultiplier = 1.0f;
    int   factories = 0;
    float buildSpeed = 1.0f;
    ResourceManager  resourceManager;
    BuildingManager  buildingManager;
    std::vector<std::array<int, 3>> building;


    float politicalPower = 0;
    float politicalMultiplier = 1.0f;
    std::array<float, 2> ideology;
    std::string ideologyName;
    std::string lastIdeology;
    float baseStability = 60;
    float stability = 60;
    float lastStability = 60;
    bool  didChangeIdeology = false;
    bool  hasChangedCountry = false;
    bool  canMakeFaction = false;
    bool  expandedInvitations = false;
    std::string puppetTo;


    std::unordered_map<std::string, std::vector<std::variant<int, float, std::string, bool, std::vector<std::string>>>> decisionTree;
    FocusTreeEngine focusTreeEngine;
    std::optional<std::tuple<std::string, int, std::vector<std::string>>> focus;


    Leader         leader;
    Cabinet        cabinet;
    ElectionSystem electionSystem;
    CombatStats    combatStats;


    std::string faction;
    Color factionColor = {255, 255, 255};
    bool  factionLeader = false;
    std::vector<std::string> atWarWith;
    std::vector<std::string> militaryAccess;
    std::vector<int>         battleBorder;
    std::vector<std::string> bordering;
    float warScore = 0;
    std::string lastAttackedBy;
    bool capitulated = false;


    float defenseMultiplier   = 1.0f;
    float attackMultiplier    = 1.0f;
    float transportMultiplier = 1.0f;
    std::vector<int> toNavalInvade;


    bool  checkBordering    = true;
    bool  checkBattleBorder = false;
    Color divisionColor     = {0, 0, 0};
    int   lastDayTrained    = 0;
    float lastTimeActed     = 0;


    void update(GameState& gs);
    void runAI(GameState& gs);
    void micro(GameState& gs);


    void addRegion(int id, GameState& gs, bool ignoreFill = false, int divRegion = -1, bool ignoreResources = false);
    void addRegions(const std::vector<int>& regionList, GameState& gs, bool ignoreFill = false, bool ignoreResources = false);
    void removeRegion(int id);


    void spawnDivisions(GameState& gs);
    void addDivision(GameState& gs, int divisions = 1, int region = -1, bool ignoreManpower = false, bool ignoreTotalMilitary = false);
    void trainDivision(GameState& gs, int divisions = 1);
    bool deployTrainingDivision(GameState& gs, int trainingIndex);
    static std::array<float, RESOURCE_COUNT> trainingCostBundle(int divCount);
    static std::array<float, RESOURCE_COUNT> deploymentCostBundle(int divCount);
    void divideDivision(GameState& gs, Division* div);
    void mergeDivisions(GameState& gs, std::vector<Division*> divs);
    void resetDivColor(Color color = {0, 0, 0});


    void addFactory(GameState& gs, int region = -1);
    void addPort(GameState& gs, int region = -1);
    void build(GameState& gs, const std::string& name, int days, int region = -1);
    void destroy(GameState& gs, int region);


    void declareWar(const std::string& country, GameState& gs, bool ignoreFaction = false, bool popup = true);
    void makePeace(const std::string& country, GameState& gs, bool popup = true);
    void callToArms(const std::string& country, GameState& gs);
    void annexCountry(const std::string& culture, GameState& gs, const std::string& country = "");
    void independence(const std::string& country, GameState& gs);
    void civilWar(const std::string& country, GameState& gs, bool popup = true);
    void revolution(const std::string& ideology, GameState& gs);
    void replaceCountry(const std::string& name, GameState& gs);
    void changeDeployment(GameState& gs);


    std::vector<std::string> getBorderCountries(GameState& gs) const;
    std::vector<int>         getBattleBorder(GameState& gs, bool ignoreAccess = false) const;
    bool                     hasLandAccessTo(const std::string& otherCountry, const GameState& gs) const;
    std::vector<int>         getAccess(GameState& gs) const;
    void reloadCultureMap(GameState& gs);
    void setIdeology(const std::array<float, 2>& ideology);
    void kill(GameState& gs, bool ignorePopup = false);

private:
    GameState& gs_;
    bool canAffordTrainingResources(int divCount) const;
    bool canAffordDeploymentResources(int divCount) const;
    void spendTrainingResources(int divCount);
    void spendDeploymentResources(int divCount);
};
