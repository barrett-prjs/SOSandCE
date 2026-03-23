#pragma once
#include "screens/screen.h"
#include "map/map_renderer.h"

class DecisionTreeScreen : public Screen {
public:
    void enter(App& app) override;
    void handleInput(App& app, const InputState& input) override;
    void update(App& app, float dt) override;
    void render(App& app) override;

private:
    Camera treeCam_;
    int hoveredFocus_ = -1;
    std::string selectedFocus_;
};
