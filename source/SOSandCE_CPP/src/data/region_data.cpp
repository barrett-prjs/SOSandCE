#include "data/region_data.h"
#include "data/data_loader.h"

const std::vector<int> RegionData::emptyConnections_;
const std::unordered_map<std::string, float> RegionData::emptyResources_;

RegionData& RegionData::instance() {
    static RegionData rd;
    return rd;
}

void RegionData::load() {
    printf("[RegionData] Loading...\n"); fflush(stdout);
    auto payload = DataLoader::getRegionPayload();


    printf("[RegionData] Loading %d regions...\n", payload.contains("regions") ? (int)payload["regions"].size() : 0); fflush(stdout);
    if (payload.contains("regions")) {
        for (auto& [key, record] : payload["regions"].items()) {
            int id = std::stoi(key);
            RegionInfo info;
            if (record.contains("color") && record["color"].is_array() && record["color"].size() >= 3) {
                info.color = {
                    record["color"][0].get<uint8_t>(),
                    record["color"][1].get<uint8_t>(),
                    record["color"][2].get<uint8_t>()
                };
            }
            if (record.contains("location") && record["location"].is_array() && record["location"].size() >= 2) {
                info.location = {record["location"][0].get<float>(), record["location"][1].get<float>()};
            }
            if (record.contains("connections")) {
                for (auto& c : record["connections"]) info.connections.push_back(c.get<int>());
            }
            if (record.contains("population") && record["population"].is_number()) info.population = record["population"].get<int>();
            if (record.contains("owner") && record["owner"].is_string()) info.owner = record["owner"].get<std::string>();
            if (record.contains("resources") && record["resources"].is_object()) {
                for (auto& [rk, rv] : record["resources"].items()) {
                    info.resources[rk] = rv.get<float>();
                }
            }

            regionInfo_[id] = info;
            colorToId_[{info.color.r, info.color.g, info.color.b}] = id;
            if (!info.owner.empty()) regionOwners_[id] = info.owner;
        }
    }

    printf("[RegionData] Loaded %d regions\n", (int)regionInfo_.size()); fflush(stdout);


    printf("[RegionData] Loading cities...\n"); fflush(stdout);
    if (payload.contains("cities")) {
        for (auto& [name, record] : payload["cities"].items()) {
            CityInfo ci;
            ci.region = (record.contains("region") && record["region"].is_number()) ? record["region"].get<int>() : 0;
            ci.culture = (record.contains("culture") && record["culture"].is_string()) ? record["culture"].get<std::string>() : "";
            cities_[name] = ci;
            locationToCities_[ci.region] = name;
        }
    }

    printf("[RegionData] Loaded %d cities\n", (int)cities_.size()); fflush(stdout);


    printf("[RegionData] Loading biomes...\n"); fflush(stdout);
    if (payload.contains("biomes")) {
        for (auto& [key, val] : payload["biomes"].items()) {

            uint8_t r = 0, g = 0, b = 0;
            if (key.find(',') != std::string::npos) {
                sscanf(key.c_str(), "%hhu,%hhu,%hhu", &r, &g, &b);
            }
            BiomeEntry be;
            if (val.is_array() && val.size() >= 4) {
                be.name = val[0].get<std::string>();
                be.attackMod = val[1].get<float>();
                be.defenseMod = val[2].get<float>();
                be.moveMod = val[3].get<float>();
            } else if (val.is_object()) {
                be.name = val.value("name", "Unknown");
                be.attackMod = val.value("attack", 1.0f);
                be.defenseMod = val.value("defense", 1.0f);
                be.moveMod = val.value("movement", 1.0f);
            }
            biomes_[{r, g, b}] = be;
        }
    }


    if (payload.contains("world_regions")) {
        for (auto& [key, val] : payload["world_regions"].items()) {
            uint8_t r = 0, g = 0, b = 0;
            if (key.find(',') != std::string::npos) {
                sscanf(key.c_str(), "%hhu,%hhu,%hhu", &r, &g, &b);
            }
            WorldRegionEntry wr;
            if (val.is_array() && val.size() >= 2) {
                wr.name = val[0].get<std::string>();
                wr.adjective = val[1].get<std::string>();
                if (val.size() >= 4) {
                    wr.location = {val[2].get<float>(), val[3].get<float>()};
                }
            }
            worldRegions_[{r, g, b}] = wr;
        }
    }
}

