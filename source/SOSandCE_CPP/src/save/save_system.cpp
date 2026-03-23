#include "save/save_system.h"
#include "core/engine.h"
#include "game/game_state.h"
#include "game/country.h"
#include "game/division.h"
#include "game/faction.h"
#include "map/map_manager.h"

SaveSystem::SaveSystem() {
    auto& engine = Engine::instance();
    savesDir_ = engine.assetsPath + "saves/";
    fs::create_directories(savesDir_);
}

bool SaveSystem::saveGame(const GameState& gs, const MapManager& maps, const std::string& name) {
    std::string saveDir = savesDir_ + name + "/";
    printf("[SaveSystem] Saving '%s' to %s\n", name.c_str(), saveDir.c_str());
    fflush(stdout);
    fs::create_directories(saveDir);


    json worldData = serializeWorldData(gs);
    std::ofstream wf(saveDir + "worldData.json");
    if (wf.is_open()) { wf << worldData.dump(2); wf.close(); }


    json countryData = json::object();
    for (auto& cName : gs.countryList) {
        auto it = gs.countries.find(cName);
        if (it != gs.countries.end()) {
            countryData[cName] = serializeCountry(*it->second);
        }
    }
    std::ofstream cf(saveDir + "countryData.json");
    if (cf.is_open()) { cf << countryData.dump(2); cf.close(); }


    json factionData = json::object();
    for (auto& fName : gs.factionList) {
        auto it = gs.factions.find(fName);
        if (it != gs.factions.end()) {
            factionData[fName] = serializeFaction(*it->second);
        }
    }
    std::ofstream ff(saveDir + "factionData.json");
    if (ff.is_open()) { ff << factionData.dump(2); ff.close(); }


    if (maps.getPoliticalMap()) saveMapSurface(maps.getPoliticalMap(), saveDir + "map.png");
    if (maps.getFactionMap()) saveMapSurface(maps.getFactionMap(), saveDir + "factions.png");
    if (maps.getIdeologyMap()) saveMapSurface(maps.getIdeologyMap(), saveDir + "ideologies.png");
    if (maps.getIndustryMap()) saveMapSurface(maps.getIndustryMap(), saveDir + "industry.png");

    printf("[SaveSystem] Save complete for '%s'\n", name.c_str());
    fflush(stdout);
    return true;
}

bool SaveSystem::loadGame(GameState& gs, MapManager& maps, const std::string& name) {
    std::string saveDir = savesDir_ + name + "/";
    if (!fs::exists(saveDir)) return false;
    std::string worldPath = saveDir + "worldData.json";
    std::string countryPath = saveDir + "countryData.json";
    std::string factionPath = saveDir + "factionData.json";
    if (!fs::exists(worldPath) || !fs::exists(countryPath)) return false;

    json worldData;
    {
        std::ifstream wf(worldPath);
        wf >> worldData;
    }

    json countryData;
    {
        std::ifstream cf(countryPath);
        cf >> countryData;
    }

    json factionData = json::object();
    if (fs::exists(factionPath)) {
        std::ifstream ff(factionPath);
        ff >> factionData;
    }

    gs.clear();
    deserializeWorldData(gs, worldData);
    maps.loadMaps(gs.mapName);
    gs.politicalMapSurf = maps.getPoliticalMap();
    gs.ideologyMapSurf = maps.getIdeologyMap();
    gs.factionMapSurf = maps.getFactionMap();
    gs.industryMapSurf = maps.getIndustryMap();
    gs.cultureMapSurf = maps.getCultureMap();
    gs.regionsMapSurf = maps.getRegionsMap();

    for (auto& [cName, cData] : countryData.items()) {
        std::vector<int> regions;
        if (cData.contains("regions") && cData["regions"].is_array()) {
            regions = cData["regions"].get<std::vector<int>>();
        }
        auto country = std::make_unique<Country>(cName, regions, gs);
        gs.registerCountry(cName, std::move(country));
    }

    for (auto& [cName, cData] : countryData.items()) {
        auto* c = gs.getCountry(cName);
        if (c) {
            deserializeCountry(*c, cData, gs);
        }
    }

    if (factionData.is_object()) {
        for (auto& [fName, fData] : factionData.items()) {
            std::vector<std::string> members;
            if (fData.contains("members") && fData["members"].is_array()) {
                members = fData["members"].get<std::vector<std::string>>();
            }
            auto faction = std::make_unique<Faction>(fName, members, gs);
            gs.registerFaction(fName, std::move(faction));
        }

        for (auto& [fName, fData] : factionData.items()) {
            auto* f = gs.getFaction(fName);
            if (f) {
                deserializeFaction(*f, fData);
            }
        }
    }

    maps.reloadIndustryMap(gs);
    maps.generateResourceMap(gs);
    gs.inGame = true;
    gs.mapDirty = false;
    return true;
}

