#include "ui/popup.h"
#include "core/engine.h"
#include "core/audio.h"
#include "core/input.h"
#include "ui/primitives.h"
#include "ui/theme.h"
#include "ui/ui_assets.h"


Popup::Popup(const std::string& title, const std::vector<std::string>& text,
             const std::vector<PopupButton>& buttons, float xSize, float ySize,
             float x, float y, const std::string& flag1, const std::string& flag2,
             bool dismissOnClick)
    : title(title), text(text), buttons(buttons),
      xSize(xSize), ySize(ySize),
      flag1(flag1), flag2(flag2), dismissOnClick(dismissOnClick) {

    auto& eng = Engine::instance();
    float uiSize = eng.uiScale;


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
                printf("[Popup] Clicked '%s' on popup '%s'\n", btn.label.c_str(), title.c_str());
                fflush(stdout);
                Audio::instance().playSound("clickedSound");
                if (btn.callback) {
                    btn.callback();
                }

                for (auto it = popupList.begin(); it != popupList.end(); ++it) {
                    if (it->get() == this) {
                        popupList.erase(it);
                        return;
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
    float halfW = xSize * uiSize * 0.5f;
    float halfH = ySize * uiSize * 0.5f;
    float px = x - halfW;
    float py = y - halfH;
    float pw = xSize * uiSize;
    float ph = ySize * uiSize;


    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, 0, 0, 0, 100);
    auto& eng = Engine::instance();
    SDL_Rect fullScreen = {0, 0, eng.WIDTH, eng.HEIGHT};
    SDL_RenderFillRect(r, &fullScreen);


    auto& assets = UIAssets::instance();
    SDL_Texture* panelTex = assets.panelBodyHeaded();
    if (panelTex) {
        UIAssets::draw9Slice(r, panelTex, px, py, pw, ph, 24);
    } else {
        UIPrim::drawRectFilled(r, Theme::bg_dark, px + 4, py + 4, pw - 8, ph - 8);
    }


    float headerH = uiSize * 1.5f;
    SDL_Texture* headerTex = assets.panelHeader();
    if (headerTex) {
        UIAssets::draw9Slice(r, headerTex, px + 4, py + 4, pw - 8, headerH, 16);
    } else {
        UIPrim::drawRectFilled(r, Theme::header, px + 4, py + 4, pw - 8, headerH);
    }


    UIPrim::drawHLine(r, Theme::gold, px + 4, px + pw - 4, py + 4 + headerH, 2);


    UIPrim::drawOrnamentalFrame(r, px, py, pw, ph, 2);


    int titleSize = std::max(12, static_cast<int>(uiSize * 0.9f));
    UIPrim::drawText(r, title, titleSize,
                     x, py + 4 + headerH * 0.5f,
                     "center", Theme::gold_bright, true);


    if (!flag1.empty()) {
        SDL_Texture* flagTex = eng.loadTexture(eng.assetsPath + "flags/" + flag1 + ".png");
        if (flagTex) {
            float flagSize = uiSize * 1.8f;
            SDL_Rect flagRect = {
                static_cast<int>(px + uiSize * 0.5f),
                static_cast<int>(py + headerH + uiSize * 0.8f),
                static_cast<int>(flagSize * 1.5f),
                static_cast<int>(flagSize)
            };
            SDL_RenderCopy(r, flagTex, nullptr, &flagRect);
        }
    }

    if (!flag2.empty()) {
        SDL_Texture* flagTex = eng.loadTexture(eng.assetsPath + "flags/" + flag2 + ".png");
        if (flagTex) {
            float flagSize = uiSize * 1.8f;
            SDL_Rect flagRect = {
                static_cast<int>(px + pw - uiSize * 0.5f - flagSize * 1.5f),
                static_cast<int>(py + headerH + uiSize * 0.8f),
                static_cast<int>(flagSize * 1.5f),
                static_cast<int>(flagSize)
            };
            SDL_RenderCopy(r, flagTex, nullptr, &flagRect);
        }
    }


    int textSize = std::max(10, static_cast<int>(uiSize * 0.65f));
    float textY = py + headerH + uiSize * 1.5f;
    float lineGap = uiSize * 0.85f;

    for (auto& line : text) {
        UIPrim::drawText(r, line, textSize,
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
