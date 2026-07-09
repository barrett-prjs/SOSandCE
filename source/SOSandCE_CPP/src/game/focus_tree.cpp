#include "game/focus_tree.h"
#include "game/game_state.h"
#include "game/country.h"
#include "game/effect_system.h"
#include "game/helpers.h"
#include "core/engine.h"
#include "data/data_loader.h"
#include "data/json.hpp"

#include <fstream>

using json = nlohmann::json;


FocusTreeLoader::FocusTreeLoader() = default;

static std::vector<FocusNode> parseJsonTree(const json& j) {
    std::vector<FocusNode> nodes;
    if (!j.is_object()) return nodes;

    for (auto& [name, val] : j.items()) {
        if (!val.is_object()) continue;

        FocusNode node;
        node.name = name;
        node.position.x = val.value("x", 0.0f);
        node.position.y = val.value("y", 0.0f);
        node.cost = val.value("cost", 0);
        node.days = val.value("days", 70);
        node.description = val.value("description", "");
        node.requirementDesc = val.value("requirement_desc", "");
        node.condition = val.value("condition", "True");

        if (val.contains("prerequisites") && val["prerequisites"].is_array()) {
            for (auto& p : val["prerequisites"]) {
                if (p.is_string()) node.prerequisites.push_back(p.get<std::string>());
            }
        }

        if (val.contains("effects") && val["effects"].is_array()) {
            for (auto& e : val["effects"]) {
                if (e.is_string()) node.effects.push_back(e.get<std::string>());
            }
        }

        if (val.contains("exclusive_group") && val["exclusive_group"].is_array()) {
            for (auto& mx : val["exclusive_group"]) {
                if (mx.is_string()) node.mutuallyExclusive.push_back(mx.get<std::string>());
            }
        }

        node.completed = false;
        node.available = true;
        nodes.push_back(std::move(node));
    }
    return nodes;
}

std::vector<FocusNode> FocusTreeLoader::load(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) return {};

    try {
        json j = json::parse(file);
        return parseJsonTree(j);
    } catch (...) {
        return {};
    }
}

std::vector<FocusNode> FocusTreeLoader::loadForCountry(const std::string& countryName,
                                                         GameState& ) {

    auto it = cache.find(countryName);
    if (it != cache.end()) {
        return it->second;
    }


    auto& eng = Engine::instance();
    std::string basePath = eng.assetsPath + "base_data/";
    std::vector<FocusNode> tree = load(basePath + "tech_trees/global.json");


    std::string countryPath = basePath + "tech_trees/countries/" + countryName + ".json";
    std::vector<FocusNode> countryNodes = load(countryPath);


    if (!countryNodes.empty()) {

        std::set<std::pair<int, int>> countryPositions;
        for (auto& cn : countryNodes) {
            countryPositions.insert({static_cast<int>(cn.position.x),
                                     static_cast<int>(cn.position.y)});
        }


        std::set<std::string> removedNames;
        tree.erase(
            std::remove_if(tree.begin(), tree.end(), [&](const FocusNode& n) {
                auto pos = std::make_pair(static_cast<int>(n.position.x),
                                          static_cast<int>(n.position.y));
                if (countryPositions.count(pos)) {
                    removedNames.insert(n.name);
                    return true;
                }
                return false;
            }),
            tree.end());


        for (auto& node : tree) {
            node.prerequisites.erase(
                std::remove_if(node.prerequisites.begin(), node.prerequisites.end(),
                                [&](const std::string& p) { return removedNames.count(p) > 0; }),
                node.prerequisites.end());
        }


        for (auto& cn : countryNodes) {
            tree.push_back(std::move(cn));
        }
    }

    cache[countryName] = tree;
    return tree;
}


FocusTreeEngine::FocusTreeEngine() = default;

void FocusTreeEngine::loadTree(const std::vector<FocusNode>& tree) {
    nodes = tree;
    completed.clear();
    currentFocus.clear();
    daysRemaining = 0;
    active = false;
}

