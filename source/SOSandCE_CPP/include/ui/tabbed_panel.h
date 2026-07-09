#pragma once
#include "ui/panel.h"

class TabbedPanel : public Panel {
public:
    std::vector<std::string> tabs;
    int activeTab = 0;
    float tabHeight = 0;


    std::function<float(SDL_Renderer* r, int tabIndex, float x, float y, float w)> drawTab;

    TabbedPanel() = default;
    TabbedPanel(float x, float y, float w, float h, const std::string& title,
                const std::vector<std::string>& tabs);

    void handleInput(const InputState& input) override;
    void render(SDL_Renderer* r) override;
};
