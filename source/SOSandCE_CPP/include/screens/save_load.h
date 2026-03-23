#pragma once
#include "screens/screen.h"

class SaveLoadScreen : public Screen {
public:
    enum class Mode { SAVE, LOAD };
    Mode mode = Mode::SAVE;
    ScreenType returnScreen = ScreenType::GAME;

    void enter(App& app) override;
    void handleInput(App& app, const InputState& input) override;
    void update(App& app, float dt) override;
    void render(App& app) override;

private:
    struct SaveEntry {
        std::string name;
        std::string date;
        bool exists;
    };
    std::vector<SaveEntry> saves_;
    int hoveredIndex_ = -1;
    std::string newSaveName_;
    bool enteringName_ = false;
};
