#pragma once
#include "core/common.h"
#include "data/json.hpp"

class GameState;
class MapManager;

using json = nlohmann::json;

struct SaveInfo {
    std::string name;
    std::string date;
    std::string controlledCountry;
    bool valid = false;
};

class SaveSystem {
public:
    SaveSystem();

    bool saveGame(const GameState& gs, const MapManager& maps, const std::string& name);
    bool loadGame(GameState& gs, MapManager& maps, const std::string& name);
    std::vector<SaveInfo> listSaves() const;
    bool deleteSave(const std::string& name);

private:
    std::string savesDir_;

    json serializeCountry(const class Country& c);
    json serializeFaction(const class Faction& f);
    json serializeGameTime(const struct GameTime& t);
    json serializeWorldData(const GameState& gs);

    void deserializeCountry(class Country& c, const json& j, GameState& gs);
    void deserializeFaction(class Faction& f, const json& j);
    void deserializeGameTime(struct GameTime& t, const json& j);
    void deserializeWorldData(GameState& gs, const json& j);

    void saveMapSurface(SDL_Surface* surf, const std::string& path);
    SDL_Surface* loadMapSurface(const std::string& path);
};
