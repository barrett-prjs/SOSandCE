#pragma once
#include "core/common.h"
#include <variant>

class GameState;


enum class EffectTarget {
    Country,
    Region,
    Division,
    Faction,
    Global
};


using EffectValue = std::variant<int, float, std::string, bool, std::vector<std::string>>;

struct Effect {
    std::string  key;
    EffectTarget target = EffectTarget::Country;
    std::string  targetName;
    std::vector<EffectValue> args;
};


class EffectSystem {
public:
    EffectSystem();


    void execute(const Effect& effect, GameState& gs);


    void executeBatch(const std::vector<Effect>& effects, GameState& gs);


    std::vector<Effect> parse(const std::vector<std::string>& effectStrings, const std::string& defaultTarget);


    void addMoney(const std::string& country, float amount, GameState& gs);
    void addPoliticalPower(const std::string& country, float amount, GameState& gs);
    void addStability(const std::string& country, float amount, GameState& gs);
    void addManpower(const std::string& country, float amount, GameState& gs);
    void setIdeology(const std::string& country, float economic, float social, GameState& gs);
    void addRegion(const std::string& country, int regionId, GameState& gs);
    void removeRegion(const std::string& country, int regionId, GameState& gs);
    void declareWar(const std::string& attacker, const std::string& defender, GameState& gs);
    void makePeace(const std::string& a, const std::string& b, GameState& gs);
    void createFaction(const std::string& name, const std::string& leader, GameState& gs);
    void addToFaction(const std::string& faction, const std::string& country, GameState& gs);
    void puppet(const std::string& overlord, const std::string& puppet, GameState& gs);
    void annex(const std::string& annexer, const std::string& target, GameState& gs);
    void addFactory(const std::string& country, int region, GameState& gs);
    void addResource(const std::string& country, int resource, float amount, GameState& gs);
    void setLeader(const std::string& country, const std::string& leaderName, GameState& gs);
    void addModifier(const std::string& country, const std::string& modifier, float value, GameState& gs);
};
