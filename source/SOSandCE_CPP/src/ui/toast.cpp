#include "ui/toast.h"
#include "ui/primitives.h"
#include "ui/theme.h"
#include "ui/ui_assets.h"

static ToastSystem s_toasts;

ToastSystem& toasts() { return s_toasts; }

void ToastSystem::show(const std::string& text, int durationMs) {
    uint32_t expire = SDL_GetTicks() + durationMs;
    toasts_.push_back({text, expire});
}

void ToastSystem::render(SDL_Renderer* r, int screenW, int screenH, float uiSize) {
    uint32_t now = SDL_GetTicks();
    float yPos = screenH / 3.0f;
    auto& assets = UIAssets::instance();

    int toastFs = std::max(14, static_cast<int>(screenH / 55.0f));
    float toastPad = screenH * 0.018f;

    for (int i = static_cast<int>(toasts_.size()) - 1; i >= 0; i--) {
        if (now >= toasts_[i].expireTime) {
            toasts_.erase(toasts_.begin() + i);
            continue;
        }

        float remaining = static_cast<float>(toasts_[i].expireTime - now);
        float alpha = std::min(1.0f, remaining / 400.0f);

        int tw = UIPrim::textWidth(toasts_[i].text, toastFs);
        float px = (screenW - tw) / 2.0f - toastPad;
        float pw = tw + toastPad * 2;
        float ph = toastFs * 2.4f;


        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(r, 0, 0, 0, static_cast<uint8_t>(40 * alpha));
        SDL_Rect sh = {(int)px + 2, (int)yPos + 3, (int)pw, (int)ph};
        SDL_RenderFillRect(r, &sh);


        SDL_Texture* toastBg = assets.panelBodyHeadless();
        if (toastBg) {
            SDL_SetTextureAlphaMod(toastBg, static_cast<uint8_t>(230 * alpha));
            UIAssets::draw9Slice(r, toastBg, px, yPos, pw, ph, 14);
            SDL_SetTextureAlphaMod(toastBg, 255);
        } else {
            SDL_SetRenderDrawColor(r, Theme::panel.r, Theme::panel.g, Theme::panel.b,
                                  static_cast<uint8_t>(230 * alpha));
            SDL_Rect bg = {(int)px, (int)yPos, (int)pw, (int)ph};
            SDL_RenderFillRect(r, &bg);
        }


        SDL_SetRenderDrawColor(r, Theme::gold_dim.r, Theme::gold_dim.g, Theme::gold_dim.b,
                              static_cast<uint8_t>(200 * alpha));
        SDL_RenderDrawLine(r, (int)(px + 6), (int)yPos + 2, (int)(px + pw - 6), (int)yPos + 2);


        SDL_Texture* infoIcon = assets.icon("Info");
        if (infoIcon) {
            float iSz = ph * 0.45f;
            SDL_SetTextureAlphaMod(infoIcon, static_cast<uint8_t>(200 * alpha));
            SDL_SetTextureColorMod(infoIcon, Theme::gold.r, Theme::gold.g, Theme::gold.b);
            SDL_Rect iDst = {(int)(px + 8), (int)(yPos + (ph - iSz) / 2), (int)iSz, (int)iSz};
            SDL_RenderCopy(r, infoIcon, nullptr, &iDst);
            SDL_SetTextureColorMod(infoIcon, 255, 255, 255);
            SDL_SetTextureAlphaMod(infoIcon, 255);
        }


        UIPrim::drawText(r, toasts_[i].text, toastFs,
                         screenW / 2.0f, yPos + ph / 2, "center", Theme::cream);

        yPos += ph + 6;
    }
}

void ToastSystem::clear() {
    toasts_.clear();
}
