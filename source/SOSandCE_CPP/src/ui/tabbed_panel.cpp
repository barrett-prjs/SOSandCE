#include "ui/tabbed_panel.h"
#include "core/input.h"
#include "ui/primitives.h"
#include "ui/theme.h"
#include "ui/ui_assets.h"


TabbedPanel::TabbedPanel(float x, float y, float w, float h,
                         const std::string& title,
                         const std::vector<std::string>& tabs)
    : Panel(x, y, w, h, title), tabs(tabs) {
    if (!tabs.empty()) activeTab = 0;
    tabHeight = Theme::s(2.0f);
}


void TabbedPanel::handleInput(const InputState& input) {
    if (!visible) return;


    Panel::handleInput(input);

    float mx = static_cast<float>(input.mouseX);
    float my = static_cast<float>(input.mouseY);


    if (input.mouseLeftDown && !tabs.empty()) {
        float hh = headerHeight();
        float tabBarY = this->y + hh;

        if (my >= tabBarY && my <= tabBarY + tabHeight && mx >= this->x && mx <= this->x + w) {
            float tabW = w / static_cast<float>(tabs.size());
            int clickedTab = static_cast<int>((mx - this->x) / tabW);
            if (clickedTab >= 0 && clickedTab < static_cast<int>(tabs.size())) {
                activeTab = clickedTab;
                scrollY = 0;
            }
        }
    }
}


void TabbedPanel::render(SDL_Renderer* r) {
    if (!visible) return;

    float hh = headerHeight();
    auto& assets = UIAssets::instance();


    UIPrim::drawShadow(r, x, y, w, h, 50, 12);
    UIPrim::drawShadow(r, x, y, w, h, 80, 4);


    SDL_Texture* bodyTex = title.empty() ? assets.panelBodyHeadless() : assets.panelBodyHeaded();
    if (bodyTex) {
        SDL_SetTextureAlphaMod(bodyTex, static_cast<uint8_t>(alpha));
        UIAssets::draw9Slice(r, bodyTex, x, y, w, h, 18);
        SDL_SetTextureAlphaMod(bodyTex, 255);
    } else {
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(r, Theme::panel.r, Theme::panel.g, Theme::panel.b,
                               static_cast<uint8_t>(alpha));
        SDL_Rect bodyRect = {(int)x, (int)y, (int)w, (int)h};
        SDL_RenderFillRect(r, &bodyRect);
        UIPrim::drawRect(r, Theme::border, x, y, w, h, Theme::border, 0, 1);
    }


    renderHeader(r);


    float tabBarY = y + hh;
    UIPrim::drawRectFilled(r, {18, 20, 26}, x, tabBarY, w, tabHeight);

    if (!tabs.empty()) {
        float tabW = w / static_cast<float>(tabs.size());
        int mx2, my2; SDL_GetMouseState(&mx2, &my2);

        for (int i = 0; i < static_cast<int>(tabs.size()); i++) {
            float tx = x + i * tabW;
            bool isActive = (i == activeTab);
            bool isHov = mx2 >= tx && mx2 <= tx + tabW && my2 >= tabBarY && my2 <= tabBarY + tabHeight;


            if (isActive) {
                SDL_Texture* hdrTex = assets.panelHeader();
                if (hdrTex) UIAssets::draw9Slice(r, hdrTex, tx, tabBarY, tabW, tabHeight, 8);
                else UIPrim::drawRectFilled(r, Theme::slot_active, tx, tabBarY, tabW, tabHeight);
            } else if (isHov) {
                UIPrim::drawRectFilled(r, Theme::btn_hover, tx, tabBarY, tabW, tabHeight);
            }


            Color textColor = isActive ? Theme::gold_bright : (isHov ? Theme::cream : Theme::grey);
            UIPrim::drawText(r, tabs[i], Theme::si(0.85f),
                             tx + tabW * 0.5f, tabBarY + tabHeight * 0.5f,
                             "center", textColor, isActive);


            if (isActive) {
                UIPrim::drawHLine(r, Theme::gold, tx + 4, tx + tabW - 4,
                                  tabBarY + tabHeight - 3, 3);
            }


            if (i < static_cast<int>(tabs.size()) - 1) {
                UIPrim::drawVLine(r, Theme::border, tx + tabW,
                                  tabBarY + 6, tabBarY + tabHeight - 6, 1);
            }
        }
    }


    UIPrim::drawHLine(r, Theme::gold_dim, x, x + w, tabBarY + tabHeight, 1);


    float contentTop = tabBarY + tabHeight;
    float contentH = h - hh - tabHeight;

    SDL_Rect clipRect = {
        static_cast<int>(x + 1),
        static_cast<int>(contentTop),
        static_cast<int>(w - 2),
        static_cast<int>(contentH)
    };
    SDL_RenderSetClipRect(r, &clipRect);


    if (drawTab && activeTab >= 0 && activeTab < static_cast<int>(tabs.size())) {
        float pad = Theme::s(0.5f);
        contentHeight = drawTab(r, activeTab,
                                x + pad, contentTop + pad - scrollY,
                                w - pad * 2);
        maxScroll = std::max(0.0f, contentHeight - contentH);
    }

    SDL_RenderSetClipRect(r, nullptr);


    if (maxScroll > 0) {
        renderScrollbar(r);
    }
}