bool FocusTreeEngine::canStartFocus(const std::string& name, const Country* country) const {
    const FocusNode* node = getNode(name);
    if (!node) return false;


    if (completedFocuses.count(name)) return false;


    for (auto& mx : node->mutuallyExclusive) {
        if (completedFocuses.count(mx)) return false;
    }


    for (auto& prereq : node->prerequisites) {
        if (!completedFocuses.count(prereq)) return false;
    }


    if (country && !node->condition.empty() && node->condition != "True") {
        const std::string& cond = node->condition;
        if (cond == "self.faction == None") {
            if (!country->faction.empty()) return false;
        } else if (cond == "not self.faction == None and self.factionLeader") {
            if (country->faction.empty() || !country->factionLeader) return false;
        } else if (cond == "not self.militarySize == 4") {
            if (country->militarySize >= 4) return false;
        } else if (cond == "not self.puppetTo == None") {
            if (country->puppetTo.empty()) return false;
        } else if (cond == "not self.hasChangedCountry") {
            if (country->hasChangedCountry) return false;
        }

    }


    if (country && node->cost > 0) {
        if (country->politicalPower < static_cast<float>(node->cost)) return false;
    }

    return node->available;
}

void FocusTreeEngine::startFocus(const std::string& name) {
    const FocusNode* node = getNode(name);
    if (!node) return;
    if (!canStartFocus(name)) return;

    currentFocus = name;
    daysRemaining = node->days;
    active = true;
}

void FocusTreeEngine::cancelFocus() {
    currentFocus.clear();
    daysRemaining = 0;
    active = false;
}

bool FocusTreeEngine::update(GameState& gs, const std::string& countryName) {
    if (!active || currentFocus.empty()) return false;

    if (gs.speed == 0) return false;


    daysRemaining -= 1;

    if (daysRemaining <= 0) {
        completeFocus(currentFocus, gs, countryName);
        active = false;
        std::string justCompleted = currentFocus;
        currentFocus.clear();
        daysRemaining = 0;
        return true;
    }

    return false;
}

void FocusTreeEngine::completeFocus(const std::string& name, GameState& gs,
                                     const std::string& countryName) {
    FocusNode* node = getNode(name);
    if (!node) return;

    completed.push_back(name);
    node->completed = true;


    for (auto& mx : node->mutuallyExclusive) {
        FocusNode* mxNode = getNode(mx);
        if (mxNode) mxNode->available = false;
    }

    applyEffects(*node, gs, countryName);
}

void FocusTreeEngine::applyEffects(const FocusNode& node, GameState& gs,
                                    const std::string& countryName) {


    EffectSystem effectSys;

    std::vector<std::string> effectStrings;
    for (auto& eff : node.effects) {
        if (auto* s = std::get_if<std::string>(&eff)) {
            effectStrings.push_back(*s);
        }
    }

    if (!effectStrings.empty()) {
        auto effects = effectSys.parse(effectStrings, countryName);
        effectSys.executeBatch(effects, gs);
    }
}

FocusNode* FocusTreeEngine::getNode(const std::string& name) {
    for (auto& n : nodes) {
        if (n.name == name) return &n;
    }
    return nullptr;
}

const FocusNode* FocusTreeEngine::getNode(const std::string& name) const {
    for (auto& n : nodes) {
        if (n.name == name) return &n;
    }
    return nullptr;
}

std::vector<std::string> FocusTreeEngine::getAvailable() const {
    std::vector<std::string> result;
    for (auto& n : nodes) {
        if (canStartFocus(n.name)) {
            result.push_back(n.name);
        }
    }
    return result;
}

float FocusTreeEngine::progress() const {
    if (!active) return 0.0f;
    const FocusNode* node = getNode(currentFocus);
    if (!node || node->days <= 0) return 1.0f;
    return 1.0f - static_cast<float>(daysRemaining) / static_cast<float>(node->days);
}