int RegionData::getRegion(Color color) const {
    if (color.r == 0 && color.g == 0 && color.b == 0) return -1;
    auto it = colorToId_.find({color.r, color.g, color.b});
    return it != colorToId_.end() ? it->second : -1;
}

Vec2 RegionData::getLocation(int regionId) const {
    auto it = regionInfo_.find(regionId);
    return it != regionInfo_.end() ? it->second.location : Vec2{0, 0};
}

const std::vector<int>& RegionData::getConnections(int regionId) const {
    auto it = regionInfo_.find(regionId);
    return it != regionInfo_.end() ? it->second.connections : emptyConnections_;
}

int RegionData::getPopulation(int regionId) const {
    auto it = regionInfo_.find(regionId);
    return it != regionInfo_.end() ? it->second.population : 0;
}

Color RegionData::getRegionColor(int regionId) const {
    auto it = regionInfo_.find(regionId);
    return it != regionInfo_.end() ? it->second.color : Color{0, 0, 0};
}

const RegionInfo* RegionData::getInfo(int regionId) const {
    auto it = regionInfo_.find(regionId);
    return it != regionInfo_.end() ? &it->second : nullptr;
}

const std::unordered_map<std::string, float>& RegionData::getResources(int regionId) const {
    auto it = regionInfo_.find(regionId);
    if (it != regionInfo_.end()) return it->second.resources;
    return emptyResources_;
}

void RegionData::updateOwner(int regionId, const std::string& name) {
    regionOwners_[regionId] = name;
}

std::string RegionData::getOwner(int regionId) const {
    auto it = regionOwners_.find(regionId);
    return it != regionOwners_.end() ? it->second : "";
}

const std::unordered_map<int, std::string>& RegionData::getAllOwners() const {
    return regionOwners_;
}

std::string RegionData::getCity(int regionId) const {
    auto it = locationToCities_.find(regionId);
    return it != locationToCities_.end() ? it->second : "";
}

Vec2 RegionData::getCityLocation(const std::string& city) const {
    auto it = cities_.find(city);
    if (it == cities_.end()) return {0, 0};
    return getLocation(it->second.region);
}

int RegionData::getCityRegion(const std::string& city) const {
    auto it = cities_.find(city);
    return it != cities_.end() ? it->second.region : -1;
}

std::string RegionData::getCityCulture(const std::string& city) const {
    auto it = cities_.find(city);
    return it != cities_.end() ? it->second.culture : "";
}

std::string RegionData::getBiomeName(Color color) const {
    auto it = biomes_.find({color.r, color.g, color.b});
    return it != biomes_.end() ? it->second.name : "Unknown";
}

std::array<float, 4> RegionData::getBiomeInfo(Color color) const {
    auto it = biomes_.find({color.r, color.g, color.b});
    if (it != biomes_.end()) {
        return {0, it->second.attackMod, it->second.defenseMod, it->second.moveMod};
    }
    return {0, 1.0f, 1.0f, 1.0f};
}

std::string RegionData::getWorldRegion(Color color) const {
    auto it = worldRegions_.find({color.r, color.g, color.b});
    return it != worldRegions_.end() ? it->second.name : "Unknown";
}

std::string RegionData::getWorldAdjective(Color color) const {
    auto it = worldRegions_.find({color.r, color.g, color.b});
    return it != worldRegions_.end() ? it->second.adjective : "Unknown";
}

std::vector<std::pair<std::string, std::string>> RegionData::getAllWorldRegions() const {
    std::vector<std::pair<std::string, std::string>> result;
    for (auto& [k, v] : worldRegions_) {
        result.emplace_back(v.name, v.adjective);
    }
    return result;
}

std::unordered_map<int, Vec2> RegionData::getAllLocations() const {
    std::unordered_map<int, Vec2> result;
    for (auto& [id, info] : regionInfo_) {
        result[id] = info.location;
    }
    return result;
}

std::unordered_map<int, std::vector<int>> RegionData::getAllConnections() const {
    std::unordered_map<int, std::vector<int>> result;
    for (auto& [id, info] : regionInfo_) {
        result[id] = info.connections;
    }
    return result;
}
