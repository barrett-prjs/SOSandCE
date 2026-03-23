#include "game/buildings.h"
#include "game/game_state.h"
#include "game/country.h"

namespace {

bool hasConstructionResources(const Country* country, const std::array<float, RESOURCE_COUNT>& cost) {
    if (!country) return false;
    for (int i = 0; i < RESOURCE_COUNT; ++i) {
        if (country->resourceManager.stockpile[i] + 0.001f < cost[i]) {
            return false;
        }
    }
    return true;
}

void spendConstructionResources(Country* country, const std::array<float, RESOURCE_COUNT>& cost) {
    if (!country) return;
    for (int i = 0; i < RESOURCE_COUNT; ++i) {
        country->resourceManager.stockpile[i] =
            std::max(0.0f, country->resourceManager.stockpile[i] - cost[i]);
    }
}

}


float Building::getEffect() const {
    switch (type) {
        case BuildingType::CivilianFactory:  return 1.0f * level;
        case BuildingType::MilitaryFactory:  return 1.0f * level;
        case BuildingType::Dockyard:         return 0.5f * level;
        case BuildingType::Mine:             return 1.5f * level;
        case BuildingType::OilWell:          return 2.0f * level;
        case BuildingType::Refinery:         return 1.3f * level;
        case BuildingType::Infrastructure:   return 0.5f * level;
        case BuildingType::Port:             return 0.5f * level;
        case BuildingType::Fortress:         return 2.0f * level;
        default: return 0.0f;
    }
}


BuildingManager::BuildingManager() = default;

int BuildingManager::countInRegion(int regionId, BuildingType type) const {
    auto it = buildings.find(regionId);
    if (it == buildings.end()) return 0;
    int count = 0;
    for (auto& b : it->second) {
        if (b.type == type) count++;
    }
    return count;
}

int BuildingManager::countAll(BuildingType type) const {
    int total = 0;
    for (auto& [rid, blist] : buildings) {
        for (auto& b : blist) {
            if (b.type == type) total++;
        }
    }
    return total;
}

bool BuildingManager::canBuild(int regionId, BuildingType type) const {
    int maxAllowed = buildingMaxPerRegion(type);
    int existing = countInRegion(regionId, type);
    return existing < maxAllowed;
}

std::vector<Building> BuildingManager::getInRegion(int regionId) const {
    auto it = buildings.find(regionId);
    if (it == buildings.end()) return {};
    return it->second;
}

void BuildingManager::addBuilding(int regionId, BuildingType type, int level) {
    Building b;
    b.type = type;
    b.level = level;
    b.regionId = regionId;
    b.damaged = false;
    b.maxLevel = buildingMaxPerRegion(type);
    buildings[regionId].push_back(b);
}

void BuildingManager::removeBuilding(int regionId, BuildingType type) {
    auto it = buildings.find(regionId);
    if (it == buildings.end()) return;
    auto& blist = it->second;
    for (auto bit = blist.begin(); bit != blist.end(); ++bit) {
        if (bit->type == type) {
            blist.erase(bit);
            if (blist.empty()) buildings.erase(it);
            return;
        }
    }
}

void BuildingManager::damageBuildings(int regionId, float severity) {
    auto it = buildings.find(regionId);
    if (it == buildings.end()) return;
    for (auto& b : it->second) {
        if (randFloat(0.0f, 1.0f) < severity) {
            b.damaged = true;
        }
    }
}

void BuildingManager::repairBuildings(int regionId) {
    auto it = buildings.find(regionId);
    if (it == buildings.end()) return;
    for (auto& b : it->second) b.damaged = false;
}

void BuildingManager::queueConstruction(BuildingType type, int regionId, int days) {
    ConstructionEntry entry;
    entry.type = type;
    entry.regionId = regionId;
    entry.daysRemaining = days;
    entry.totalDays = days;
    constructionQueue.push_back(entry);
}

void BuildingManager::update(GameState& gs, float buildSpeedMul) {
    if (constructionQueue.empty()) return;

    int civCount = countAll(BuildingType::CivilianFactory);
    float speedMul = (1.0f + civCount * 0.1f) * buildSpeedMul;
    float rate = static_cast<float>(gs.speed) / 240.0f;

    std::vector<size_t> completedIndices;
    for (size_t i = 0; i < constructionQueue.size(); ++i) {
        auto& entry = constructionQueue[i];
        entry.daysRemaining -= static_cast<int>(std::ceil(rate * speedMul));
        if (entry.daysRemaining <= 0) {
            completedIndices.push_back(i);
        }
    }

    for (int i = static_cast<int>(completedIndices.size()) - 1; i >= 0; --i) {
        size_t idx = completedIndices[i];
        auto& entry = constructionQueue[idx];
        addBuilding(entry.regionId, entry.type);
        constructionQueue.erase(constructionQueue.begin() + idx);
    }
}

void BuildingManager::transferRegion(int regionId, BuildingManager& other) {
    auto it = buildings.find(regionId);
    if (it == buildings.end()) return;
    auto& target = other.buildings[regionId];
    target.insert(target.end(), it->second.begin(), it->second.end());
    buildings.erase(it);
}

