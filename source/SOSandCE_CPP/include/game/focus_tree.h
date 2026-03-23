#pragma once
#include "core/common.h"
#include <variant>

class GameState;


using FocusEffect = std::variant<int, float, std::string, bool, std::vector<std::string>>;

struct FocusNode {
    std::string              name;
    std::string              description;
    std::string              requirementDesc;
    std::string              condition;
    std::string              icon;
    int                      cost = 0;
    int                      days = 70;
    std::vector<std::string> prerequisites;
    std::vector<std::string> mutuallyExclusive;
    std::vector<FocusEffect> effects;
    Vec2                     position;
    bool                     completed = false;
    bool                     available = true;
};


class FocusTreeLoader {
public:
    FocusTreeLoader();


    std::vector<FocusNode> load(const std::string& path);
    std::vector<FocusNode> loadForCountry(const std::string& countryName, GameState& gs);


    std::unordered_map<std::string, std::vector<FocusNode>> cache;
};


class FocusTreeEngine {
public:
    FocusTreeEngine();


    std::vector<FocusNode> nodes;


    std::string currentFocus;
    int         daysRemaining = 0;
    bool        active = false;


    std::vector<std::string> completed;
    std::set<std::string> completedFocuses;


    void loadTree(const std::vector<FocusNode>& tree);
    bool canStartFocus(const std::string& name, const class Country* country = nullptr) const;
    void startFocus(const std::string& name);
    void cancelFocus();
    bool update(GameState& gs, const std::string& countryName);
    void completeFocus(const std::string& name, GameState& gs, const std::string& countryName);
    void applyEffects(const FocusNode& node, GameState& gs, const std::string& countryName);


    FocusNode*       getNode(const std::string& name);
    const FocusNode* getNode(const std::string& name) const;
    std::vector<std::string> getAvailable() const;
    float progress() const;
};
