#include "screens/map_select.h"
#include "core/app.h"
#include "core/engine.h"
#include "core/input.h"
#include "ui/primitives.h"
#include "ui/theme.h"
#include "ui/ui_assets.h"
#include "game/game_state.h"

namespace {
struct MapSelectLayout {
    float winW, winH, winX, winY;
    float pad, headerH, footerH;
    float contentX, contentY, contentW, contentH;
    float cardX, cardY, cardW, cardH, cardGap;
    float backX, backY, backW, backH;

    void compute(int W, int H) {
        float u = H / 100.0f * Engine::instance().uiScaleFactor();
        winW = std::round(W * 0.48f);
        winW = std::clamp(winW, 550.0f, 1250.0f);
        winH = std::round(H * 0.70f);
        winX = std::round((W - winW) / 2.0f);
        winY = std::round((H - winH) / 2.0f);
        pad = std::round(u * 1.8f);
        headerH = std::round(u * 5.0f);
        footerH = std::round(u * 6.5f);
        contentX = winX + pad;
        contentY = winY + headerH + pad;
        contentW = winW - pad * 2.0f;
        contentH = winH - headerH - pad - footerH;

        cardH = std::round(u * 5.5f);
        cardGap = std::round(u * 0.8f);
        cardX = contentX + pad * 0.5f;
        cardY = contentY + pad * 0.5f;
        cardW = contentW - pad;

        backW = std::round(winW * 0.30f);
        backH = std::round(u * 4.8f);
        backX = winX + (winW - backW) / 2.0f;
        backY = winY + winH - footerH + std::round(u * 0.8f);
    }
};
}


void MapSelectScreen::enter(App& app) {
    hoveredIndex_ = -1;
    selectedMap.clear();
    scenarios_.clear();


    auto& eng = Engine::instance();
    std::string startsDir = eng.assetsPath + "starts/";

    if (fs::exists(startsDir)) {
        for (auto& entry : fs::directory_iterator(startsDir)) {
            if (!entry.is_directory()) continue;

            ScenarioInfo info;
            info.name = entry.path().filename().string();


            std::string creatorPath = entry.path().string() + "/creator.txt";
            if (fs::exists(creatorPath)) {
                std::ifstream file(creatorPath);
                if (file.is_open()) {
                    std::getline(file, info.creator);
                }
            }
            std::string datePath = entry.path().string() + "/startDate.txt";
            if (fs::exists(datePath)) {
                std::ifstream file(datePath);
                if (file.is_open()) {
                    std::getline(file, info.startDate);
                }
            }

            if (info.creator.empty()) {
                std::string infoPath = entry.path().string() + "/info.txt";
                if (fs::exists(infoPath)) {
                    std::ifstream file(infoPath);
                    if (file.is_open()) {
                        std::getline(file, info.creator);
                        std::getline(file, info.startDate);
                    }
                }
            }

            if (info.creator.empty()) info.creator = "Unknown";
            if (info.startDate.empty()) info.startDate = "";

            scenarios_.push_back(info);
        }
    }

    std::sort(scenarios_.begin(), scenarios_.end(),
              [](const ScenarioInfo& a, const ScenarioInfo& b) {
                  return a.name < b.name;
              });
}


void MapSelectScreen::handleInput(App& app, const InputState& input) {
    auto& eng = Engine::instance();
    int W = eng.WIDTH;
    int H = eng.HEIGHT;
    int mx = input.mouseX;
    int my = input.mouseY;
    MapSelectLayout L;
    L.compute(W, H);

    if (input.mouseLeftDown) {
        for (int i = 0; i < static_cast<int>(scenarios_.size()); i++) {
            float by = L.cardY + i * (L.cardH + L.cardGap);
            if (by + L.cardH < L.contentY || by > L.contentY + L.contentH) continue;

            SDL_Rect btnRect = {
                static_cast<int>(L.cardX), static_cast<int>(by),
                static_cast<int>(L.cardW), static_cast<int>(L.cardH)
            };
            SDL_Point pt = {mx, my};
            if (SDL_PointInRect(&pt, &btnRect)) {
                selectedMap = scenarios_[i].name;

                app.gameState().mapName = selectedMap;
                nextScreen = ScreenType::COUNTRY_SELECT;
            }
        }


        SDL_Rect backRect = {
            static_cast<int>(L.backX), static_cast<int>(L.backY),
            static_cast<int>(L.backW), static_cast<int>(L.backH)
        };
        SDL_Point pt = {mx, my};
        if (SDL_PointInRect(&pt, &backRect)) {
            nextScreen = ScreenType::MAIN_MENU;
        }
    }


    if (input.isKeyDown(SDL_SCANCODE_ESCAPE)) {
        nextScreen = ScreenType::MAIN_MENU;
    }
}


void MapSelectScreen::update(App& app, float dt) {

}


