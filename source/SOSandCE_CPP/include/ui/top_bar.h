#pragma once
#include "core/common.h"
#include "ui/tooltip.h"

class App;
class GameState;

class TopBar {
public:
    TopBar();

    void rebuild(App& app, GameState& gs, int width);
    bool hit(int mx, int my) const;
    int height() const { return height_; }

    Tooltip tooltip_;

private:
    int width_ = 0, height_ = 0;
};
