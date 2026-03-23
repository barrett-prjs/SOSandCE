#pragma once
#include "core/common.h"
#include "ui/tooltip.h"

class App;
class GameState;

struct TopBarSlot {
    std::string label;
    std::string value;
    Color valueColor = {210, 200, 172};
    SDL_Texture* icon = nullptr;
    std::vector<std::string> tipLines;
};

class TopBar {
public:
    TopBar();

    void rebuild(App& app, GameState& gs, int width);
    void render(SDL_Renderer* r);
    void updateHover(int mx, int my);
    bool hit(int mx, int my) const;
    int height() const { return height_; }

public:
    Tooltip tooltip_;
private:
    int width_ = 0, height_ = 0;
    SDL_Texture* surface_ = nullptr;
    bool visible_ = true;

    struct SlotRect {
        SDL_Rect rect;
        std::vector<std::string> tip;
    };
    std::vector<SlotRect> slotRects_;
};
