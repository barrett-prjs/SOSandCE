#include "screens/country_select.h"
#include "core/app.h"
#include "core/engine.h"
#include "core/input.h"
#include "ui/primitives.h"
#include "ui/theme.h"
#include "ui/ui_assets.h"
#include "data/data_loader.h"
#include "game/game_state.h"

namespace {
struct CountrySelectLayout {
    float winW, winH, winX, winY;
    float pad, headerH, footerH;
    float searchX, searchY, searchW, searchH;
    float listX, listY, listW, listTop, listBottom;
    float rowH, rowGap;
    float btnY, btnW, btnH, btnGap;

    void compute(int W, int H) {
        float u = H / 100.0f * Engine::instance().uiScaleFactor();
        winW = std::round(W * 0.44f);
        winW = std::clamp(winW, 560.0f, 1120.0f);
        winH = std::round(H * 0.74f);
        winH = std::clamp(winH, 620.0f, 980.0f);
        winX = std::round((W - winW) * 0.5f);
        winY = std::round((H - winH) * 0.5f);

        pad = std::round(u * 1.8f);
        headerH = std::round(u * 5.0f);
        footerH = std::round(u * 6.6f);

        searchX = winX + pad;
        searchY = winY + headerH + pad;
        searchW = winW - pad * 2.0f;
        searchH = std::round(u * 3.2f);

        listX = searchX;
        listY = searchY + searchH + pad * 0.8f;
        listW = searchW;
        listTop = listY;
        listBottom = winY + winH - footerH - pad;

        rowH = std::round(u * 3.0f);
        rowGap = std::round(u * 0.45f);

        btnGap = std::round(u * 0.8f);
        btnY = winY + winH - footerH + std::round(u * 0.9f);
        btnH = std::round(u * 4.6f);
        btnW = (winW - pad * 2.0f - btnGap * 2.0f) / 3.0f;
    }
};
}


void CountrySelectScreen::enter(App& app) {
    scrollOffset_ = 0;
    hoveredIndex_ = -1;
    searchFilter_.clear();
    selectedCountry.clear();


    countries_.clear();
    try {
        json records = DataLoader::getCountryRecords();
        if (records.is_array()) {
            for (auto& rec : records) {
                if (rec.contains("name") && rec["name"].is_string()) {
                    countries_.push_back(rec["name"].get<std::string>());
                }
            }
        } else if (records.is_object()) {
            for (auto& [key, val] : records.items()) {
                countries_.push_back(key);
            }
        }
    } catch (...) {

    }
    std::sort(countries_.begin(), countries_.end());
}


