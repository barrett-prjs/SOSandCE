#pragma once
#include "screens/screen.h"

class SettingsScreen : public Screen {
public:
    enum class SettingsTab {
        INTERFACE,
        AUDIO,
        VIDEO,
        GAMEPLAY
    };

    ScreenType returnScreen = ScreenType::MAIN_MENU;

    void enter(App& app) override;
    void handleInput(App& app, const InputState& input) override;
    void update(App& app, float dt) override;
    void render(App& app) override;

private:
    void applySettings(App& app);

    SettingsTab activeTab_ = SettingsTab::INTERFACE;
    float tempUISize_ = 24.0f;
    float tempMusicVol_ = 1.0f;
    float tempSoundVol_ = 1.0f;
    int tempFPS_;
    bool tempFog_ = true;
    bool tempSidebarLeft_ = false;
    bool tempFullscreen_ = true;
};
