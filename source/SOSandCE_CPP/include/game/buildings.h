#pragma once
#include "core/common.h"
#include "game/economy.h"

class GameState;


enum class BuildingType {
    CivilianFactory,
    MilitaryFactory,
    Dockyard,
    Mine,
    OilWell,
    Refinery,
    Infrastructure,
    Port,
    Fortress,
    COUNT
};

inline const char* buildingTypeName(BuildingType bt) {
    switch (bt) {
        case BuildingType::CivilianFactory:  return "Civilian Factory";
        case BuildingType::MilitaryFactory:  return "Arms Factory";
        case BuildingType::Dockyard:         return "Dockyard";
        case BuildingType::Mine:             return "Mine";
        case BuildingType::OilWell:          return "Oil Well";
        case BuildingType::Refinery:         return "Refinery";
        case BuildingType::Infrastructure:   return "Infrastructure";
        case BuildingType::Port:             return "Port";
        case BuildingType::Fortress:         return "Fortress";
        default:                             return "Unknown";
    }
};


inline int buildingBaseCost(BuildingType bt) {
    switch (bt) {
        case BuildingType::CivilianFactory:  return 5000;
        case BuildingType::MilitaryFactory:  return 8000;
        case BuildingType::Dockyard:         return 6000;
        case BuildingType::Mine:             return 3000;
        case BuildingType::OilWell:          return 4000;
        case BuildingType::Refinery:         return 6000;
        case BuildingType::Infrastructure:   return 2000;
        case BuildingType::Port:             return 5000;
        case BuildingType::Fortress:         return 4000;
        default: return 5000;
    }
}


inline int buildingDays(BuildingType bt) {
    switch (bt) {
        case BuildingType::CivilianFactory:  return 120;
        case BuildingType::MilitaryFactory:  return 150;
        case BuildingType::Dockyard:         return 60;
        case BuildingType::Mine:             return 90;
        case BuildingType::OilWell:          return 100;
        case BuildingType::Refinery:         return 120;
        case BuildingType::Infrastructure:   return 60;
        case BuildingType::Port:             return 90;
        case BuildingType::Fortress:         return 90;
        default: return 120;
    }
}


inline int buildingMaxPerRegion(BuildingType bt) {
    switch (bt) {
        case BuildingType::CivilianFactory:  return 3;
        case BuildingType::MilitaryFactory:  return 3;
        case BuildingType::Dockyard:         return 2;
        case BuildingType::Mine:             return 2;
        case BuildingType::OilWell:          return 2;
        case BuildingType::Refinery:         return 1;
        case BuildingType::Infrastructure:   return 3;
        case BuildingType::Port:             return 3;
        case BuildingType::Fortress:         return 3;
        default: return 3;
    }
}


inline const char* buildingDescription(BuildingType bt) {
    switch (bt) {
        case BuildingType::CivilianFactory:  return "Produces money and speeds construction";
        case BuildingType::MilitaryFactory:  return "Reduces training time and boosts combat stats";
        case BuildingType::Dockyard:         return "Enables naval access, boosts transport speed";
        case BuildingType::Mine:             return "Multiplies resource extraction by 1.5x";
        case BuildingType::OilWell:          return "Extracts +2 oil per day";
        case BuildingType::Refinery:         return "Boosts oil output by 1.3x";
        case BuildingType::Infrastructure:   return "+20% move speed, +15% supply, +5% build speed";
        case BuildingType::Port:             return "Port for naval trade and transport";
        case BuildingType::Fortress:         return "Defensive fortification";
        default: return "";
    }
}

constexpr int BUILDING_TYPE_COUNT = static_cast<int>(BuildingType::COUNT);


inline Color buildingMapColor(BuildingType bt) {
    switch (bt) {
        case BuildingType::CivilianFactory:  return {0, 0, 255};
        case BuildingType::MilitaryFactory:  return {255, 0, 0};
        case BuildingType::Dockyard:         return {0, 255, 255};
        case BuildingType::Mine:             return {139, 90, 43};
        case BuildingType::OilWell:          return {30, 30, 30};
        case BuildingType::Refinery:         return {255, 165, 0};
        case BuildingType::Infrastructure:   return {255, 255, 0};
        case BuildingType::Port:             return {0, 255, 255};
        case BuildingType::Fortress:         return {128, 0, 0};
        default:                             return {255, 255, 255};
    }
}