std::vector<SaveInfo> SaveSystem::listSaves() const {
    std::vector<SaveInfo> saves;
    if (!fs::exists(savesDir_)) return saves;
    for (auto& entry : fs::directory_iterator(savesDir_)) {
        if (entry.is_directory()) {
            SaveInfo info;
            info.name = entry.path().filename().string();
            std::string worldPath = entry.path().string() + "/worldData.json";
            if (fs::exists(worldPath)) {
                try {
                    std::ifstream f(worldPath);
                    json j; f >> j;
                    info.controlledCountry = j.value("controlledCountry", "");
                    int y = j.value("year", 0);
                    int m = j.value("month", 0);
                    int d = j.value("day", 0);
                    info.date = std::to_string(y) + "-" + std::to_string(m) + "-" + std::to_string(d);
                    info.valid = true;
                } catch (...) { info.valid = false; }
            }
            saves.push_back(info);
        }
    }
    return saves;
}

bool SaveSystem::deleteSave(const std::string& name) {
    std::string saveDir = savesDir_ + name;
    if (fs::exists(saveDir)) {
        fs::remove_all(saveDir);
        return true;
    }
    return false;
}

json SaveSystem::serializeWorldData(const GameState& gs) {
    json j;
    j["controlledCountry"] = gs.controlledCountry;
    j["mapName"] = gs.mapName;
    j["speed"] = gs.speed;
    j["hour"] = gs.time.hour;
    j["day"] = gs.time.day;
    j["month"] = gs.time.month;
    j["year"] = gs.time.year;
    j["startHour"] = gs.time.startHour;
    j["startDay"] = gs.time.startDay;
    j["startMonth"] = gs.time.startMonth;
    j["startYear"] = gs.time.startYear;
    j["totalDays"] = gs.time.totalDays;
    j["ports"] = gs.ports;
    j["canals"] = gs.canals;
    return j;
}

void SaveSystem::deserializeWorldData(GameState& gs, const json& j) {
    gs.controlledCountry = j.value("controlledCountry", "");
    gs.mapName = j.value("mapName", "Modern Day");
    gs.speed = j.value("speed", 0);
    gs.time.hour = j.value("hour", 1);
    gs.time.day = j.value("day", 1);
    gs.time.month = j.value("month", 1);
    gs.time.year = j.value("year", 2026);
    gs.time.startHour = j.value("startHour", 1);
    gs.time.startDay = j.value("startDay", 1);
    gs.time.startMonth = j.value("startMonth", 1);
    gs.time.startYear = j.value("startYear", 2026);
    gs.time.totalDays = j.value("totalDays", 0);
    if (j.contains("ports")) gs.ports = j["ports"].get<std::vector<int>>();
    if (j.contains("canals")) gs.canals = j["canals"].get<std::vector<int>>();
}

