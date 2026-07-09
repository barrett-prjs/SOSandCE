#pragma once
#include "core/common.h"
#include "data/json.hpp"

using json = nlohmann::json;

namespace DataLoader {
    json loadJson(const std::string& path);

    json getCountryRecords();
    json getRegionPayload();
    json getResourcesDefs();
    json getBuildingsDefs();
    json getCombatConfig();
    json getDiplomacyConfig();
    json getAIProfiles();
    json getLeaderNames();
    json getIdeologies();
    json getBalanceConfig();
    json getFocusTree(const std::string& countryName);
    json getGlobalFocusTree();

    void setBasePath(const std::string& path);
    std::string basePath();
}