inline std::array<float, RESOURCE_COUNT> buildingResourceCost(BuildingType bt) {
    std::array<float, RESOURCE_COUNT> cost{};
    cost.fill(0.0f);
    switch (bt) {
        case BuildingType::CivilianFactory:
            cost[static_cast<int>(Resource::Steel)] = 40.0f;
            cost[static_cast<int>(Resource::Aluminum)] = 12.0f;
            break;
        case BuildingType::MilitaryFactory:
            cost[static_cast<int>(Resource::Steel)] = 55.0f;
            cost[static_cast<int>(Resource::Tungsten)] = 18.0f;
            cost[static_cast<int>(Resource::Chromium)] = 10.0f;
            break;
        case BuildingType::Dockyard:
        case BuildingType::Port:
            cost[static_cast<int>(Resource::Steel)] = 48.0f;
            cost[static_cast<int>(Resource::Oil)] = 8.0f;
            break;
        case BuildingType::Mine:
            cost[static_cast<int>(Resource::Steel)] = 22.0f;
            cost[static_cast<int>(Resource::Tungsten)] = 6.0f;
            break;
        case BuildingType::OilWell:
            cost[static_cast<int>(Resource::Steel)] = 24.0f;
            cost[static_cast<int>(Resource::Aluminum)] = 8.0f;
            break;
        case BuildingType::Refinery:
            cost[static_cast<int>(Resource::Steel)] = 34.0f;
            cost[static_cast<int>(Resource::Oil)] = 10.0f;
            cost[static_cast<int>(Resource::Rubber)] = 8.0f;
            break;
        case BuildingType::Infrastructure:
            cost[static_cast<int>(Resource::Steel)] = 18.0f;
            cost[static_cast<int>(Resource::Oil)] = 4.0f;
            break;
        case BuildingType::Fortress:
            cost[static_cast<int>(Resource::Steel)] = 36.0f;
            cost[static_cast<int>(Resource::Tungsten)] = 8.0f;
            break;
        default:
            break;
    }
    return cost;
}


struct Building {
    BuildingType type = BuildingType::CivilianFactory;
    int          level = 1;
    int          maxLevel = 5;
    int          regionId = -1;
    bool         damaged = false;

    float getEffect() const;
};


struct ConstructionEntry {
    BuildingType type = BuildingType::CivilianFactory;
    int          regionId = -1;
    float        daysRemaining = 0;
    int          totalDays = 0;

    float progress() const {
        return totalDays > 0 ? 1.0f - daysRemaining / static_cast<float>(totalDays) : 1.0f;
    }
};


inline BuildingType buildingTypeFromString(const std::string& s) {
    if (s == "civilian_factory")  return BuildingType::CivilianFactory;
    if (s == "military_factory" || s == "arms_factory")  return BuildingType::MilitaryFactory;
    if (s == "dockyard")         return BuildingType::Dockyard;
    if (s == "mine")             return BuildingType::Mine;
    if (s == "oil_well")         return BuildingType::OilWell;
    if (s == "refinery")         return BuildingType::Refinery;
    if (s == "infrastructure")   return BuildingType::Infrastructure;
    if (s == "port")             return BuildingType::Port;
    if (s == "fortress")         return BuildingType::Fortress;
    return BuildingType::CivilianFactory;
}

inline std::string buildingTypeToString(BuildingType bt) {
    switch (bt) {
        case BuildingType::CivilianFactory:  return "civilian_factory";
        case BuildingType::MilitaryFactory:  return "arms_factory";
        case BuildingType::Dockyard:         return "dockyard";
        case BuildingType::Mine:             return "mine";
        case BuildingType::OilWell:          return "oil_well";
        case BuildingType::Refinery:         return "refinery";
        case BuildingType::Infrastructure:   return "infrastructure";
        case BuildingType::Port:             return "port";
        case BuildingType::Fortress:         return "fortress";
        default: return "civilian_factory";
    }
}


class Country;

class BuildingManager {
public:
    BuildingManager();


    std::unordered_map<int, std::vector<Building>> buildings;


    std::vector<ConstructionEntry> queue;


    std::vector<ConstructionEntry>& constructionQueue = queue;


    int  countInRegion(int regionId, BuildingType type) const;
    int  countAll(BuildingType type) const;
    bool canBuild(int regionId, BuildingType type) const;
    std::vector<Building> getInRegion(int regionId) const;


    int  getBuildingCount(const std::string& type) const { return countAll(buildingTypeFromString(type)); }
    int  getTotalBuildingCount() const {
        int total = 0;
        for (auto& [rid, blist] : buildings) total += static_cast<int>(blist.size());
        return total;
    }
    bool canBuild(int regionId, const std::string& type) const { return canBuild(regionId, buildingTypeFromString(type)); }
    bool startConstruction(int regionId, const std::string& type, Country* c);


    std::unordered_map<std::string, float> getEffectsSummary() const;
    std::unordered_map<std::string, float> getRegionEffects(int regionId) const;


    float getConstructionSpeed(Country* c) const;


    std::vector<ConstructionEntry> tick(float productionPenalty, Country* c);


    BuildingType destroyRandomInRegion(int regionId);


    void setStartingBuildings(const std::vector<int>& regions, int factoryCount);


    void addBuilding(int regionId, BuildingType type, int level = 1);
    void removeBuilding(int regionId, BuildingType type);
    void damageBuildings(int regionId, float severity);
    void repairBuildings(int regionId);


    void queueConstruction(BuildingType type, int regionId, int days);
    void update(GameState& gs, float buildSpeedMul = 1.0f);


    void transferRegion(int regionId, BuildingManager& other);
    void removeRegion(int regionId);
};
