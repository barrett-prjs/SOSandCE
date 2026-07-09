#include "ui/panel.h"
#include "core/input.h"
#include "ui/primitives.h"
#include "ui/theme.h"
#include "ui/ui_assets.h"


Panel::Panel(float x, float y, float w, float h, const std::string& title)
    : title(title) {
    this->x = x;
    this->y = y;
    this->w = w;
    this->h = h;
}


float Panel::headerHeight() const {
    return Theme::s(2.5f);
}


void Panel::scroll(float delta) {
    scrollY -= delta;
    if (scrollY < 0) scrollY = 0;
    if (scrollY > maxScroll) scrollY = maxScroll;
}


void Panel::handleInput(const InputState& input) {
    if (!visible) return;

    float mx = static_cast<float>(input.mouseX);
    float my = static_cast<float>(input.mouseY);

    hovered = containsPoint(mx, my);


    if (draggable) {
        float hh = headerHeight();
        bool inHeader = (mx >= x && mx <= x + w && my >= y && my <= y + hh);

        if (input.mouseLeftDown && inHeader) {

            if (closeable) {
                float closeX = x + w - Theme::s(2.0f);
                float closeY2 = y + Theme::s(0.25f);
                float closeSize = Theme::s(1.5f);
                if (mx >= closeX && mx <= closeX + closeSize &&
                    my >= closeY2 && my <= closeY2 + closeSize) {
                    visible = false;
                    if (onClose) onClose();
                    return;
                }
            }

            dragging_ = true;
            dragOffX_ = mx - x;
            dragOffY_ = my - y;
        }

        if (dragging_ && input.mouseLeft) {
            x = mx - dragOffX_;
            y = my - dragOffY_;
        }

        if (input.mouseLeftUp) {
            dragging_ = false;
        }
    }


    if (hovered && input.scrollY != 0) {
        scroll(input.scrollY * Theme::s(2.0f));
    }
}


void Panel::render(SDL_Renderer* r) {
    if (!visible) return;

    float hh = headerHeight();
    auto& assets = UIAssets::instance();


    UIPrim::drawShadow(r, x, y, w, h, 50, 12);
    UIPrim::drawShadow(r, x, y, w, h, 80, 4);


    SDL_Texture* bodyTex = title.empty() ? assets.panelBodyHeadless() : assets.panelBodyHeaded();
    if (bodyTex) {
        SDL_SetTextureAlphaMod(bodyTex, static_cast<uint8_t>(alpha));
        UIAssets::draw9Slice(r, bodyTex, x, y + (title.empty() ? 0 : hh), w,
                            h - (title.empty() ? 0 : hh), 18);
        SDL_SetTextureAlphaMod(bodyTex, 255);
    } else {
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(r, Theme::panel.r, Theme::panel.g, Theme::panel.b,
                               static_cast<uint8_t>(alpha));
        SDL_Rect bodyRect = {
            static_cast<int>(x), static_cast<int>(y),
            static_cast<int>(w), static_cast<int>(h)
        };
        SDL_RenderFillRect(r, &bodyRect);
        UIPrim::drawRect(r, Theme::border, x, y, w, h, Theme::border, 0, 1);
    }


    renderHeader(r);


    float contentTop = y + hh;
    float contentH = h - hh;

    SDL_Rect clipRect = {
        static_cast<int>(x + 1),
        static_cast<int>(contentTop),
        static_cast<int>(w - 2),
        static_cast<int>(contentH)
    };
    SDL_RenderSetClipRect(r, &clipRect);


    if (drawContent) {
        float pad = Theme::s(0.5f);
        contentHeight = drawContent(r, x + pad, contentTop + pad - scrollY, w - pad * 2);
        maxScroll = std::max(0.0f, contentHeight - contentH);
    }

    SDL_RenderSetClipRect(r, nullptr);


    if (maxScroll > 0) {
        renderScrollbar(r);
    }
}


void Panel::renderHeader(SDL_Renderer* r) {
    if (title.empty()) return;

    float hh = headerHeight();
    auto& assets = UIAssets::instance();


    SDL_Texture* headerTex = assets.panelHeader();
    if (headerTex) {
        UIAssets::draw9Slice(r, headerTex, x, y, w, hh, 12);
    } else {
        UIPrim::drawRectFilled(r, Theme::header, x, y, w, hh);
    }


    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, 255, 255, 255, 6);
    SDL_Rect topHl = {(int)x + 4, (int)y + 1, (int)w - 8, (int)(hh * 0.3f)};
    SDL_RenderFillRect(r, &topHl);


    UIPrim::drawHLine(r, Theme::gold_dim, x + 2, x + w - 2, y + hh - 1, 1);


    UIPrim::drawText(r, title, Theme::si(1.0f),
                     x + Theme::s(0.8f), y + hh * 0.5f,
                     "midleft", Theme::gold_bright, true);


    if (closeable) {
        float closeX = x + w - Theme::s(2.0f);
        float closeY2 = y + (hh - Theme::s(1.5f)) * 0.5f;
        float closeSize = Theme::s(1.5f);

        int mx, my;
        SDL_GetMouseState(&mx, &my);
        bool closeHovered = (mx >= closeX && mx <= closeX + closeSize &&
                             my >= closeY2 && my <= closeY2 + closeSize);

        Color closeBg = closeHovered ? Theme::btn_danger : Theme::btn;
        UIPrim::drawRoundedRect(r, closeBg, closeX, closeY2, closeSize, closeSize, 3,
                                closeHovered ? Theme::red : Theme::border);


        SDL_Texture* crossIcon = assets.icon("Cross");
        if (crossIcon) {
            float iconPad = closeSize * 0.22f;
            SDL_SetTextureColorMod(crossIcon, closeHovered ? 255 : 200, closeHovered ? 200 : 195, closeHovered ? 200 : 185);
            SDL_Rect iDst = {(int)(closeX + iconPad), (int)(closeY2 + iconPad),
                            (int)(closeSize - iconPad * 2), (int)(closeSize - iconPad * 2)};
            SDL_RenderCopy(r, crossIcon, nullptr, &iDst);
            SDL_SetTextureColorMod(crossIcon, 255, 255, 255);
        } else {
            UIPrim::drawText(r, "X", Theme::si(0.8f),
                             closeX + closeSize * 0.5f, closeY2 + closeSize * 0.5f,
                             "center", Theme::cream);
        }
    }
}


void Panel::renderScrollbar(SDL_Renderer* r) {
    float hh = headerHeight();
    float contentH = h - hh;
    float scrollbarW = Theme::s(0.35f);
    float scrollbarX = x + w - scrollbarW - 3;
    float scrollbarY = y + hh + 4;
    float scrollbarH = contentH - 8;


    UIPrim::drawRoundedRect(r, Theme::bg_dark, scrollbarX, scrollbarY,
                            scrollbarW, scrollbarH, 2);


    float totalContent = contentHeight;
    if (totalContent <= 0) return;

    float thumbRatio = contentH / totalContent;
    if (thumbRatio >= 1.0f) return;

    float thumbH = std::max(Theme::s(1.5f), scrollbarH * thumbRatio);
    float thumbY = scrollbarY + (scrollbarH - thumbH) * (scrollY / maxScroll);


    UIPrim::drawRoundedRect(r, Theme::gold_dim, scrollbarX, thumbY,
                            scrollbarW, thumbH, 2);

    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, 255, 255, 255, 15);
    SDL_Rect thumbHl = {(int)(scrollbarX + 1), (int)(thumbY + 2),
                        (int)(scrollbarW - 2), (int)(thumbH * 0.4f)};
    SDL_RenderFillRect(r, &thumbHl);
}
