#pragma once
#include "screens/screen.h"

class MapSelectScreen : public Screen {
public:
    void enter(App& app) override;
    void handleInput(App& app, const InputState& input) override;
    void update(App& app, float dt) override;
    void render(App& app) override;

    std::string selectedMap;

private:
    struct ScenarioInfo {
        std::string name;
        std::string creator;
        std::string startDate;
    };
    std::vector<ScenarioInfo> scenarios_;
    int hoveredIndex_ = -1;
};
