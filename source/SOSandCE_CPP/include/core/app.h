#pragma once

#include "core/engine.h"
#include "core/input.h"
#include "core/settings.h"
#include "screens/screen.h"

#include <memory>
#include <cstdint>


class GameState;

class App {
public:
    App();
    ~App();


    int run();


    void switchScreen(ScreenType type);
    Screen* currentScreen() const;
    Screen* screen(ScreenType type) const;
    void requestGameResetOnTransition();


    Settings&   settings() { return settings_; }
    InputState& input()    { return input_; }
    GameState&  gameState();


    static App& instance();

private:
    void initSDL();
    void loadAssets();
    void mainLoop();
    void handleResize(int w, int h);
    void resetGameSession();

    Settings   settings_;
    InputState input_;
    std::unique_ptr<GameState> gameState_;


    ScreenManager screens_;
    ScreenType pendingScreen_     = ScreenType::NONE;
    bool resetGameOnTransition_   = false;

    bool     running_       = true;
    uint32_t lastFrameTime_ = 0;
};
