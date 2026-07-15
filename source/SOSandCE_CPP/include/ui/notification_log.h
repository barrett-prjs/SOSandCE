#pragma once

#include "core/common.h"
#include <deque>

class GameState;

enum class NoteCat { War = 0, Diplomacy = 1, Focus = 2, Construction = 3, Military = 4 };

class NotificationLog {
public:
    static NotificationLog& instance();

    void add(NoteCat cat, const std::string& text, const GameState& gs);
    void clear();

    void render(SDL_Renderer* r, int screenW, int screenH);
    bool wantsMouse(int mx, int my) const;
    bool handleScroll(int mx, int my, int scrollY);

private:
    struct Entry {
        NoteCat cat;
        std::string date;
        std::string text;
    };

    std::deque<Entry> entries_;
    bool catEnabled_[5] = {true, true, true, true, true};
    float x_ = -1.0f, y_ = -1.0f;
    bool collapsed_ = true;
    float scroll_ = 0.0f;
    bool dragging_ = false;
    float dragDX_ = 0.0f, dragDY_ = 0.0f;
    bool prevMouseDown_ = false;
    SDL_Rect panelRect_{0, 0, 0, 0};
    int unseen_ = 0;
};

NotificationLog& notifyLog();
