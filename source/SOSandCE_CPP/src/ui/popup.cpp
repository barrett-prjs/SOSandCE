#include "ui/popup.h"
#include "core/engine.h"
#include "core/audio.h"
#include "core/input.h"
#include "ui/primitives.h"
#include "ui/theme.h"
#include "ui/ui_assets.h"
#include "ui/anim.h"


Popup::Popup()
    : xSize(22), ySize(6), xBase(0), yBase(0), x(0), y(0) {
    animId_ = "popup:" + std::to_string(reinterpret_cast<uintptr_t>(this));
    UIAnim::trigger(animId_);
}

Popup::Popup(const std::string& title, const std::vector<std::string>& text,
             const std::vector<PopupButton>& buttons, float xSize, float ySize,
             float x, float y, const std::string& flag1, const std::string& flag2,
             bool dismissOnClick)
    : title(title), text(text), buttons(buttons),
      xSize(xSize), ySize(ySize),
      flag1(flag1), flag2(flag2), dismissOnClick(dismissOnClick) {

    auto& eng = Engine::instance();
    float uiSize = eng.uiScale;

    animId_ = "popup:" + std::to_string(reinterpret_cast<uintptr_t>(this));
    UIAnim::trigger(animId_);

    if (x < 0) {
        xBase = eng.WIDTH * 0.5f;
        this->x = xBase;
    } else {
        xBase = x * uiSize;
        this->x = xBase;
    }

    if (y < 0) {
        yBase = eng.HEIGHT * 0.5f;
        this->y = yBase;
    } else {
        yBase = y * uiSize;
        this->y = yBase;
    }
}

Popup::~Popup() {
    UIAnim::forget(animId_);
}


void Popup::update(const InputState& input, float uiSize,
                   std::vector<std::unique_ptr<Popup>>& popupList) {
    float halfW = xSize * uiSize * 0.5f;
    float halfH = ySize * uiSize * 0.5f;
    float px = x - halfW;
    float py = y - halfH;
    float pw = xSize * uiSize;
    float ph = ySize * uiSize;

    int mx = input.mouseX;
    int my = input.mouseY;
    float fmx = static_cast<float>(mx);
    float fmy = static_cast<float>(my);

    float headerH = uiSize * 1.5f;
    bool inPopup = (fmx >= px && fmx <= px + pw && fmy >= py && fmy <= py + ph);


    if (holdingPopup_ && input.mouseLeft) {
        x = fmx - xOffset_;
        y = fmy - yOffset_;
    }
    if (input.mouseLeftUp) {
        holdingPopup_ = false;
    }


    if (input.mouseLeftDown && inPopup) {
        bool hitButton = false;
        for (auto& btn : buttons) {
            float bx = x + btn.xOffset * uiSize;
            float by = py + btn.yOffset * uiSize;
            float bw = btn.halfWidth * uiSize * 2;
            float bh = uiSize * 1.75f;

            float btnLeft = bx - bw * 0.5f;
            float btnTop = by - bh * 0.5f;

            if (fmx >= btnLeft && fmx <= btnLeft + bw &&
                fmy >= btnTop && fmy <= btnTop + bh) {
                hitButton = true;
                Audio::instance().playSound("clickedSound");
                bool keepOpen = btn.keepOpen;
                if (btn.callback) {
                    btn.callback();
                }

                if (!keepOpen) {
                    for (auto it = popupList.begin(); it != popupList.end(); ++it) {
                        if (it->get() == this) {
                            popupList.erase(it);
                            return;
                        }
                    }
                }
                break;
            }
        }

        if (!hitButton && dismissOnClick) {
            Audio::instance().playSound("clickedSound");
            for (auto it = popupList.begin(); it != popupList.end(); ++it) {
                if (it->get() == this) {
                    popupList.erase(it);
                    return;
                }
            }
        }


        bool inHeader = (fmx >= px && fmx <= px + pw && fmy >= py && fmy <= py + headerH);
        if (!hitButton && inHeader) {
            holdingPopup_ = true;
            xOffset_ = fmx - x;
            yOffset_ = fmy - y;
        }
    }
}


