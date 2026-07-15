#include "ui/notification_log.h"
#include "ui/primitives.h"
#include "ui/theme.h"
#include "ui/hoi4.h"
#include "core/engine.h"
#include "game/game_state.h"
#include "game/helpers.h"

NotificationLog& NotificationLog::instance() {
    static NotificationLog s;
    return s;
}

NotificationLog& notifyLog() {
    return NotificationLog::instance();
}

void NotificationLog::add(NoteCat cat, const std::string& text, const GameState& gs) {
    Entry e;
    e.cat = cat;
    e.text = text;
    e.date = std::to_string(gs.time.day) + " " + Helpers::getMonthName(gs.time.month).substr(0, 3);
    entries_.push_front(e);
    while (entries_.size() > 120) entries_.pop_back();
    if (collapsed_) unseen_++;
}

void NotificationLog::clear() {
    entries_.clear();
    unseen_ = 0;
    scroll_ = 0.0f;
}

bool NotificationLog::wantsMouse(int mx, int my) const {
    return mx >= panelRect_.x && mx <= panelRect_.x + panelRect_.w &&
           my >= panelRect_.y && my <= panelRect_.y + panelRect_.h;
}

bool NotificationLog::handleScroll(int mx, int my, int scrollY) {
    if (!wantsMouse(mx, my) || collapsed_) return false;
    scroll_ = std::max(0.0f, scroll_ - scrollY * 2.0f);
    return true;
}

