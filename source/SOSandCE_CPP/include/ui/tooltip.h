#pragma once
#include "core/common.h"

class Tooltip {
public:
    void show(const std::vector<std::string>& lines, float x, float y);
    void hide();
    void render(SDL_Renderer* r, int screenW, int screenH);
    bool isVisible() const { return visible_; }

private:
    std::vector<std::string> lines_;
    float x_ = 0, y_ = 0;
    bool visible_ = false;
};

class RegionTooltip {
public:
    void showAt(float x, float y, const std::string& header,
                const std::vector<std::pair<std::string, Color>>& lines,
                SDL_Texture* flagTex = nullptr, int screenW = 1200, int screenH = 675);
    void hide() { visible_ = false; }
    void render(SDL_Renderer* r);
    bool isVisible() const { return visible_; }
    bool containsPoint(float px, float py) const;

private:
    std::string header_;
    std::vector<std::pair<std::string, Color>> lines_;
    SDL_Texture* flagTex_ = nullptr;
    float x_ = 0, y_ = 0, w_ = 0, h_ = 0;
    bool visible_ = false;
};