void Popup::draw(SDL_Renderer* r, float uiSize) {
    auto& engRef = Engine::instance();
    float halfW = xSize * uiSize * 0.5f;
    float halfH = ySize * uiSize * 0.5f;
    x = std::clamp(x, halfW, std::max(halfW, engRef.WIDTH - halfW));
    y = std::clamp(y, halfH, std::max(halfH, engRef.HEIGHT - halfH));
    float px = x - halfW;
    float py = y - halfH;
    float pw = xSize * uiSize;
    float ph = ySize * uiSize;

    float animT = UIAnim::progress(animId_);

    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, 0, 0, 0, static_cast<uint8_t>(100.0f * animT));
    auto& eng = Engine::instance();
    SDL_Rect fullScreen = {0, 0, eng.WIDTH, eng.HEIGHT};
    SDL_RenderFillRect(r, &fullScreen);

    UIAnim::pushMenuTransition(r, animId_, x, y);


    UIPrim::drawShadow(r, px, py, pw, ph, 80, 8);
    UIPrim::drawRoundedRect(r, Theme::surface1, px, py, pw, ph, 7,
                            {Theme::border.r, Theme::border.g, Theme::border.b, 140});


    float headerH = uiSize * 1.5f;
    UIPrim::drawRectFilled(r, Theme::header, px + 2, py + 2, pw - 4, headerH);


    UIPrim::drawHLine(r, Theme::gold_dim, px + 6, px + pw - 6, py + 2 + headerH, 1);


    int titleSize = std::max(12, static_cast<int>(uiSize * 0.9f));
    UIPrim::drawText(r, UIPrim::fitText(title, titleSize, pw - uiSize * 1.5f), titleSize,
                     x, py + 4 + headerH * 0.5f,
                     "center", Theme::gold_bright, true);


    float flagH = headerH - 10.0f;
    float flagW = flagH * 1.6f;
    if (!flag1.empty()) {
        SDL_Texture* flagTex = eng.loadTexture(eng.assetsPath + "flags/" + flag1 + ".png");
        if (flagTex) {
            SDL_Rect flagRect = {
                static_cast<int>(px + uiSize * 0.4f),
                static_cast<int>(py + 7),
                static_cast<int>(flagW),
                static_cast<int>(flagH)
            };
            SDL_RenderCopy(r, flagTex, nullptr, &flagRect);
            SDL_SetRenderDrawColor(r, Theme::border.r, Theme::border.g, Theme::border.b, 120);
            SDL_RenderDrawRect(r, &flagRect);
        }
    }

    if (!flag2.empty()) {
        SDL_Texture* flagTex = eng.loadTexture(eng.assetsPath + "flags/" + flag2 + ".png");
        if (flagTex) {
            SDL_Rect flagRect = {
                static_cast<int>(px + pw - uiSize * 0.4f - flagW),
                static_cast<int>(py + 7),
                static_cast<int>(flagW),
                static_cast<int>(flagH)
            };
            SDL_RenderCopy(r, flagTex, nullptr, &flagRect);
            SDL_SetRenderDrawColor(r, Theme::border.r, Theme::border.g, Theme::border.b, 120);
            SDL_RenderDrawRect(r, &flagRect);
        }
    }


    int textSize = std::max(10, static_cast<int>(uiSize * 0.65f));
    float textY = py + headerH + uiSize * 1.5f;
    float lineGap = uiSize * 0.85f;
    float textMaxW = pw - uiSize * 1.2f;

    for (auto& line : text) {
        int lineFs = textSize;
        while (lineFs > 9 && UIPrim::textWidth(line, lineFs) > textMaxW) {
            lineFs--;
        }
        UIPrim::drawText(r, UIPrim::fitText(line, lineFs, textMaxW), lineFs,
                         x, textY, "center", Theme::grey);
        textY += lineGap;
    }


    int btnTextSize = std::max(10, static_cast<int>(uiSize * 0.7f));
    int mx, my;
    SDL_GetMouseState(&mx, &my);

    for (auto& btn : buttons) {
        float bx = x + btn.xOffset * uiSize;
        float by = py + btn.yOffset * uiSize;
        float bw = btn.halfWidth * uiSize * 2;
        float bh = uiSize * 1.75f;

        float btnLeft = bx - bw * 0.5f;
        float btnTop = by - bh * 0.5f;

        bool hovered = (mx >= btnLeft && mx <= btnLeft + bw &&
                        my >= btnTop && my <= btnTop + bh);
        bool btnPressed = hovered && (SDL_GetMouseState(nullptr, nullptr) & SDL_BUTTON_LMASK);
        UIPrim::drawMenuButton(r, btnLeft, btnTop, bw, bh, btn.label, mx, my, btnPressed, "", btnTextSize);
    }

    UIAnim::popMenuTransition(r);

    if (animT < 0.999f) {
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(r, 0, 0, 0, static_cast<uint8_t>((1.0f - animT) * 150.0f));
        SDL_Rect fadeRect = {(int)(px - 4), (int)(py - 4), (int)(pw + 8), (int)(ph + 8)};
        SDL_RenderFillRect(r, &fadeRect);
    }
}


void Popup::rebuildImage(float uiSize) {

}


TextBox::TextBox(const std::string& title, const std::vector<PopupButton>& buttons,
                 float xSize, float ySize, float x, float y)
    : Popup(title, {}, buttons, xSize, ySize, x, y) {
    type = "textbox";
}

void TextBox::update(const InputState& input, float uiSize,
                     std::vector<std::unique_ptr<Popup>>& popupList) {

    if (input.hasTextInput) {
        inputText += input.textInput;
        if (static_cast<int>(inputText.size()) > maxChars) {
            inputText.resize(maxChars);
        }
    }

    if (input.isKeyDown(SDL_SCANCODE_BACKSPACE) && !inputText.empty()) {
        inputText.pop_back();
    }


    if (input.isKeyDown(SDL_SCANCODE_RETURN) && !buttons.empty()) {
        if (buttons[0].callback) {
            buttons[0].callback();
        }
        for (auto it = popupList.begin(); it != popupList.end(); ++it) {
            if (it->get() == this) {
                popupList.erase(it);
                return;
            }
        }
    }


    Popup::update(input, uiSize, popupList);
}
