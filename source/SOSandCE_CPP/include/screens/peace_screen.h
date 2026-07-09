#pragma once
#include "screens/screen.h"
#include "map/map_renderer.h"
#include <vector>
#include <string>

class PeaceConference;

class PeaceScreen : public Screen {
public:
    ~PeaceScreen() override;
    void enter(App& app) override;
    void exit(App& app) override;
    void handleInput(App& app, const InputState& input) override;
    void update(App& app, float dt) override;
    void render(App& app) override;

    PeaceConference* conference = nullptr;


    std::string currentEnemy;

    std::vector<int> selectedProvinces;

private:
    void rebuildTreatyMap(App& app);
    void clampCameraToMapBounds(int screenW, int screenH);
    void advanceToNextEnemy();
    void resetCurrentDealState();

    int hoveredProvince_ = -1;
    int scrollOffset_ = 0;
    float sideScroll_ = 0;
    bool puppetRequested_ = false;
    bool installGovernmentRequested_ = false;
    bool demilitarizeRequested_ = false;
    bool reparationsRequested_ = false;
    Camera camera_;
    MapRenderer mapRenderer_;
    SDL_Surface* treatyMapSurface_ = nullptr;
};
