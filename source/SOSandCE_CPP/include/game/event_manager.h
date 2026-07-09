#pragma once
#include "core/common.h"

class GameState;


class EventManager {
public:
    EventManager();
    ~EventManager();


    float globalTension = 0.0f;


    int   eventFrequency = 30;
    int   daysSinceLastEvent = 0;
    float eventChanceMultiplier = 1.0f;


    std::vector<std::string> eventHistory;
    std::unordered_map<std::string, int> eventCooldowns;


    void update(GameState& gs);


    void localWar(GameState& gs);
    void civilWar(GameState& gs);
    void independence(GameState& gs);
    void createFaction(GameState& gs);
    void joinFaction(GameState& gs);
    void factionWar(GameState& gs);
    void borderConflict(GameState& gs);
    void countryEvent(GameState& gs);


    bool canFireEvent(const std::string& type) const;
    void setCooldown(const std::string& type, int days);
    void tickCooldowns();
    float getEventProbability(const std::string& type) const;


    void addTension(float amount);
    void decayTension(float rate = 0.01f);
};
