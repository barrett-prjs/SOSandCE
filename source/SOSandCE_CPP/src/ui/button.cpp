#include "ui/button.h"
#include "core/input.h"
#include "ui/primitives.h"
#include "ui/theme.h"
#include "ui/ui_assets.h"


Button::Button(float x, float y, float w, float h, const std::string& label,
               std::function<void()> onClick)
    : label(label), onClick(std::move(onClick)) {
    this->x = x;
    this->y = y;
    this->w = w;
    this->h = h;
}


void Button::handleInput(const InputState& input) {
    if (!visible || !enabled) {
        hovered = false;
        pressed = false;
        return;
    }

    hovered = containsPoint(static_cast<float>(input.mouseX),
                             static_cast<float>(input.mouseY));

    if (hovered && input.mouseLeftDown) {
        pressed = true;
        if (onClick) {
            onClick();
        }
    } else {
        pressed = false;
    }
}


void Button::render(SDL_Renderer* r) {
    if (!visible) return;

    auto& assets = UIAssets::instance();

    float drawY = (pressed && hovered) ? y + 1 : y;

    SDL_Texture* panelTex = assets.panelBodyHeadless();
    if (panelTex) {
        uint8_t mod = !enabled ? 148 : (pressed && hovered ? 214 : (hovered ? 255 : 238));
        uint8_t alpha = !enabled ? 190 : (hovered ? 255 : 246);
        SDL_SetTextureColorMod(panelTex, mod, mod, mod);
        SDL_SetTextureAlphaMod(panelTex, alpha);
        UIAssets::draw9Slice(r, panelTex, x, drawY, w, h, 18);
        SDL_SetTextureColorMod(panelTex, 255, 255, 255);
        SDL_SetTextureAlphaMod(panelTex, 255);
    } else {

        Color bgColor = !enabled ? Theme::btn_disabled
                      : (pressed ? Theme::btn_press
                      : (hovered ? Theme::btn_hover : Theme::btn));
        UIPrim::drawRoundedRect(r, bgColor, x, drawY, w, h, 5,
                                hovered ? Theme::gold_dim : Theme::border);
    }

    UIPrim::drawHLine(r, hovered && enabled ? Theme::gold_dim : Color{Theme::border.r, Theme::border.g, Theme::border.b, 90},
                      x + 6, x + w - 6, drawY + h - 2, 1);


    int textSize = fontSize;
    if (textSize <= 0) {
        textSize = Theme::si(0.9f);
        if (textSize < 10) textSize = 10;
    }
    Color textColor = !enabled ? Theme::dark_grey
                    : (hovered ? Theme::gold : Theme::cream);

    if (!label.empty()) {
        if (value.empty()) {
            UIPrim::drawText(r, label, textSize,
                             x + w * 0.5f, drawY + h * 0.5f,
                             "center", textColor, true);
        } else {
            UIPrim::drawText(r, label, textSize,
                             x + Theme::s(0.5f), drawY + h * 0.5f,
                             "midleft", textColor);
            UIPrim::drawText(r, value, textSize,
                             x + w - Theme::s(0.5f), drawY + h * 0.5f,
                             "midright", Theme::gold);
        }
    }
}
