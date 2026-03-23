#pragma once
#include "core/common.h"

class GameState;


enum class DemandType {
    AnnexRegion,
    AnnexCountry,
    ReleaseCountry,
    Puppet,
    Demilitarize,
    WarReparations,
    ChangeIdeology,
    COUNT
};


struct PeaceDemand {
    DemandType  type = DemandType::AnnexRegion;
    std::string demander;
    std::string target;
    std::string releaseName;
    std::vector<int> regions;
    float       warScoreCost = 0.0f;
    bool        accepted = false;
};


class PeaceConference {
public:
    PeaceConference();
    ~PeaceConference();


    std::vector<std::string> winners;
    std::vector<std::string> losers;
    std::vector<std::string> originalWinners;
    std::vector<std::string> originalLosers;


    std::vector<PeaceDemand> demands;
    int currentDemandIndex = 0;


    std::unordered_map<std::string, float> warScoreBudget;


    bool active = false;
    bool finished = false;


    void start(const std::vector<std::string>& winners,
               const std::vector<std::string>& losers,
               GameState& gs);
    void addDemand(const PeaceDemand& demand);
    void removeDemand(int index);
    bool canAfford(const std::string& country, float cost) const;
    void processDemand(int index, GameState& gs);
    void processAll(GameState& gs);
    void finish(GameState& gs);
    void autoResolve(GameState& gs);


    std::vector<PeaceDemand> getDemandsFor(const std::string& country) const;
    float getRemainingWarScore(const std::string& country) const;
};
