#pragma once
#include "core/common.h"
#include <variant>

class GameState;


using OptionValue = std::variant<int, float, std::string, bool, std::vector<std::string>>;

struct PoliticalOption {
    std::string name;
    std::string description;
    std::string category;
    float       ppCost = 0;
    bool        available = true;
    std::vector<std::string> effects;
    std::vector<std::string> requirements;
};

struct DiplomaticDemand {
    std::string type;
    std::string target;
    std::vector<OptionValue> args;
    float       cost = 0;
    bool        available = true;
};


std::unordered_map<std::string, std::vector<OptionValue>> createDecisionTree(
    const std::string& countryName, GameState& gs);


std::vector<PoliticalOption> getOptions(const std::string& countryName, GameState& gs);


std::vector<PoliticalOption> getCountryOptions(const std::string& countryName, GameState& gs);


std::vector<DiplomaticDemand> getDemands(const std::string& countryName,
                                          const std::string& targetCountry,
                                          GameState& gs);
