#pragma once
#include "screens/screen.h"

class MainMenuScreen : public Screen {
public:
    void enter(App& app) override;
    void exit(App& app) override;
    void handleInput(App& app, const InputState& input) override;
    void update(App& app, float dt) override;
    void render(App& app) override;

private:
    int hoveredButton_ = -1;
    float animTimer_ = 0;
    SDL_Texture* backgroundTex_ = nullptr;


    enum class MenuState { MAIN, MAPS, COUNTRIES, SAVES, SETTINGS };
    MenuState menuState_ = MenuState::MAIN;


    std::vector<std::string> mapList_;
    std::vector<std::string> countryNames_;
    std::vector<std::string> savesList_;
    std::string selectedMap_ = "Modern Day";
    std::string selectedCountry_;
    int scrollOffset_ = 0;

    void renderMainMenu(App& app);
    void renderMapsMenu(App& app);
    void renderCountriesMenu(App& app);
    void renderSavesMenu(App& app);
    void renderSettingsMenu(App& app);
};
