#include "data/data_loader.h"

static std::string s_basePath;

namespace DataLoader {

void setBasePath(const std::string& path) {
    s_basePath = path;
}

std::string basePath() {
    return s_basePath;
}

json loadJson(const std::string& path) {
    if (!fs::exists(path)) return json::object();
    std::ifstream f(path);
    if (!f.is_open()) return json::object();
    try {
        json j;
        f >> j;
        return j;
    } catch (...) {
        return json::object();
    }
}

json getCountryRecords() { return loadJson(s_basePath + "countries.json"); }
json getResourcesDefs() { return loadJson(s_basePath + "resources.json"); }
json getBuildingsDefs() { return loadJson(s_basePath + "buildings.json"); }
json getCombatConfig() { return loadJson(s_basePath + "combat_stats.json"); }
json getDiplomacyConfig() { return loadJson(s_basePath + "diplomacy.json"); }
json getAIProfiles() { return loadJson(s_basePath + "ai_profiles.json"); }
json getLeaderNames() { return loadJson(s_basePath + "leader_names.json"); }
json getIdeologies() { return loadJson(s_basePath + "ideologies.json"); }
json getBalanceConfig() { return loadJson(s_basePath + "balance.json"); }

json getRegionPayload() {
    json payload;
    payload["regions"] = loadJson(s_basePath + "regions.json");
    payload["cities"] = loadJson(s_basePath + "cities.json");
    payload["world_regions"] = loadJson(s_basePath + "world_regions.json");
    payload["biomes"] = loadJson(s_basePath + "biomes.json");
    return payload;
}

json getFocusTree(const std::string& countryName) {
    std::string path = s_basePath + "tech_trees/countries/" + countryName + ".json";
    if (!fs::exists(path)) return json();
    return loadJson(path);
}

json getGlobalFocusTree() {
    return loadJson(s_basePath + "tech_trees/global.json");
}

}
