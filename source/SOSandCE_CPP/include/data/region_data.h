#pragma once
#include "core/common.h"

struct RegionInfo {
    Color color;
    Vec2 location;
    std::vector<int> connections;
    int population = 0;
    std::string owner;
    std::unordered_map<std::string, float> resources;
};

struct CityInfo {
    int region;
    std::string culture;
};

class RegionData {
public:
    static RegionData& instance();

    void load();


    int getRegion(Color color) const;
    Vec2 getLocation(int regionId) const;
    const std::vector<int>& getConnections(int regionId) const;
    int getPopulation(int regionId) const;
    Color getRegionColor(int regionId) const;
    const RegionInfo* getInfo(int regionId) const;
    const std::unordered_map<std::string, float>& getResources(int regionId) const;


    void updateOwner(int regionId, const std::string& name);
    std::string getOwner(int regionId) const;
    const std::unordered_map<int, std::string>& getAllOwners() const;


    const std::unordered_map<std::string, CityInfo>& getCities() const { return cities_; }
    std::string getCity(int regionId) const;
    Vec2 getCityLocation(const std::string& city) const;
    int getCityRegion(const std::string& city) const;
    std::string getCityCulture(const std::string& city) const;


    std::string getBiomeName(Color color) const;
    std::array<float, 4> getBiomeInfo(Color color) const;


    std::string getWorldRegion(Color color) const;
    std::string getWorldAdjective(Color color) const;
    std::vector<std::pair<std::string, std::string>> getAllWorldRegions() const;


    std::unordered_map<int, Vec2> getAllLocations() const;
    std::unordered_map<int, std::vector<int>> getAllConnections() const;

    int regionCount() const { return static_cast<int>(regionInfo_.size()); }

private:
    RegionData() = default;
    std::unordered_map<int, RegionInfo> regionInfo_;
    std::map<std::tuple<uint8_t,uint8_t,uint8_t>, int> colorToId_;
    std::unordered_map<int, std::string> regionOwners_;
    std::unordered_map<std::string, CityInfo> cities_;
    std::unordered_map<int, std::string> locationToCities_;

    struct BiomeEntry { std::string name; float attackMod, defenseMod, moveMod; };
    std::map<std::tuple<uint8_t,uint8_t,uint8_t>, BiomeEntry> biomes_;

    struct WorldRegionEntry { std::string name, adjective; Vec2 location; };
    std::map<std::tuple<uint8_t,uint8_t,uint8_t>, WorldRegionEntry> worldRegions_;

    static const std::vector<int> emptyConnections_;
    static const std::unordered_map<std::string, float> emptyResources_;
};
