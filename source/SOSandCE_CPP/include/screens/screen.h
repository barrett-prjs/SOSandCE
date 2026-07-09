#pragma once
#include "core/common.h"

class App;
struct InputState;

enum class ScreenType {
    NONE,
    MAIN_MENU,
    SETTINGS,
    COUNTRY_SELECT,
    MAP_SELECT,
    GAME,
    PEACE_CONFERENCE,
    SAVE_LOAD,
    DECISION_TREE,
    QUIT
};

class Screen {
public:
    virtual ~Screen() = default;

    virtual void enter(App& app) {}
    virtual void exit(App& app) {}
    virtual void handleInput(App& app, const InputState& input) = 0;
    virtual void update(App& app, float dt) = 0;
    virtual void render(App& app) = 0;

    ScreenType nextScreen = ScreenType::NONE;
};

class ScreenManager {
public:
    void registerScreen(ScreenType type, std::unique_ptr<Screen> screen);
    void switchTo(ScreenType type, App& app);
    void update(App& app, float dt);
    void render(App& app);
    void handleInput(App& app, const InputState& input);
    void clear(App& app);
    Screen* get(ScreenType type) const;
    Screen* current() const { return current_; }
    ScreenType currentType() const { return currentType_; }

private:
    std::unordered_map<int, std::unique_ptr<Screen>> screens_;
    Screen* current_ = nullptr;
    ScreenType currentType_ = ScreenType::NONE;
};