json SaveSystem::serializeCountry(const Country& c) {
    json j;
    j["name"] = c.name;
    j["regions"] = c.regions;
    j["regionsBeforeWar"] = c.regionsBeforeWar;
    j["coreRegions"] = c.coreRegions;
    j["money"] = c.money;
    j["factories"] = c.factories;
    j["population"] = c.population;
    j["manPower"] = c.manPower;
    j["usedManPower"] = c.usedManPower;
    j["politicalPower"] = c.politicalPower;
    j["politicalMultiplier"] = c.politicalMultiplier;
    j["baseStability"] = c.baseStability;
    j["stability"] = c.stability;
    j["ideology"] = {c.ideology[0], c.ideology[1]};
    j["ideologyName"] = c.ideologyName;
    j["culture"] = c.culture;
    j["capital"] = c.capital;
    j["cities"] = c.cities;
    j["deployRegion"] = c.deployRegion;
    j["faction"] = c.faction;
    j["factionLeader"] = c.factionLeader;
    j["atWarWith"] = c.atWarWith;
    j["militaryAccess"] = c.militaryAccess;
    j["militarySize"] = c.militarySize;
    j["buildSpeed"] = c.buildSpeed;
    j["moneyMultiplier"] = c.moneyMultiplier;
    j["attackMultiplier"] = c.attackMultiplier;
    j["defenseMultiplier"] = c.defenseMultiplier;
    j["transportMultiplier"] = c.transportMultiplier;
    j["puppetTo"] = c.puppetTo;
    j["capitulated"] = c.capitulated;
    j["canMakeFaction"] = c.canMakeFaction;
    j["expandedInvitations"] = c.expandedInvitations;
    j["hasChangedCountry"] = c.hasChangedCountry;
    j["warScore"] = c.warScore;


    j["leader"] = {
        {"name", c.leader.name}, {"ideology", c.leader.ideology},
        {"age", c.leader.age}, {"traitName", c.leader.traitName},
        {"economyBonus", c.leader.economyBonus}, {"militaryBonus", c.leader.militaryBonus},
        {"diplomacyBonus", c.leader.diplomacyBonus}, {"stabilityBonus", c.leader.stabilityBonus},
    };


    json res;
    for (int i = 0; i < RESOURCE_COUNT; ++i) {
        res["stockpile"].push_back(c.resourceManager.stockpile[i]);
        res["tradeImports"].push_back(c.resourceManager.tradeImports[i]);
        res["tradeExports"].push_back(c.resourceManager.tradeExports[i]);
    }
    j["resources"] = res;


    json buildings = json::object();
    for (auto& [rid, blist] : c.buildingManager.buildings) {
        json arr = json::array();
        for (auto& b : blist) {
            arr.push_back({{"type", buildingTypeToString(b.type)}, {"level", b.level}});
        }
        buildings[std::to_string(rid)] = arr;
    }
    j["buildings"] = buildings;


    json queue = json::array();
    for (auto& entry : c.buildingManager.queue) {
        queue.push_back({{"type", buildingTypeToString(entry.type)},
                         {"regionId", entry.regionId},
                         {"daysRemaining", entry.daysRemaining},
                         {"totalDays", entry.totalDays}});
    }
    j["constructionQueue"] = queue;


    json divs = json::array();
    for (auto& div : c.divisions) {
        divs.push_back({{"region", div->region}, {"divisionStack", div->divisionStack},
                        {"units", div->units}, {"maxUnits", div->maxUnits},
                        {"resources", div->resources}, {"maxResources", div->maxResources}});
    }
    j["divisions"] = divs;


    json tr = json::array();
    for (auto& cycle : c.training) {
        tr.push_back({cycle[0], cycle[1]});
    }
    j["training"] = tr;


    j["completedFocuses"] = std::vector<std::string>(
        c.focusTreeEngine.completedFocuses.begin(),
        c.focusTreeEngine.completedFocuses.end());
    if (c.focus.has_value()) {
        const auto& [name, days, effects] = *c.focus;
        j["currentFocus"] = {
            {"name", name},
            {"daysRemaining", days},
            {"effects", effects}
        };
    }

    return j;
}

json SaveSystem::serializeFaction(const Faction& f) {
    json j;
    j["name"] = f.name;
    j["members"] = f.members;
    j["factionLeader"] = f.factionLeader;
    j["ideology"] = f.ideology;
    j["factionWar"] = f.factionWar;
    j["color"] = {f.color.r, f.color.g, f.color.b};
    return j;
}

void SaveSystem::saveMapSurface(SDL_Surface* surf, const std::string& path) {
    if (surf) IMG_SavePNG(surf, path.c_str());
}

SDL_Surface* SaveSystem::loadMapSurface(const std::string& path) {
    return Engine::instance().loadSurface(path);
}