void NotificationLog::render(SDL_Renderer* r, int screenW, int screenH) {
    auto& eng = Engine::instance();
    float u = screenH / 100.0f * eng.uiScaleFactor();

    float panelW = std::round(u * 24.0f);
    float headerH = std::round(u * 2.7f);
    float chipH = std::round(u * 2.1f);
    float rowH = std::round(u * 2.05f);
    int maxRows = 8;

    if (x_ < 0.0f) {
        float topBarH = std::round(std::max(56.0f, u * 5.5f));
        x_ = std::round(u * 1.0f);
        y_ = topBarH + std::round(u * 0.8f);
    }

    std::vector<int> visible;
    if (!collapsed_) {
        for (int i = 0; i < static_cast<int>(entries_.size()); i++) {
            if (catEnabled_[static_cast<int>(entries_[i].cat)]) visible.push_back(i);
        }
    }
    int rows = collapsed_ ? 0 : std::min<int>(maxRows, std::max<int>(1, static_cast<int>(visible.size())));
    float bodyH = collapsed_ ? 0.0f : (chipH + rows * rowH + u * 0.5f);
    float panelH = headerH + bodyH;

    x_ = std::clamp(x_, 0.0f, std::max(0.0f, screenW - panelW));
    y_ = std::clamp(y_, 0.0f, std::max(0.0f, screenH - panelH));

    int mx, my;
    Uint32 mstate = SDL_GetMouseState(&mx, &my);
    bool down = (mstate & SDL_BUTTON_LMASK) != 0;
    bool clicked = down && !prevMouseDown_;

    bool overHeader = mx >= x_ && mx <= x_ + panelW && my >= y_ && my <= y_ + headerH;
    if (clicked && overHeader) {
        dragging_ = true;
        dragDX_ = mx - x_;
        dragDY_ = my - y_;
    }
    if (dragging_) {
        if (down) {
            x_ = std::clamp(mx - dragDX_, 0.0f, std::max(0.0f, screenW - panelW));
            y_ = std::clamp(my - dragDY_, 0.0f, std::max(0.0f, screenH - panelH));
        } else {
            dragging_ = false;
        }
    }

    panelRect_ = {static_cast<int>(x_), static_cast<int>(y_),
                  static_cast<int>(panelW), static_cast<int>(panelH)};

    HOI4::panel(r, x_, y_, panelW, panelH);
    UIPrim::drawGradientV(r, x_ + 2, y_ + 2, panelW - 4, headerH - 2,
                          {54, 58, 52, 255}, {34, 37, 33, 255});
    HOI4::goldDivider(r, x_ + 4, x_ + panelW - 4, y_ + headerH - 1);

    int hdrFs = std::max(12, static_cast<int>(headerH * 0.42f));
    std::string title = HOI4::tracked("Log");
    if (collapsed_ && unseen_ > 0) title += "  (" + std::to_string(unseen_) + ")";
    UIPrim::drawText(r, title, hdrFs, x_ + u * 0.8f, y_ + headerH * 0.5f, "midleft",
                     (collapsed_ && unseen_ > 0) ? Theme::gold_bright : Theme::gold, true);

    float tglSz = headerH * 0.62f;
    float tglX = x_ + panelW - tglSz - u * 0.5f;
    float tglY = y_ + (headerH - tglSz) * 0.5f;
    bool tglHov = mx >= tglX && mx <= tglX + tglSz && my >= tglY && my <= tglY + tglSz;
    HOI4::button(r, tglX, tglY, tglSz, tglSz, tglHov, false, true);
    UIPrim::drawText(r, collapsed_ ? "+" : "-", hdrFs, tglX + tglSz * 0.5f, tglY + tglSz * 0.45f,
                     "center", Theme::cream);
    if (clicked && tglHov) {
        collapsed_ = !collapsed_;
        dragging_ = false;
        if (!collapsed_) unseen_ = 0;
    }

    if (!collapsed_) {
        static const char* chipNames[5] = {"War", "Diplo", "Focus", "Build", "Army"};
        float chipY = y_ + headerH + u * 0.25f;
        float chipGap = std::round(u * 0.3f);
        float chipW = (panelW - u * 1.0f - chipGap * 4.0f) / 5.0f;
        int chipFs = std::max(10, static_cast<int>(chipH * 0.42f));
        for (int c = 0; c < 5; c++) {
            float cx = x_ + u * 0.5f + c * (chipW + chipGap);
            bool hov = mx >= cx && mx <= cx + chipW && my >= chipY && my <= chipY + chipH * 0.8f;
            HOI4::button(r, cx, chipY, chipW, chipH * 0.8f, hov, false, catEnabled_[c]);
            UIPrim::drawText(r, chipNames[c], chipFs, cx + chipW * 0.5f, chipY + chipH * 0.4f,
                             "center", catEnabled_[c] ? Theme::cream : Theme::dark_grey);
            if (clicked && hov) {
                catEnabled_[c] = !catEnabled_[c];
                scroll_ = 0.0f;
            }
        }

        float listY = chipY + chipH;
        int listFs = std::max(11, static_cast<int>(rowH * 0.5f));
        int dateFs = std::max(10, static_cast<int>(rowH * 0.42f));
        static const Color catColors[5] = {
            {208, 82, 66, 255}, {120, 160, 200, 255}, {216, 195, 134, 255},
            {150, 170, 120, 255}, {170, 150, 190, 255}
        };

        int first = static_cast<int>(scroll_);
        if (first > static_cast<int>(visible.size()) - rows) {
            first = std::max(0, static_cast<int>(visible.size()) - rows);
            scroll_ = static_cast<float>(first);
        }

        if (visible.empty()) {
            UIPrim::drawText(r, "No notifications", listFs, x_ + panelW * 0.5f,
                             listY + rowH * 0.5f, "center", Theme::dark_grey);
        }

        for (int vi = 0; vi < rows && first + vi < static_cast<int>(visible.size()); vi++) {
            const Entry& e = entries_[visible[first + vi]];
            float ry = listY + vi * rowH;
            SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(r, catColors[static_cast<int>(e.cat)].r,
                                   catColors[static_cast<int>(e.cat)].g,
                                   catColors[static_cast<int>(e.cat)].b, 210);
            SDL_Rect mark = {static_cast<int>(x_ + u * 0.5f), static_cast<int>(ry + rowH * 0.25f),
                             3, static_cast<int>(rowH * 0.5f)};
            SDL_RenderFillRect(r, &mark);
            UIPrim::drawText(r, e.date, dateFs, x_ + u * 1.1f, ry + rowH * 0.5f, "midleft", Theme::dark_grey);
            float textX = x_ + u * 4.6f;
            UIPrim::drawText(r, UIPrim::fitText(e.text, listFs, x_ + panelW - u * 0.6f - textX),
                             listFs, textX, ry + rowH * 0.5f, "midleft", Theme::cream);
        }

        if (static_cast<int>(visible.size()) > rows) {
            float trackX = x_ + panelW - 5.0f;
            float trackH = rows * rowH - 4.0f;
            float thumbH = std::max(14.0f, trackH * rows / std::max(1, (int)visible.size()));
            float denom = std::max(1.0f, static_cast<float>(visible.size() - rows));
            float thumbY = listY + 2.0f + (trackH - thumbH) * (scroll_ / denom);
            UIPrim::drawRectFilled(r, {0, 0, 0, 90}, trackX, listY + 2.0f, 3.0f, trackH);
            UIPrim::drawRectFilled(r, {Theme::gold_dim.r, Theme::gold_dim.g, Theme::gold_dim.b, 170},
                                   trackX, thumbY, 3.0f, thumbH);
        }
    }

    prevMouseDown_ = down;
}
