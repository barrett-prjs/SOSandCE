#pragma once
#include "ui/widget.h"

class Panel : public Widget {
public:
    std::string title;
    float scrollY = 0;
    float maxScroll = 0;
    float contentHeight = 0;
    bool closeable = true;
    bool draggable = true;
    int alpha = 230;
    std::function<void()> onClose;


    std::function<float(SDL_Renderer* r, float x, float y, float w)> drawContent;

    Panel() = default;
    Panel(float x, float y, float w, float h, const std::string& title = "");

    void handleInput(const InputState& input) override;
    void render(SDL_Renderer* r) override;
    void scroll(float delta);
    float headerHeight() const;

protected:
    void renderHeader(SDL_Renderer* r);
    void renderScrollbar(SDL_Renderer* r);
    bool dragging_ = false;
    float dragOffX_ = 0, dragOffY_ = 0;
};