void MapSelectScreen::render(App& app) {
    auto& eng = Engine::instance();
    int W = eng.WIDTH, H = eng.HEIGHT;
    float u = H / 100.0f * eng.uiScaleFactor();
    auto* r = eng.renderer;
    int mx, my; SDL_GetMouseState(&mx, &my);
    MapSelectLayout L;
    L.compute(W, H);

    eng.clear(Theme::bg);
    UIPrim::drawVignette(r, W, H, 80);


    auto& assets = UIAssets::instance();
    for (int s = 20; s >= 1; s--) {
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(r, 0, 0, 0, (uint8_t)std::min(255, (int)(80.0f * s / 20.0f * 0.4f)));
        SDL_Rect sh = {(int)L.winX - s/2, (int)L.winY + s, (int)L.winW + s, (int)L.winH};
        SDL_RenderFillRect(r, &sh);
    }
    UIPrim::drawRoundedRect(r, {18, 20, 26}, L.winX, L.winY, L.winW, L.winH, 8);
    SDL_Texture* windowTex = assets.panelBodyHeaded();
    if (!windowTex) windowTex = assets.panelBodyHeadless();
    if (windowTex) UIAssets::draw9Slice(r, windowTex, L.winX, L.winY, L.winW, L.winH, 28);

    SDL_Texture* headerTex = assets.panelHeader();
    if (headerTex) UIAssets::draw9Slice(r, headerTex, L.winX + 3, L.winY + 3, L.winW - 6, L.headerH, 14);
    int titleFs = std::max(16, (int)(L.headerH * 0.42f));
    UIPrim::drawText(r, "SELECT SCENARIO", titleFs, L.winX + L.winW / 2, L.winY + L.headerH * 0.5f, "center", Theme::gold_bright, true);
    UIPrim::drawHLine(r, Theme::gold, L.winX + 8, L.winX + L.winW - 8, L.winY + L.headerH + 2, 2);


    UIPrim::drawRoundedRect(r, {10, 12, 16}, L.contentX, L.contentY, L.contentW, L.contentH, 6);
    SDL_SetRenderDrawColor(r, Theme::border.r, Theme::border.g, Theme::border.b, 40);
    SDL_Rect wb = {(int)L.contentX, (int)L.contentY, (int)L.contentW, (int)L.contentH};
    SDL_RenderDrawRect(r, &wb);


    if (scenarios_.empty()) {
        int emFs = std::max(14, (int)(u * 1.3f));
        UIPrim::drawText(r, "No scenarios found", emFs, L.contentX + L.contentW / 2, L.contentY + L.contentH * 0.4f, "center", Theme::grey);
        int subFs = std::max(12, (int)(u * 1.0f));
        UIPrim::drawText(r, "Place scenario folders in starts/", subFs, L.contentX + L.contentW / 2, L.contentY + L.contentH * 0.52f, "center", Theme::dark_grey);
    }

    SDL_Rect clipScen = {(int)L.contentX, (int)L.contentY, (int)L.contentW, (int)L.contentH};
    SDL_RenderSetClipRect(r, &clipScen);

    int nameFs = std::max(14, (int)(L.cardH * 0.30f));
    int subFs = std::max(12, (int)(L.cardH * 0.22f));

    for (int i = 0; i < (int)scenarios_.size(); i++) {
        float by = L.cardY + i * (L.cardH + L.cardGap);
        if (by + L.cardH < L.contentY || by > L.contentY + L.contentH) continue;

        bool hovered = (mx >= L.cardX && mx <= L.cardX + L.cardW && my >= by && my <= by + L.cardH);


        SDL_Texture* btnTex = hovered ? assets.btnRectHover() : assets.btnRectDefault();
        if (btnTex) {
            SDL_SetTextureColorMod(btnTex, hovered ? 200 : 160, hovered ? 198 : 158, hovered ? 195 : 155);
            UIAssets::draw9Slice(r, btnTex, L.cardX, by, L.cardW, L.cardH, 14);
            SDL_SetTextureColorMod(btnTex, 255, 255, 255);
        }


        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(r, Theme::gold.r, Theme::gold.g, Theme::gold.b, hovered ? 220 : 80);
        SDL_Rect lbar = {(int)L.cardX + 4, (int)by + 6, 4, (int)L.cardH - 12};
        SDL_RenderFillRect(r, &lbar);


        Color nameC = hovered ? Theme::gold_bright : Theme::cream;
        UIPrim::drawText(r, scenarios_[i].name, nameFs, L.cardX + std::round(u * 1.5f), by + L.cardH * 0.32f, "midleft", nameC, true);


        if (!scenarios_[i].creator.empty()) {
            UIPrim::drawText(r, "by " + scenarios_[i].creator, subFs, L.cardX + std::round(u * 1.5f), by + L.cardH * 0.68f, "midleft", Theme::grey);
        }
        if (!scenarios_[i].startDate.empty()) {
            UIPrim::drawText(r, scenarios_[i].startDate, subFs, L.cardX + L.cardW - std::round(u * 1.5f), by + L.cardH * 0.5f, "midright", Theme::gold_dim);
        }
    }
    SDL_RenderSetClipRect(r, nullptr);


    float footerY = L.winY + L.winH - L.footerH;
    UIPrim::drawHLine(r, {Theme::gold_dim.r, Theme::gold_dim.g, Theme::gold_dim.b, 40}, L.winX + L.pad, L.winX + L.winW - L.pad, footerY, 1);
    UIPrim::drawMenuButton(r, L.backX, L.backY, L.backW, L.backH, "Back", mx, my, false);

    UIPrim::drawOrnamentalFrame(r, L.winX, L.winY, L.winW, L.winH, 2);
}