void CountrySelectScreen::handleInput(App& app, const InputState& input) {
    auto& eng = Engine::instance();
    int W = eng.WIDTH;
    int H = eng.HEIGHT;
    int mx = input.mouseX;
    int my = input.mouseY;
    CountrySelectLayout L;
    L.compute(W, H);


    if (input.scrollY != 0) {
        scrollOffset_ -= input.scrollY * static_cast<int>(L.rowH + L.rowGap);
        if (scrollOffset_ < 0) scrollOffset_ = 0;
    }


    if (input.hasTextInput) {
        searchFilter_ += input.textInput;
        scrollOffset_ = 0;
    }
    if (input.isKeyDown(SDL_SCANCODE_BACKSPACE) && !searchFilter_.empty()) {
        searchFilter_.pop_back();
        scrollOffset_ = 0;
    }


    std::vector<std::string> filtered;
    for (auto& c : countries_) {
        if (searchFilter_.empty()) {
            filtered.push_back(c);
        } else {

            std::string lowerName = c;
            std::string lowerFilter = searchFilter_;
            std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
            std::transform(lowerFilter.begin(), lowerFilter.end(), lowerFilter.begin(), ::tolower);
            if (lowerName.find(lowerFilter) != std::string::npos) {
                filtered.push_back(c);
            }
        }
    }


    if (input.mouseLeftDown) {
        float startY = L.listY - scrollOffset_;


        for (int i = 0; i < static_cast<int>(filtered.size()); i++) {
            float by = startY + i * (L.rowH + L.rowGap);
            if (by + L.rowH < L.listTop || by > L.listBottom) continue;

            SDL_Rect btnRect = {
                static_cast<int>(L.listX), static_cast<int>(by),
                static_cast<int>(L.listW), static_cast<int>(L.rowH)
            };
            SDL_Point pt = {mx, my};
            if (SDL_PointInRect(&pt, &btnRect)) {
                selectedCountry = filtered[i];
            }
        }


        float backX = L.winX + L.pad;
        SDL_Rect backRect = {
            static_cast<int>(backX), static_cast<int>(L.btnY),
            static_cast<int>(L.btnW), static_cast<int>(L.btnH)
        };
        SDL_Point pt = {mx, my};
        if (SDL_PointInRect(&pt, &backRect)) {
            nextScreen = ScreenType::MAP_SELECT;
        }


        float startX = backX + L.btnW + L.btnGap;
        SDL_Rect startRect = {
            static_cast<int>(startX), static_cast<int>(L.btnY),
            static_cast<int>(L.btnW), static_cast<int>(L.btnH)
        };
        if (SDL_PointInRect(&pt, &startRect) && !selectedCountry.empty()) {

            app.gameState().controlledCountry = selectedCountry;
            app.gameState().mapViewOnly = false;
            app.gameState().inGame = false;
            nextScreen = ScreenType::GAME;
        }

        float mapViewX = startX + L.btnW + L.btnGap;
        SDL_Rect mapViewRect = {
            static_cast<int>(mapViewX), static_cast<int>(L.btnY),
            static_cast<int>(L.btnW), static_cast<int>(L.btnH)
        };
        if (SDL_PointInRect(&pt, &mapViewRect)) {
            app.gameState().controlledCountry.clear();
            app.gameState().mapViewOnly = true;
            app.gameState().speed = 0;
            app.gameState().inGame = false;
            nextScreen = ScreenType::GAME;
        }
    }


    if (input.isKeyDown(SDL_SCANCODE_ESCAPE)) {
        nextScreen = ScreenType::MAP_SELECT;
    }
}


void CountrySelectScreen::update(App& app, float dt) {

}