void SaveSystem::deserializeCountry(Country& c, const json& j, GameState& gs) {
    c.money = j.value("money", 0.0f);
    c.population = j.value("population", 0.0f);
    c.manPower = j.value("manPower", 0.0f);
    c.usedManPower = j.value("usedManPower", 0.0f);
    c.politicalPower = j.value("politicalPower", 0.0f);
    c.politicalMultiplier = j.value("politicalMultiplier", 1.0f);
    c.baseStability = j.value("baseStability", 60.0f);
    c.stability = j.value("stability", 60.0f);
    c.ideologyName = j.value("ideologyName", "");
    c.faction = j.value("faction", "");
    c.factionLeader = j.value("factionLeader", false);
    c.militarySize = j.value("militarySize", 2);
    c.buildSpeed = j.value("buildSpeed", 1.0f);
    c.moneyMultiplier = j.value("moneyMultiplier", 1.0f);
    c.attackMultiplier = j.value("attackMultiplier", 1.0f);
    c.defenseMultiplier = j.value("defenseMultiplier", 1.0f);
    c.transportMultiplier = j.value("transportMultiplier", 1.0f);
    c.puppetTo = j.value("puppetTo", "");
    c.capitulated = j.value("capitulated", false);
    c.canMakeFaction = j.value("canMakeFaction", false);
    c.expandedInvitations = j.value("expandedInvitations", false);
    c.hasChangedCountry = j.value("hasChangedCountry", false);
    c.warScore = j.value("warScore", 0.0f);
    c.capital = j.value("capital", c.capital);
    c.deployRegion = j.value("deployRegion", c.deployRegion);

    if (j.contains("ideology") && j["ideology"].is_array() && j["ideology"].size() >= 2) {
        c.ideology[0] = j["ideology"][0].get<float>();
        c.ideology[1] = j["ideology"][1].get<float>();
    }
    if (j.contains("atWarWith")) c.atWarWith = j["atWarWith"].get<std::vector<std::string>>();
    if (j.contains("militaryAccess")) c.militaryAccess = j["militaryAccess"].get<std::vector<std::string>>();
    if (j.contains("regionsBeforeWar")) c.regionsBeforeWar = j["regionsBeforeWar"].get<std::vector<int>>();
    if (j.contains("coreRegions")) c.coreRegions = j["coreRegions"].get<std::vector<int>>();
    if (j.contains("cities")) c.cities = j["cities"].get<std::vector<std::string>>();


    if (j.contains("leader") && j["leader"].is_object()) {
        auto& lj = j["leader"];
        c.leader.name = lj.value("name", "");
        c.leader.ideology = lj.value("ideology", "");
        c.leader.age = lj.value("age", 50);
        c.leader.traitName = lj.value("traitName", "");
        c.leader.economyBonus = lj.value("economyBonus", 0.0f);
        c.leader.militaryBonus = lj.value("militaryBonus", 0.0f);
        c.leader.diplomacyBonus = lj.value("diplomacyBonus", 0.0f);
        c.leader.stabilityBonus = lj.value("stabilityBonus", 0.0f);
    }


    if (j.contains("resources") && j["resources"].is_object()) {
        auto& rj = j["resources"];
        if (rj.contains("stockpile") && rj["stockpile"].is_array()) {
            for (int i = 0; i < std::min((int)rj["stockpile"].size(), RESOURCE_COUNT); ++i)
                c.resourceManager.stockpile[i] = rj["stockpile"][i].get<float>();
        }
        if (rj.contains("tradeImports") && rj["tradeImports"].is_array()) {
            for (int i = 0; i < std::min((int)rj["tradeImports"].size(), RESOURCE_COUNT); ++i)
                c.resourceManager.tradeImports[i] = rj["tradeImports"][i].get<float>();
        }
        if (rj.contains("tradeExports") && rj["tradeExports"].is_array()) {
            for (int i = 0; i < std::min((int)rj["tradeExports"].size(), RESOURCE_COUNT); ++i)
                c.resourceManager.tradeExports[i] = rj["tradeExports"][i].get<float>();
        }
    }


    if (j.contains("buildings") && j["buildings"].is_object()) {
        c.buildingManager.buildings.clear();
        for (auto& [ridStr, bArr] : j["buildings"].items()) {
            int rid = std::stoi(ridStr);
            for (auto& bj : bArr) {
                BuildingType bt = buildingTypeFromString(bj.value("type", "civilian_factory"));
                int level = bj.value("level", 1);
                c.buildingManager.addBuilding(rid, bt, level);
            }
        }
    }


    if (j.contains("constructionQueue") && j["constructionQueue"].is_array()) {
        c.buildingManager.queue.clear();
        for (auto& qj : j["constructionQueue"]) {
            BuildingType bt = buildingTypeFromString(qj.value("type", "civilian_factory"));
            int rid = qj.value("regionId", -1);
            int daysRem = qj.value("daysRemaining", 0);
            int totalDays = qj.value("totalDays", 120);
            c.buildingManager.queueConstruction(bt, rid, daysRem);
            if (!c.buildingManager.queue.empty()) {
                c.buildingManager.queue.back().totalDays = totalDays;
            }
        }
    }


    if (j.contains("training") && j["training"].is_array()) {
        c.training.clear();
        for (auto& tj : j["training"]) {
            if (tj.is_array() && tj.size() >= 2) {
                c.training.push_back({tj[0].get<int>(), tj[1].get<int>()});
            }
        }
    }


    c.divisions.clear();
    if (j.contains("divisions") && j["divisions"].is_array()) {
        for (auto& dj : j["divisions"]) {
            int region = dj.value("region", -1);
            int stack = dj.value("divisionStack", 1);
            if (region < 0) continue;

            auto div = std::make_unique<Division>(c.name, stack, region, c.divisionColor, gs);
            div->units = dj.value("units", div->units);
            div->maxUnits = dj.value("maxUnits", div->maxUnits);
            div->resources = dj.value("resources", div->resources);
            div->maxResources = dj.value("maxResources", div->maxResources);
            c.divisions.push_back(std::move(div));
        }
    }


    c.focusTreeEngine.completed.clear();
    c.focusTreeEngine.completedFocuses.clear();
    if (j.contains("completedFocuses") && j["completedFocuses"].is_array()) {
        for (auto& fj : j["completedFocuses"]) {
            if (!fj.is_string()) continue;
            std::string focusName = fj.get<std::string>();
            c.focusTreeEngine.completed.push_back(focusName);
            c.focusTreeEngine.completedFocuses.insert(focusName);
            if (auto* node = c.focusTreeEngine.getNode(focusName)) {
                node->completed = true;
            }
        }
    }

    c.focus.reset();
    c.focusTreeEngine.currentFocus.clear();
    c.focusTreeEngine.daysRemaining = 0;
    c.focusTreeEngine.active = false;
    if (j.contains("currentFocus") && j["currentFocus"].is_object()) {
        const auto& cf = j["currentFocus"];
        std::string name = cf.value("name", "");
        int daysRemaining = cf.value("daysRemaining", 0);
        std::vector<std::string> effects;
        if (cf.contains("effects") && cf["effects"].is_array()) {
            effects = cf["effects"].get<std::vector<std::string>>();
        }
        if (!name.empty() && daysRemaining > 0) {
            c.focus = std::make_tuple(name, daysRemaining, effects);
            c.focusTreeEngine.currentFocus = name;
            c.focusTreeEngine.daysRemaining = daysRemaining;
            c.focusTreeEngine.active = true;
        }
    }

    if (std::find(c.militaryAccess.begin(), c.militaryAccess.end(), c.name) == c.militaryAccess.end()) {
        c.militaryAccess.push_back(c.name);
    }
    std::sort(c.militaryAccess.begin(), c.militaryAccess.end());
    c.militaryAccess.erase(std::unique(c.militaryAccess.begin(), c.militaryAccess.end()), c.militaryAccess.end());
    std::sort(c.atWarWith.begin(), c.atWarWith.end());
    c.atWarWith.erase(std::unique(c.atWarWith.begin(), c.atWarWith.end()), c.atWarWith.end());

    c.factories = c.buildingManager.countAll(BuildingType::CivilianFactory) +
                  c.buildingManager.countAll(BuildingType::MilitaryFactory);
}

void SaveSystem::deserializeFaction(Faction& f, const json& j) {
    f.name = j.value("name", "");
    if (j.contains("members")) f.members = j["members"].get<std::vector<std::string>>();
    f.factionLeader = j.value("factionLeader", "");
    f.ideology = j.value("ideology", "nonaligned");
    if (j.contains("factionWar")) f.factionWar = j["factionWar"].get<std::vector<std::string>>();
    if (j.contains("color") && j["color"].is_array() && j["color"].size() >= 3) {
        f.color = {j["color"][0].get<uint8_t>(), j["color"][1].get<uint8_t>(), j["color"][2].get<uint8_t>()};
    }
}

void SaveSystem::deserializeGameTime(GameTime& t, const json& j) {
    t.hour = j.value("hour", 1);
    t.day = j.value("day", 1);
    t.month = j.value("month", 1);
    t.year = j.value("year", 2026);
}

json SaveSystem::serializeGameTime(const GameTime& t) {
    return {{"hour", t.hour}, {"day", t.day}, {"month", t.month}, {"year", t.year}};
}
