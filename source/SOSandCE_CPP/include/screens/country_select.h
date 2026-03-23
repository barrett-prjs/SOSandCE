#pragma once
#include "screens/screen.h"

class CountrySelectScreen : public Screen {
public:
    void enter(App& app) override;
    void handleInput(App& app, const InputState& input) override;
    void update(App& app, float dt) override;
    void render(App& app) override;

    std::string selectedCountry;

private:
    std::vector<std::string> countries_;
    int scrollOffset_ = 0;
    int hoveredIndex_ = -1;
    std::string searchFilter_;
};