void CountrySelectScreen::render(App& app) {
    auto& eng = Engine::instance();
    int W = eng.WIDTH;
    int H = eng.HEIGHT;
    auto* r = eng.renderer;
    int mx, my;
    SDL_GetMouseState(&mx, &my);

    eng.clear(Theme::bg);
    UIPrim::drawVignette(r, W, H, 100);
    CountrySelectLayout L;
    L.compute(W, H);

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

    SDL_Texture* headerTex2 = assets.panelHeader();
    if (headerTex2) UIAssets::draw9Slice(r, headerTex2, L.winX + 3, L.winY + 3, L.winW - 6, L.headerH, 14);
    int titleFs = std::max(16, (int)(L.headerH * 0.42f));
    UIPrim::drawText(r, "SELECT COUNTRY", titleFs, L.winX + L.winW / 2, L.winY + L.headerH * 0.5f, "center", Theme::gold_bright, true);
    UIPrim::drawHLine(r, Theme::gold, L.winX + 8, L.winX + L.winW - 8, L.winY + L.headerH + 2, 2);
    UIPrim::drawOrnamentalFrame(r, L.winX, L.winY, L.winW, L.winH, 2);


    UIPrim::drawRoundedRect(r, {10, 12, 16}, L.searchX, L.searchY, L.searchW, L.searchH, 4, Theme::border);

    std::string searchText = searchFilter_.empty() ? "Search..." : searchFilter_ + "_";
    Color searchColor = searchFilter_.empty() ? Theme::dark_grey : Theme::cream;
    int searchFs = std::max(13, (int)(H / 75.0f));
    UIPrim::drawText(r, searchText, searchFs,
                     L.searchX + std::round(L.pad * 0.35f), L.searchY + L.searchH * 0.5f,
                     "midleft", searchColor);


    std::vector<std::string> filtered;
    for (auto& c : countries_) {
        if (searchFilter_.empty()) {
            filtered.push_back(c);
        } else {
            std::string lowerName = c;
            std::string lowerFilter = searchFilter_;
            std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
            std::transform(lowerFilter.begin(), lowerFilter.end(), lowerFilter.begin(), ::tolower);
            if (lowerName.find(lowerFilter) != std::string::npos) {
                filtered.push_back(c);
            }
        }
    }


    float startY = L.listY - scrollOffset_;


    float clipTop = L.listTop;
    float clipBottom = L.listBottom;

    for (int i = 0; i < static_cast<int>(filtered.size()); i++) {
        float by = startY + i * (L.rowH + L.rowGap);
        if (by + L.rowH < clipTop || by > clipBottom) continue;

        bool isSelected = (filtered[i] == selectedCountry);
        std::string label = filtered[i];
        if (isSelected) label = "> " + label;

        UIPrim::drawMenuButton(r, L.listX, by, L.listW, L.rowH,
                                label, mx, my, false);

        if (isSelected) {
            UIPrim::drawRect(r, Theme::gold, L.listX, by, L.listW, L.rowH,
                             Theme::gold, 0, 1);
        }
    }


    int maxScroll = std::max(0, static_cast<int>(filtered.size() * (L.rowH + L.rowGap) - (clipBottom - clipTop)));
    if (scrollOffset_ > maxScroll) scrollOffset_ = maxScroll;

    float backX = L.winX + L.pad;
    float startX = backX + L.btnW + L.btnGap;
    float mapViewX = startX + L.btnW + L.btnGap;
    UIPrim::drawMenuButton(r, backX, L.btnY, L.btnW, L.btnH, "Back", mx, my, false);

    bool canStart = !selectedCountry.empty();
    float startBtnX = startX;
    float startBtnW = L.btnW;
    float startBtnH = L.btnH;
    bool startHov = canStart && mx >= startBtnX && mx <= startBtnX + startBtnW &&
                    my >= L.btnY && my <= L.btnY + startBtnH;

    SDL_Texture* panelTex = UIAssets::instance().panelBodyHeadless();
    if (panelTex) {
        uint8_t mod = !canStart ? 148 : (startHov ? 255 : 238);
        SDL_SetTextureColorMod(panelTex, mod, mod, mod);
        SDL_SetTextureAlphaMod(panelTex, canStart ? 255 : 190);
        UIAssets::draw9Slice(r, panelTex, startBtnX, L.btnY, startBtnW, startBtnH, 18);
        SDL_SetTextureColorMod(panelTex, 255, 255, 255);
        SDL_SetTextureAlphaMod(panelTex, 255);
    } else if (canStart) {
        UIPrim::drawRoundedRect(r, startHov ? Color{55, 120, 70} : Theme::btn_confirm, startBtnX, L.btnY, startBtnW, startBtnH, 5, Theme::green);
    } else {
        UIPrim::drawRoundedRect(r, Theme::btn_disabled, startBtnX, L.btnY, startBtnW, startBtnH, 5, Theme::border);
    }
    UIPrim::drawHLine(r, canStart && startHov ? Theme::gold_dim : Color{Theme::border.r, Theme::border.g, Theme::border.b, 90},
                      startBtnX + 6, startBtnX + startBtnW - 6, L.btnY + startBtnH - 2, 1);
    Color startTc = canStart ? (startHov ? Theme::gold_bright : Theme::cream) : Theme::dark_grey;
    int btnFs = std::max(13, (int)(H / 78.0f));
    UIPrim::drawText(r, "Start", btnFs,
                     startBtnX + startBtnW * 0.5f, L.btnY + startBtnH * 0.5f,
                     "center", startTc, canStart);

    bool mapViewHov = mx >= mapViewX && mx <= mapViewX + L.btnW &&
                      my >= L.btnY && my <= L.btnY + L.btnH;
    UIPrim::drawMenuButton(r, mapViewX, L.btnY, L.btnW, L.btnH, "Map View", mx, my, false);
    UIPrim::drawText(r, "Open the scenario map without starting a campaign", std::max(10, btnFs - 2),
                     L.winX + L.winW * 0.5f, L.btnY - std::round(L.pad * 0.45f), "center",
                     mapViewHov ? Theme::cream : Theme::grey);
}