void BuildingManager::removeRegion(int regionId) {
    buildings.erase(regionId);
}


static const std::unordered_map<std::string, float>& buildingEffects(BuildingType bt) {
    static const std::unordered_map<BuildingType, std::unordered_map<std::string, float>> table = {
        {BuildingType::CivilianFactory,  {{"construction_speed", 0.1f}, {"money_production", 5000.0f}}},
        {BuildingType::MilitaryFactory,  {{"military_production", 1.0f}, {"training_speed", 0.15f}}},
        {BuildingType::Dockyard,         {{"transport_speed", 0.1f}}},
        {BuildingType::Mine,             {{"resource_multiplier", 1.5f}}},
        {BuildingType::OilWell,          {{"oil_production", 2.0f}}},
        {BuildingType::Refinery,         {{"fuel_production", 1.0f}}},
        {BuildingType::Infrastructure,   {{"movement_speed", 0.2f}, {"supply", 0.15f}, {"construction_speed", 0.05f}}},
    };
    static const std::unordered_map<std::string, float> empty;
    auto it = table.find(bt);
    return it != table.end() ? it->second : empty;
}

std::unordered_map<std::string, float> BuildingManager::getEffectsSummary() const {
    std::unordered_map<std::string, float> totals;
    for (auto& [rid, blist] : buildings) {
        for (auto& b : blist) {
            for (auto& [eff, val] : buildingEffects(b.type)) {
                totals[eff] += val;
            }
        }
    }
    return totals;
}

std::unordered_map<std::string, float> BuildingManager::getRegionEffects(int regionId) const {
    std::unordered_map<std::string, float> totals;
    auto it = buildings.find(regionId);
    if (it == buildings.end()) return totals;
    for (auto& b : it->second) {
        for (auto& [eff, val] : buildingEffects(b.type)) {
            totals[eff] += val;
        }
    }
    return totals;
}


float BuildingManager::getConstructionSpeed(Country* c) const {
    int civCount = countAll(BuildingType::CivilianFactory);
    float infraBonus = 0.0f;
    auto effects = getEffectsSummary();
    auto it = effects.find("construction_speed");
    if (it != effects.end()) infraBonus = it->second;
    float base = 1.0f + civCount * 0.1f + infraBonus;
    float bs = c ? c->buildSpeed : 1.0f;
    return base * bs;
}


std::vector<ConstructionEntry> BuildingManager::tick(float productionPenalty, Country* c) {
    if (queue.empty()) return {};

    float speedMult = getConstructionSpeed(c) * productionPenalty;

    std::vector<ConstructionEntry> completed;
    for (auto it = queue.begin(); it != queue.end(); ) {
        it->daysRemaining -= speedMult;
        if (it->daysRemaining <= 0.0f) {
            addBuilding(it->regionId, it->type);
            completed.push_back(*it);
            it = queue.erase(it);
        } else {
            ++it;
        }
    }
    return completed;
}


bool BuildingManager::startConstruction(int regionId, const std::string& type, Country* c) {
    BuildingType bt = buildingTypeFromString(type);
    if (!canBuild(regionId, bt)) return false;

    float scale = c ? (1.0f + static_cast<float>(c->regions.size()) / 150.0f) : 1.0f;
    float cost = static_cast<float>(buildingBaseCost(bt)) * scale;
    if (c && c->money < cost) return false;
    auto resCost = buildingResourceCost(bt);
    if (c && !hasConstructionResources(c, resCost)) return false;
    if (c) {
        c->money -= cost;
        spendConstructionResources(c, resCost);
    }

    int days = buildingDays(bt);
    queueConstruction(bt, regionId, days);
    return true;
}


BuildingType BuildingManager::destroyRandomInRegion(int regionId) {
    auto it = buildings.find(regionId);
    if (it == buildings.end() || it->second.empty()) return BuildingType::COUNT;

    int idx = randInt(0, static_cast<int>(it->second.size()) - 1);
    BuildingType removed = it->second[idx].type;
    it->second.erase(it->second.begin() + idx);
    if (it->second.empty()) buildings.erase(it);
    return removed;
}


void BuildingManager::setStartingBuildings(const std::vector<int>& regions, int factoryCount) {
    if (regions.empty()) return;

    int civToPlace = std::max(1, factoryCount / 2);
    int armsToPlace = factoryCount - civToPlace;

    int idx = 0;
    for (int i = 0; i < civToPlace; i++) {
        int rid = regions[idx % regions.size()];
        if (canBuild(rid, BuildingType::CivilianFactory)) {
            addBuilding(rid, BuildingType::CivilianFactory);
        }
        idx++;
    }
    for (int i = 0; i < armsToPlace; i++) {
        int rid = regions[idx % regions.size()];
        if (canBuild(rid, BuildingType::MilitaryFactory)) {
            addBuilding(rid, BuildingType::MilitaryFactory);
        }
        idx++;
    }
}
