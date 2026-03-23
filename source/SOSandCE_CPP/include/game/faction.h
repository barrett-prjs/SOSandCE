#pragma once
#include "core/common.h"

class GameState;


class Faction {
public:
    Faction(const std::string& name, const std::vector<std::string>& members, GameState& gs);
    ~Faction();

    std::string name;
    std::string type = "faction";
    Color       color;
    std::string ideology;
    std::vector<std::string> spirit;
    std::vector<std::string> factionWar;
    std::vector<std::string> members;
    std::string factionLeader;
    std::string flag;
    float lastTimeActed = 0;

    void update(GameState& gs);
    std::vector<int> getBattleBorder(GameState& gs) const;
    void reloadDivisionColors(GameState& gs);
    void addCountry(const std::string& country, GameState& gs, bool sendPrompt = true, bool createPopup = true);
    void removeCountry(const std::string& country, GameState& gs, bool ignoreFill = false, bool doPopup = true);
    void declareWar(const std::string& faction, GameState& gs);
    void kill(GameState& gs);

private:
    GameState& gs_;
};
