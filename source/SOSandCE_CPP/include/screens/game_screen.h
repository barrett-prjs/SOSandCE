#pragma once
#include "screens/screen.h"
#include "map/map_manager.h"
#include "map/map_renderer.h"
#include "ui/click_registry.h"

class UIManager;
class Sidebar;
class TopBar;
class PopupStack;

class GameScreen : public Screen {
public:
    GameScreen();
    ~GameScreen();

    void enter(App& app) override;
    void exit(App& app) override;
    void handleInput(App& app, const InputState& input) override;
    void update(App& app, float dt) override;
    void render(App& app) override;


    void setupGame(App& app, const std::string& scenario, int eventFrequency = 14);
    bool loadGame(App& app, const std::string& saveName);
    void resetSessionState();
    void startCampaignAsCountry(App& app, const std::string& countryName);
    void startSpectatorMode(App& app);
    void takeControlOfCountry(App& app, const std::string& countryName);


    MapManager& mapManager() { return mapManager_; }
    Camera& camera() { return camera_; }

private:
    MapManager mapManager_;
    MapRenderer mapRenderer_;
    Camera camera_;
    ClickRegistry clickRegistry_;


    std::string clicked_;
    std::string openedTab_;
    std::string openedPoliticalTab_;
    std::string currentlyBuilding_ = "civilian_factory";
    bool showDivisions_ = true;
    bool showCities_ = true;
    bool showResources_ = false;
    bool showUI_ = true;
    bool oldDivisions_ = false;
    float sideBarAnimation_ = 0;
    float sideBarSize_ = 0.2f;
    float sideBarScroll_ = 0;
    bool holdingSideBar_ = false;


    std::vector<int> selectedRegions_;
    std::vector<class Division*> selectedDivisions_;
    int xPressed_ = 0, yPressed_ = 0;
    uint32_t timePressed_ = 0;
    bool pressed_ = false;


    std::unique_ptr<UIManager> uiManager_;
    std::unique_ptr<Sidebar> sidebar_;


    SDL_Rect speedMinusRect_ = {0,0,0,0};
    SDL_Rect speedPlusRect_ = {0,0,0,0};
    SDL_Rect spectatorPlayRect_ = {0,0,0,0};


    void handleCameraInput(const InputState& input, App& app);
    void clampCameraToMapBounds(int screenW, int screenH);
    void handleMapClick(App& app, const InputState& input);
    void handleMapRightClick(App& app, const InputState& input);
    void handleKeyboard(App& app, const InputState& input);
    bool isMapViewMode(const class GameState& gs) const;
    bool isSpectatorMode(const class GameState& gs) const;
    void openCountrySidebar(App& app, const std::string& countryName);
    void openOverlaySidebar(App& app, int regionId, const std::string& owner, MapMode mode);
    void focusCameraOnCountry(const std::string& countryName, App& app);
    bool divisionHitAtScreen(const class Division* div, const class GameState& gs, int screenX, int screenY, int screenW, int screenH) const;
    bool divisionInsideRect(const class Division* div, const class GameState& gs, int minX, int minY, int maxX, int maxY, int screenW, int screenH) const;


    void renderMap(App& app);
    void renderUI(App& app);
    void renderPopups(App& app);


    void showTutorialPopup(GameState& gs);
    void showStartPopup(GameState& gs);
};
