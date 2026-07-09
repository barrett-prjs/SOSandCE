#include "ui/top_bar.h"
#include "core/app.h"
#include "core/engine.h"
#include "game/game_state.h"

TopBar::TopBar() {
    auto& eng = Engine::instance();
    height_ = std::max(56, static_cast<int>(std::round(eng.HEIGHT / 100.0f * eng.uiScaleFactor() * 5.5f)));
}

void TopBar::rebuild(App& app, GameState& gs, int width) {
    width_ = width;
    auto& eng = Engine::instance();
    height_ = std::max(56, static_cast<int>(std::round(eng.HEIGHT / 100.0f * eng.uiScaleFactor() * 5.5f)));
}

bool TopBar::hit(int mx, int my) const {
    return my >= 0 && my < height_ && mx >= 0 && mx < width_;
}
