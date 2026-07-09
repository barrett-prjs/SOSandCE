#pragma once
#include "core/common.h"

class GameState;


struct Leader {
    std::string name;
    std::string title;
    std::string party;
    std::string ideology;
    std::string portrait;
    std::string traitName;
    float       popularity = 0.5f;
    int         age = 50;
    bool        isAlive = true;


    float economyBonus   = 0.0f;
    float militaryBonus  = 0.0f;
    float diplomacyBonus = 0.0f;
    float stabilityBonus = 0.0f;

    std::string getFullTitle() const;
};


struct CabinetMember {
    std::string name;
    std::string role;
    std::string ideology;
    float       skill = 1.0f;
    std::string effect;
};

struct Cabinet {
    std::vector<CabinetMember> members;

    CabinetMember* getByRole(const std::string& role);
    void           add(const CabinetMember& m);
    void           remove(const std::string& role);
    void           clear();
    float          getTotalSkill() const;
};


struct PoliticalParty {
    std::string name;
    std::string ideology;
    float       support = 0.0f;
    Color       color;
};

class ElectionSystem {
public:
    ElectionSystem();
    explicit ElectionSystem(const std::string& ideology) : ElectionSystem() {

        (void)ideology;
    }

    std::vector<PoliticalParty> parties;
    int  electionFrequency = 48;
    int  monthsSinceElection = 0;
    bool electionsEnabled = true;
    int  lastProcessedMonth = -1;

    void update(GameState& gs, const std::string& countryName);
    void holdElection(GameState& gs, const std::string& countryName);
    void addParty(const PoliticalParty& party);
    void removeParty(const std::string& name);
    PoliticalParty* getLeadingParty();
    PoliticalParty* getPartyByName(const std::string& name);
    void shiftSupport(const std::string& partyName, float delta);
    void normalizeSupport();
};


class PoliticalEventManager {
public:
    PoliticalEventManager();
    ~PoliticalEventManager();

    void update(GameState& gs);
    void fireEvent(const std::string& eventId, const std::string& countryName, GameState& gs);


    void checkCoup(const std::string& countryName, GameState& gs);
    void checkRevolution(const std::string& countryName, GameState& gs);
    void checkElection(const std::string& countryName, GameState& gs);
    void checkPolicyChange(const std::string& countryName, GameState& gs);
    void checkLeaderDeath(const std::string& countryName, GameState& gs);
    void checkScandal(const std::string& countryName, GameState& gs);
    void checkEconomicShock(const std::string& countryName, GameState& gs);
    void checkMilitaryIncident(const std::string& countryName, GameState& gs);


    std::unordered_map<std::string, float> eventCooldowns;
    float globalPoliticalTension = 0.0f;
    int   lastProcessedDay = -1;
};


Leader generateLeader(const std::string& culture, const std::string& ideology);
Cabinet generateCabinet(const std::string& culture);
