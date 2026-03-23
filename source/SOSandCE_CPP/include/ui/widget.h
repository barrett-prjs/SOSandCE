#pragma once
#include "core/common.h"

struct InputState;

class Widget {
public:
    float x = 0, y = 0, w = 0, h = 0;
    bool visible = true;
    bool hovered = false;
    bool pressed = false;
    int zOrder = 0;

    virtual ~Widget() = default;
    virtual void handleInput(const InputState& input) {}
    virtual void update(float dt) {}
    virtual void render(SDL_Renderer* r) = 0;

    bool containsPoint(float px, float py) const {
        return px >= x && px <= x + w && py >= y && py <= y + h;
    }
};
