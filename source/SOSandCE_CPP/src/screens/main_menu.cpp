#include "screens/main_menu.h"
#include "screens/game_screen.h"
#include "screens/save_load.h"
#include "screens/settings_screen.h"
#include "core/app.h"
#include "core/engine.h"
#include "core/audio.h"
#include "core/input.h"
#include "ui/primitives.h"
#include "ui/theme.h"
#include "ui/ui_assets.h"
#include "data/data_loader.h"
#include "data/country_data.h"
#include "data/region_data.h"
#include "game/game_state.h"
#include "game/country.h"
#include "game/helpers.h"
#include "imgui.h"
#include "ui/rmlui_backend.h"
#include <RmlUi/Core.h>
#include <set>


struct MenuBG {
    SDL_Surface* mapSurf = nullptr;
    SDL_Texture* cachedTex = nullptr;
    float camx = 0, camy = 0, zoom = 3.0f;
    float speed = 0;
    float angle = 0;
    float time = 0;
    float maxTime = 600;
    bool initted = false;

    void init(SDL_Surface* m) {
        mapSurf = m;
        if (!m) return;
        if (cachedTex) { SDL_DestroyTexture(cachedTex); cachedTex = nullptr; }
        camx = m->w * 0.3f;
        camy = m->h * 0.3f;
        zoom = 3.0f;
        speed = 0.3f;
        angle = randFloat(0, 6.28318f);
        time = 0;
        maxTime = 600;
        initted = true;
    }

    void update(SDL_Renderer* r, int W, int H, int fps) {
        if (!mapSurf || !initted) return;
        float dt = 1.0f / std::max(1, fps);
        camx += std::cos(angle) * speed * dt * 60.0f;
        camy += std::sin(angle) * speed * dt * 60.0f;
        time += dt * 60.0f;
        if (time > maxTime) {

            camx = randFloat(mapSurf->w * 0.1f, mapSurf->w * 0.7f);
            camy = randFloat(mapSurf->h * 0.15f, mapSurf->h * 0.65f);
            angle = randFloat(0, 6.28318f);
            time = 0;
        }


        if (!cachedTex) {
            SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
            cachedTex = SDL_CreateTextureFromSurface(r, mapSurf);
            SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
        }
        if (!cachedTex) return;


        float visW = W / zoom;
        float visH = H / zoom;
        float srcXf = camx - visW / 2.0f;
        float srcYf = camy - visH / 2.0f;


        srcXf = std::max(0.0f, std::min(srcXf, mapSurf->w - visW));
        srcYf = std::max(0.0f, std::min(srcYf, mapSurf->h - visH));


        SDL_Rect src = {(int)srcXf, (int)srcYf, (int)std::ceil(visW), (int)std::ceil(visH)};

        if (src.x + src.w > mapSurf->w) src.w = mapSurf->w - src.x;
        if (src.y + src.h > mapSurf->h) src.h = mapSurf->h - src.y;
        if (src.w <= 0 || src.h <= 0) return;

        SDL_Rect dst = {0, 0, W, H};
        SDL_RenderCopy(r, cachedTex, &src, &dst);


        float fadeLen = maxTime * 0.12f;
        if (time < fadeLen) {
            int alpha = static_cast<int>((1.0f - time / fadeLen) * 200);
            SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(r, 0, 0, 0, (uint8_t)std::min(255, alpha));
            SDL_RenderFillRect(r, &dst);
        } else if (time > maxTime - fadeLen) {
            int alpha = static_cast<int>(((time - (maxTime - fadeLen)) / fadeLen) * 200);
            SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(r, 0, 0, 0, (uint8_t)std::min(255, alpha));
            SDL_RenderFillRect(r, &dst);
        }
    }
};

static MenuBG s_menuBG;


static std::string getCountryType(const std::string& culture) {
    return CountryData::instance().getCountryType(culture);
}


static std::string findMajorOnMap(const std::string& culture, const std::vector<std::string>& mapCountries) {
    auto& cd = CountryData::instance();
    for (auto& cn : mapCountries) {
        auto* rec = cd.getRecord(cn);
        if (rec && rec->culture == culture) return cn;
    }
    return "";
}


static Rml::ElementDocument* s_menuDoc = nullptr;

void MainMenuScreen::enter(App& app) {
    menuState_ = MenuState::MAIN;
    hoveredButton_ = -1;
    animTimer_ = 0;
    scrollOffset_ = 0;
    selectedCountry_.clear();

    auto& eng = Engine::instance();


    if (s_menuDoc) {
        s_menuDoc->Hide();
    }


    mapList_.clear();
    std::string startsDir = eng.assetsPath + "starts/";
    if (fs::exists(startsDir)) {
        for (auto& entry : fs::directory_iterator(startsDir)) {
            if (entry.is_directory())
                mapList_.push_back(entry.path().filename().string());
        }
    }
    std::sort(mapList_.begin(), mapList_.end());
    if (selectedMap_.empty() && !mapList_.empty()) selectedMap_ = "Modern Day";


    savesList_.clear();
    std::string savesDir = eng.assetsPath + "saves/";
    if (fs::exists(savesDir)) {
        for (auto& entry : fs::directory_iterator(savesDir))
            if (entry.is_directory()) savesList_.push_back(entry.path().filename().string());
    }
    std::sort(savesList_.begin(), savesList_.end());


    if (!s_menuBG.initted) {

        std::string mapPath = eng.assetsPath + "starts/Modern Day/map.png";
        SDL_Surface* m = eng.loadSurface(mapPath);
        if (m) s_menuBG.init(m);
    }


    std::string musicPath = eng.assetsPath + "music/menuMusic.mp3";
    if (fs::exists(musicPath)) Audio::instance().playMusic(musicPath, true, 1000);
}

void MainMenuScreen::exit(App& app) {}


void MainMenuScreen::handleInput(App& app, const InputState& input) {
    auto& eng = Engine::instance();
    int W = eng.WIDTH, H = eng.HEIGHT;
    float ui = eng.uiScale;
    int mx = input.mouseX, my = input.mouseY;
    bool clicked = input.mouseLeftDown;

    auto hit = [&](float rx, float ry, float rw, float rh) {
        return clicked && mx >= rx && mx <= rx + rw && my >= ry && my <= ry + rh;
    };

    switch (menuState_) {
    case MenuState::MAIN: {
        if (clicked) {

            float u2 = H / 100.0f * Engine::instance().uiScaleFactor();
            float pW = std::round(W * 0.42f);
            pW = std::clamp(pW, 500.0f, 1100.0f);
            float pX = std::round((W - pW) / 2.0f);
            float logoH2 = H * 0.18f, logoY2 = u2 * 4;
            float outerPad2 = std::round(u2 * 2.0f);
            float headerH2 = std::round(u2 * 5.5f);
            float sectionGap2 = std::round(u2 * 1.5f);
            float bH = std::round(u2 * 6.5f);
            float bGap = std::round(u2 * 1.2f);
            float bRegPad = std::round(u2 * 2.2f);

            float pY = std::round(logoY2 + logoH2 + u2 * 3.5f);
            float goldLineY2 = pY + headerH2 + 2;
            float wellX2 = pX + outerPad2;
            float wellY2 = goldLineY2 + sectionGap2;
            float wellW2 = pW - outerPad2 * 2;
            float bW = std::round(wellW2 - outerPad2 * 2);
            float bX = std::round(wellX2 + outerPad2);
            float bStartY = std::round(wellY2 + bRegPad);

            for (int i = 0; i < 4; i++) {
                float by = std::round(bStartY + i * (bH + bGap));
                if (mx >= bX && mx <= bX + bW && my >= by && my <= by + bH) {
                    Audio::instance().playSound("clickedSound");
                    if (i == 0) { menuState_ = MenuState::MAPS; scrollOffset_ = 0; }
                    if (i == 1) {
                        auto* saveLoad = dynamic_cast<SaveLoadScreen*>(app.screen(ScreenType::SAVE_LOAD));
                        if (saveLoad) {
                            saveLoad->mode = SaveLoadScreen::Mode::LOAD;
                            saveLoad->returnScreen = ScreenType::MAIN_MENU;
                        }
                        nextScreen = ScreenType::SAVE_LOAD;
                    }
                    if (i == 2) {
                        if (auto* settings = dynamic_cast<SettingsScreen*>(app.screen(ScreenType::SETTINGS))) {
                            settings->returnScreen = ScreenType::MAIN_MENU;
                        }
                        nextScreen = ScreenType::SETTINGS;
                    }
                    if (i == 3) { nextScreen = ScreenType::QUIT; }
                    break;
                }
            }
        }
        break;
    }
    case MenuState::MAPS: {
        if (input.isKeyDown(SDL_SCANCODE_ESCAPE)) { menuState_ = MenuState::MAIN; scrollOffset_ = 0; }
        if (input.scrollY != 0) { scrollOffset_ -= input.scrollY; scrollOffset_ = std::max(0, scrollOffset_); }

        if (clicked) {

            float u2 = H / 100.0f * Engine::instance().uiScaleFactor();
            float winW2 = std::round(W * 0.60f);
            winW2 = std::clamp(winW2, 700.0f, 1600.0f);
            float winH2 = std::round(H * 0.72f);
            float winX2 = std::round((W - winW2) / 2.0f);
            float winY2 = std::round((H - winH2) / 2.0f);
            float pad2 = std::round(u2 * 1.8f);
            float headerH2 = std::round(u2 * 5.0f);
            float contentY2 = winY2 + headerH2 + pad2;
            float contentH2 = winH2 - headerH2 - pad2 * 2 - u2 * 6.5f;
            float listW2 = std::round(winW2 * 0.35f);
            float listHdrH2 = std::round(u2 * 3.0f);
            float row_h2 = std::round(u2 * 3.8f);
            float listBodyY2 = contentY2 + listHdrH2 + 2;
            float listBodyH2 = contentH2 - listHdrH2 - 2;
            float listX2 = winX2 + pad2;
            float listY2 = listBodyY2 + 2;


            for (int m2 = 0; m2 < (int)mapList_.size(); m2++) {
                float ry = listY2 + m2 * row_h2 - scrollOffset_ * row_h2;
                if (ry + row_h2 < listBodyY2 || ry > listBodyY2 + listBodyH2) continue;
                if (mx >= listX2 && mx <= listX2 + listW2 && my >= ry && my <= ry + row_h2) {
                    selectedMap_ = mapList_[m2];
                    Audio::instance().playSound("clickedSound");
                    std::string mapPath = Engine::instance().assetsPath + "starts/" + selectedMap_ + "/map.png";
                    SDL_Surface* m3 = Engine::instance().loadSurface(mapPath);
                    if (m3) { if (s_menuBG.mapSurf) SDL_FreeSurface(s_menuBG.mapSurf); s_menuBG.init(m3); }
                }
            }


            float footerY2 = winY2 + winH2 - std::round(u2 * 6.0f);
            float btnH3 = std::round(u2 * 4.8f);
            float btnW3 = std::round(winW2 * 0.22f);
            float btnGap3 = std::round(u2 * 1.5f);
            float totalBW = btnW3 * 2 + btnGap3;
            float bx2 = winX2 + (winW2 - totalBW) / 2;
            float by2 = footerY2 + std::round(u2 * 0.8f);


            if (mx >= bx2 && mx <= bx2 + btnW3 && my >= by2 && my <= by2 + btnH3) {
                Audio::instance().playSound("clickedSound");
                menuState_ = MenuState::MAIN; scrollOffset_ = 0;
            }

            float contX = bx2 + btnW3 + btnGap3;
            if (mx >= contX && mx <= contX + btnW3 && my >= by2 && my <= by2 + btnH3 && !selectedMap_.empty()) {
                Audio::instance().playSound("clickedSound");
                countryNames_.clear();
                std::string polMapPath = Engine::instance().assetsPath + "starts/" + selectedMap_ + "/map.png";
                SDL_Surface* polSurf = Engine::instance().loadSurface(polMapPath);
                if (polSurf) {
                    std::set<std::string> unique;
                    auto& rd = RegionData::instance();
                    auto& cd = CountryData::instance();
                    for (int rid = 1; rid <= rd.regionCount(); rid++) {
                        Vec2 loc = rd.getLocation(rid);
                        int px2 = (int)loc.x, py2 = (int)loc.y;
                        if (px2 >= 0 && py2 >= 0 && px2 < polSurf->w && py2 < polSurf->h) {
                            Color pixel = getPixel(polSurf, px2, py2);
                            std::string cn = cd.colorToCountry(pixel);
                            if (!cn.empty()) unique.insert(cn);
                        }
                    }
                    SDL_FreeSurface(polSurf);
                    for (auto& c : unique) countryNames_.push_back(c);
                }
                std::sort(countryNames_.begin(), countryNames_.end());
                menuState_ = MenuState::COUNTRIES; scrollOffset_ = 0;
            }
        }
        break;
    }
    case MenuState::COUNTRIES: {
        if (input.isKeyDown(SDL_SCANCODE_ESCAPE)) { menuState_ = MenuState::MAPS; scrollOffset_ = 0; }
        if (input.scrollY != 0) { scrollOffset_ -= input.scrollY; scrollOffset_ = std::max(0, scrollOffset_); }

        if (clicked) {

            float u2 = H / 100.0f * Engine::instance().uiScaleFactor();
            float winW2 = std::round(W * 0.72f);
            winW2 = std::clamp(winW2, 900.0f, 1900.0f);
            float winH2 = std::round(H * 0.82f);
            float winX2 = std::round((W - winW2) / 2.0f);
            float winY2 = std::round((H - winH2) / 2.0f);
            float pad2 = std::round(u2 * 1.8f);
            float headerH2 = std::round(u2 * 5.0f);
            float contentY2 = winY2 + headerH2 + pad2;
            float footerH2 = std::round(u2 * 6.5f);


            const char* mc[] = {"French","American","British","German","Japanese","Russian","Chinese","Indian"};
            std::vector<std::string> am;
            for (int i2 = 0; i2 < 8; i2++) {
                std::string cn2 = findMajorOnMap(mc[i2], countryNames_);
                if (!cn2.empty()) am.push_back(cn2);
            }
            int nm = std::max(1, (int)am.size());
            float cGap = std::round(u2 * 0.8f);
            float cArea = winW2 - pad2 * 2;
            float cW = std::min((cArea - (nm - 1) * cGap) / nm, std::round(u2 * 10.0f));
            float cH = std::round(u2 * 9.0f);
            float cTotal = nm * cW + (nm - 1) * cGap;
            float cX = winX2 + (winW2 - cTotal) / 2;
            float cY = contentY2 + std::round(u2 * 2.0f);
            for (int i2 = 0; i2 < (int)am.size(); i2++) {
                float cx2 = cX + i2 * (cW + cGap);
                if (mx >= cx2 && mx <= cx2 + cW && my >= cY && my <= cY + cH) {
                    selectedCountry_ = am[i2];
                    Audio::instance().playSound("clickedSound");
                }
            }


            float lowerY2 = cY + cH + std::round(u2 * 1.5f) + std::round(u2 * 1.8f);
            float listW2 = std::round(winW2 * 0.42f);
            float listH2 = winY2 + winH2 - lowerY2 - footerH2;
            float row_h2 = std::round(u2 * 3.2f);
            float listX2 = winX2 + pad2;
            float listY2 = lowerY2 + 2;
            for (int c = 0; c < (int)countryNames_.size(); c++) {
                float ry = listY2 + c * row_h2 - scrollOffset_ * row_h2;
                if (ry + row_h2 < lowerY2 || ry > lowerY2 + listH2) continue;
                if (mx >= listX2 && mx <= listX2 + listW2 && my >= ry && my <= ry + row_h2) {
                    selectedCountry_ = countryNames_[c];
                    Audio::instance().playSound("clickedSound");
                }
            }


            float footerY2 = winY2 + winH2 - footerH2;
            float btnH3 = std::round(u2 * 4.5f);
            float btnGap3 = std::round(u2 * 1.2f);
            float btnW3 = (winW2 - pad2 * 2.0f - btnGap3 * 3.0f) / 4.0f;
            btnW3 = std::clamp(btnW3, std::round(u2 * 9.5f), std::round(u2 * 13.5f));
            float totalBtnW = btnW3 * 4 + btnGap3 * 3;
            float bx2 = winX2 + (winW2 - totalBtnW) / 2;
            float by2 = footerY2 + std::round(u2 * 1.0f);

            if (mx >= bx2 && mx <= bx2 + btnW3 && my >= by2 && my <= by2 + btnH3) {
                Audio::instance().playSound("clickedSound");
                menuState_ = MenuState::MAPS; scrollOffset_ = 0;
            }
            float mapsX = bx2 + btnW3 + btnGap3;
            if (mx >= mapsX && mx <= mapsX + btnW3 && my >= by2 && my <= by2 + btnH3) {
                Audio::instance().playSound("clickedSound");
                menuState_ = MenuState::MAPS; scrollOffset_ = 0;
            }
            float startX = mapsX + btnW3 + btnGap3;
            if (mx >= startX && mx <= startX + btnW3 && my >= by2 && my <= by2 + btnH3 && !selectedCountry_.empty()) {
                Audio::instance().playSound("startGameSound");
                auto& gs = app.gameState();
                gs.controlledCountry = selectedCountry_;
                gs.mapViewOnly = false;
                gs.mapName = selectedMap_;
                gs.inGame = false;
                nextScreen = ScreenType::GAME;
            }
            float mapViewX = startX + btnW3 + btnGap3;
            if (mx >= mapViewX && mx <= mapViewX + btnW3 && my >= by2 && my <= by2 + btnH3) {
                Audio::instance().playSound("clickedSound");
                auto& gs = app.gameState();
                gs.controlledCountry.clear();
                gs.mapViewOnly = true;
                gs.speed = 0;
                gs.mapName = selectedMap_;
                gs.inGame = false;
                nextScreen = ScreenType::GAME;
            }
        }
        break;
    }
    case MenuState::SAVES: {
        if (input.scrollY != 0) { scrollOffset_ -= input.scrollY; scrollOffset_ = std::max(0, scrollOffset_); }

        if (input.isKeyDown(SDL_SCANCODE_ESCAPE)) { menuState_ = MenuState::MAIN; scrollOffset_ = 0; }
        break;
    }
    case MenuState::SETTINGS:
        if (auto* settings = dynamic_cast<SettingsScreen*>(app.screen(ScreenType::SETTINGS))) {
            settings->returnScreen = ScreenType::MAIN_MENU;
        }
        nextScreen = ScreenType::SETTINGS;
        break;
    }
}

void MainMenuScreen::update(App& app, float dt) { animTimer_ += dt; }


void MainMenuScreen::render(App& app) {
    auto& eng = Engine::instance();
    int W = eng.WIDTH, H = eng.HEIGHT;


    s_menuBG.update(eng.renderer, W, H, eng.FPS);
    if (!s_menuBG.initted) eng.clear(Theme::bg_dark);

    switch (menuState_) {
    case MenuState::MAIN:      renderMainMenu(app); break;
    case MenuState::MAPS:      renderMapsMenu(app); break;
    case MenuState::COUNTRIES:  renderCountriesMenu(app); break;
    case MenuState::SAVES:     renderSavesMenu(app); break;
    case MenuState::SETTINGS:
        if (auto* settings = dynamic_cast<SettingsScreen*>(app.screen(ScreenType::SETTINGS))) {
            settings->returnScreen = ScreenType::MAIN_MENU;
        }
        nextScreen = ScreenType::SETTINGS;
        break;
    }
}


void MainMenuScreen::renderMainMenu(App& app) {
    auto& eng = Engine::instance();
    auto* r = eng.renderer;
    int W = eng.WIDTH, H = eng.HEIGHT;
    int mx, my; SDL_GetMouseState(&mx, &my);
    bool pressed = SDL_GetMouseState(nullptr, nullptr) & SDL_BUTTON_LMASK;
    auto& assets = UIAssets::instance();


    float u = H / 100.0f * eng.uiScaleFactor();


    UIPrim::drawMapDim(r, W, H, 190);

    UIPrim::drawVignette(r, W, H, 200);

    UIPrim::drawGradientV(r, 0, 0, (float)W, H * 0.35f,
                          {0, 0, 0, 160}, {0, 0, 0, 0});

    UIPrim::drawGradientV(r, 0, H * 0.80f, (float)W, H * 0.20f,
                          {0, 0, 0, 0}, {0, 0, 0, 180});


    if (!backgroundTex_) {
        backgroundTex_ = eng.loadTexture(eng.assetsPath + "img/logo.png");
    }
    float logoH = H * 0.18f;
    float logoY = u * 4;
    if (backgroundTex_) {
        int lw, lh;
        SDL_QueryTexture(backgroundTex_, nullptr, nullptr, &lw, &lh);
        float logoW = static_cast<float>(lw) / lh * logoH;
        SDL_Rect dst = {(int)(W / 2.0f - logoW / 2), (int)logoY, (int)logoW, (int)logoH};
        SDL_RenderCopy(r, backgroundTex_, nullptr, &dst);
    }


    float panelW = std::round(W * 0.42f);
    panelW = std::clamp(panelW, 500.0f, 1100.0f);
    float panelX = std::round((W - panelW) / 2.0f);


    float outerPad = std::round(u * 2.0f);
    float sectionGap = std::round(u * 1.5f);


    float headerH = std::round(u * 5.5f);

    float btnH = std::round(u * 6.5f);
    float btnGap = std::round(u * 1.2f);
    float btnRegionPad = std::round(u * 2.2f);


    float versionH = std::round(u * 3.0f);


    float btnGroupH = 4 * btnH + 3 * btnGap;
    float bodyH = btnRegionPad + btnGroupH + btnRegionPad;
    float totalH = headerH + bodyH + versionH;
    float panelY = std::round(logoY + logoH + u * 3.5f);


    if (panelY + totalH > H - u * 6) {
        panelY = H - u * 6 - totalH;
    }


    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    for (int s = 24; s >= 1; s--) {
        int a = static_cast<int>(90.0f * s / 24.0f * 0.4f);
        SDL_SetRenderDrawColor(r, 0, 0, 0, (uint8_t)std::min(255, a));
        SDL_Rect sh = {(int)panelX - s/2, (int)panelY + s, (int)panelW + s, (int)totalH};
        SDL_RenderFillRect(r, &sh);
    }


    UIPrim::drawRectFilled(r, {8, 10, 14}, panelX - 2, panelY - 2, panelW + 4, totalH + 4);


    UIPrim::drawRoundedRect(r, {18, 20, 26}, panelX, panelY, panelW, totalH, 8);


    SDL_Texture* windowTex = assets.panelBodyHeaded();
    if (!windowTex) windowTex = assets.panelBodyHeadless();
    if (windowTex) {
        UIAssets::draw9Slice(r, windowTex, panelX, panelY, panelW, totalH, 28);
    }


    SDL_Texture* headerTex = assets.panelHeader();
    if (headerTex) {
        UIAssets::draw9Slice(r, headerTex, panelX + 3, panelY + 3, panelW - 6, headerH, 14);
    } else {
        UIPrim::drawRectFilled(r, {14, 16, 22}, panelX + 3, panelY + 3, panelW - 6, headerH);
    }


    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, 255, 255, 255, 8);
    SDL_Rect hdrHl = {(int)panelX + 6, (int)panelY + 4, (int)panelW - 12, (int)(headerH * 0.3f)};
    SDL_RenderFillRect(r, &hdrHl);


    float goldLineY = panelY + headerH + 2;
    UIPrim::drawHLine(r, Theme::gold, panelX + 8, panelX + panelW - 8, goldLineY, 2);
    UIPrim::drawHLine(r, {Theme::gold_dim.r, Theme::gold_dim.g, Theme::gold_dim.b, 60},
                      panelX + 16, panelX + panelW - 16, goldLineY + 3, 1);


    int headerFs = std::max(16, (int)(headerH * 0.38f));
    UIPrim::drawText(r, "COMMUNITY EDITION", headerFs,
                     panelX + panelW / 2, panelY + headerH * 0.38f, "center", Theme::gold_bright, true);
    int subFs = std::max(12, (int)(headerH * 0.24f));
    UIPrim::drawText(r, "Version 2.0", subFs,
                     panelX + panelW / 2, panelY + headerH * 0.72f, "center", Theme::gold_dim, false);


    float wellX = panelX + outerPad;
    float wellY = goldLineY + sectionGap;
    float wellW = panelW - outerPad * 2;
    float wellH = bodyH;


    UIPrim::drawRoundedRect(r, {12, 14, 18}, wellX, wellY, wellW, wellH, 6);

    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, 0, 0, 0, 30);
    SDL_Rect wellShadow = {(int)wellX + 2, (int)wellY + 1, (int)wellW - 4, 4};
    SDL_RenderFillRect(r, &wellShadow);

    SDL_SetRenderDrawColor(r, Theme::border.r, Theme::border.g, Theme::border.b, 80);
    SDL_Rect wellBorder = {(int)wellX, (int)wellY, (int)wellW, (int)wellH};
    SDL_RenderDrawRect(r, &wellBorder);


    float btnW = std::round(wellW - outerPad * 2);
    float btnX = std::round(wellX + outerPad);
    float btnStartY = std::round(wellY + btnRegionPad);

    struct MenuBtn { const char* label; const char* icon; };
    MenuBtn btns[] = {
        {"NEW GAME",   "Play"},
        {"LOAD GAME",  "Home"},
        {"SETTINGS",   "Gear"},
        {"QUIT",       "Exit"},
    };

    SDL_Texture* btnPanelTex = assets.panelBodyHeadless();

    for (int i = 0; i < 4; i++) {
        float by = std::round(btnStartY + i * (btnH + btnGap));
        bool hov = mx >= btnX && mx <= btnX + btnW && my >= by && my <= by + btnH;
        bool isPress = hov && pressed;


        bool wasHov = (hoveredButton_ == i);
        if (hov && !wasHov) {
            hoveredButton_ = i;
            Audio::instance().playSound("hoveredSound");
        }
        if (!hov && wasHov && hoveredButton_ == i) {
            hoveredButton_ = -1;
        }

        float drawY = isPress ? by + 2 : by;


        if (btnPanelTex) {
            uint8_t mod = isPress ? 214 : (hov ? 255 : 238);
            SDL_SetTextureColorMod(btnPanelTex, mod, mod, mod);
            SDL_SetTextureAlphaMod(btnPanelTex, hov ? 255 : 246);
            UIAssets::draw9Slice(r, btnPanelTex, btnX, drawY, btnW, btnH, 18);
            SDL_SetTextureColorMod(btnPanelTex, 255, 255, 255);
            SDL_SetTextureAlphaMod(btnPanelTex, 255);
        } else {
            Color bg = isPress ? Color{30, 32, 38} : (hov ? Color{55, 58, 68} : Color{38, 42, 50});
            UIPrim::drawRoundedRect(r, bg, btnX, drawY, btnW, btnH, 6, hov ? Theme::gold_dim : Theme::border);
        }


        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(r, 255, 255, 255, hov ? 14 : 5);
        SDL_Rect btnHl = {(int)btnX + 6, (int)drawY + 2, (int)btnW - 12, (int)(btnH * 0.22f)};
        SDL_RenderFillRect(r, &btnHl);


        {
            uint8_t barAlpha = isPress ? 255 : (hov ? 240 : 100);
            SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(r, Theme::gold.r, Theme::gold.g, Theme::gold.b, barAlpha);
            SDL_Rect bar = {(int)btnX + 5, (int)drawY + 6, 5, (int)btnH - 12};
            SDL_RenderFillRect(r, &bar);
        }


        if (hov) {
            UIPrim::drawHLine(r, Theme::gold, btnX + 10, btnX + btnW - 10, drawY + 2, 1);
        }


        SDL_SetRenderDrawColor(r, 0, 0, 0, 40);
        SDL_RenderDrawLine(r, (int)btnX + 6, (int)(drawY + btnH - 2),
                          (int)(btnX + btnW - 6), (int)(drawY + btnH - 2));


        SDL_Texture* iconTex = assets.icon(btns[i].icon);
        float iconSz = std::round(btnH * 0.48f);
        float iconAreaW = btnH;
        if (iconTex) {
            float iconX = std::round(btnX + iconAreaW * 0.55f - iconSz / 2);
            float iconY2 = std::round(drawY + (btnH - iconSz) / 2.0f);
            SDL_SetTextureAlphaMod(iconTex, hov ? 255 : 150);
            if (isPress) SDL_SetTextureColorMod(iconTex, Theme::gold.r, Theme::gold.g, Theme::gold.b);
            else if (hov) SDL_SetTextureColorMod(iconTex, Theme::gold_bright.r, Theme::gold_bright.g, Theme::gold_bright.b);
            else SDL_SetTextureColorMod(iconTex, 180, 175, 165);
            SDL_Rect idst = {(int)iconX, (int)iconY2, (int)iconSz, (int)iconSz};
            SDL_RenderCopy(r, iconTex, nullptr, &idst);
            SDL_SetTextureColorMod(iconTex, 255, 255, 255);
            SDL_SetTextureAlphaMod(iconTex, 255);
        }


        SDL_SetRenderDrawColor(r, Theme::border.r, Theme::border.g, Theme::border.b, hov ? 100 : 40);
        float sepX = btnX + iconAreaW;
        SDL_RenderDrawLine(r, (int)sepX, (int)drawY + 8, (int)sepX, (int)(drawY + btnH - 8));


        int fs = std::max(18, (int)(btnH * 0.34f));
        Color tc = isPress ? Theme::gold : (hov ? Theme::gold_bright : Theme::cream);
        float textX = btnX + iconAreaW + std::round(u * 1.5f);
        UIPrim::drawText(r, btns[i].label, fs, textX, drawY + btnH / 2, "midleft", tc, true);


        SDL_Texture* arrowIcon = assets.icon("SolidArrow-Right");
        if (arrowIcon) {
            float arrowSz = std::round(btnH * 0.22f);
            float arrowX = btnX + btnW - arrowSz - std::round(u * 1.5f);
            float arrowY = drawY + (btnH - arrowSz) / 2;
            SDL_SetTextureAlphaMod(arrowIcon, hov ? 180 : 60);
            SDL_SetTextureColorMod(arrowIcon, Theme::gold_dim.r, Theme::gold_dim.g, Theme::gold_dim.b);
            SDL_Rect aDst = {(int)arrowX, (int)arrowY, (int)arrowSz, (int)arrowSz};
            SDL_RenderCopy(r, arrowIcon, nullptr, &aDst);
            SDL_SetTextureColorMod(arrowIcon, 255, 255, 255);
            SDL_SetTextureAlphaMod(arrowIcon, 255);
        }
    }


    float verY = panelY + totalH - versionH;

    UIPrim::drawRectFilled(r, {10, 12, 16}, panelX + 3, verY, panelW - 6, versionH - 3);

    UIPrim::drawHLine(r, {Theme::gold_dim.r, Theme::gold_dim.g, Theme::gold_dim.b, 60},
                      panelX + 16, panelX + panelW - 16, verY, 1);
    int verFs = std::max(11, (int)(versionH * 0.35f));
    UIPrim::drawText(r, "Spirits of Steel: A New Dawn", verFs,
                     panelX + panelW / 2, verY + versionH * 0.5f, "center",
                     {Theme::grey.r, Theme::grey.g, Theme::grey.b, 180}, false);


    UIPrim::drawOrnamentalFrame(r, panelX, panelY, panelW, totalH, 2);


    int credFs = std::max(12, (int)(u * 1.1f));
    float credY = (float)(H - u * 2.5f);
    UIPrim::drawText(r, "A Game By Gavin Grubert", credFs,
                     u * 1.5f, credY, "midleft", {Theme::grey.r, Theme::grey.g, Theme::grey.b, 120}, false);
}


void MainMenuScreen::renderMapsMenu(App& app) {
    auto& eng = Engine::instance();
    auto* r = eng.renderer;
    int W = eng.WIDTH, H = eng.HEIGHT;
    float u = H / 100.0f * eng.uiScaleFactor();
    int mx, my; SDL_GetMouseState(&mx, &my);
    bool pressed1 = SDL_GetMouseState(nullptr,nullptr) & SDL_BUTTON_LMASK;
    auto& assets = UIAssets::instance();


    UIPrim::drawMapDim(r, W, H, 190);
    UIPrim::drawVignette(r, W, H, 180);


    float winW = std::round(W * 0.60f);
    winW = std::clamp(winW, 700.0f, 1600.0f);
    float winH = std::round(H * 0.72f);
    float winX = std::round((W - winW) / 2.0f);
    float winY = std::round((H - winH) / 2.0f);
    float pad = std::round(u * 1.8f);
    float headerH = std::round(u * 5.0f);


    for (int s = 20; s >= 1; s--) {
        int a = (int)(80.0f * s / 20.0f * 0.4f);
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(r, 0, 0, 0, (uint8_t)std::min(255, a));
        SDL_Rect sh = {(int)winX - s/2, (int)winY + s, (int)winW + s, (int)winH};
        SDL_RenderFillRect(r, &sh);
    }


    UIPrim::drawRoundedRect(r, {18, 20, 26}, winX, winY, winW, winH, 8);
    SDL_Texture* windowTex = assets.panelBodyHeaded();
    if (!windowTex) windowTex = assets.panelBodyHeadless();
    if (windowTex) UIAssets::draw9Slice(r, windowTex, winX, winY, winW, winH, 28);


    SDL_Texture* headerTex = assets.panelHeader();
    if (headerTex) UIAssets::draw9Slice(r, headerTex, winX + 3, winY + 3, winW - 6, headerH, 14);
    int titleFs = std::max(16, (int)(headerH * 0.42f));
    UIPrim::drawText(r, "SELECT SCENARIO", titleFs, winX + winW / 2, winY + headerH * 0.5f, "center", Theme::gold_bright, true);
    UIPrim::drawHLine(r, Theme::gold, winX + 8, winX + winW - 8, winY + headerH + 2, 2);


    float contentY = winY + headerH + pad;
    float contentH = winH - headerH - pad * 2 - u * 6.5f;
    float listW = std::round(winW * 0.35f);
    float detailW = winW - listW - pad * 3;
    float detailX = winX + pad + listW + pad;


    float listX = winX + pad;
    UIPrim::drawRoundedRect(r, {10, 12, 16}, listX, contentY, listW, contentH, 6);
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, Theme::border.r, Theme::border.g, Theme::border.b, 60);
    SDL_Rect listBorder = {(int)listX, (int)contentY, (int)listW, (int)contentH};
    SDL_RenderDrawRect(r, &listBorder);


    float listHdrH = std::round(u * 3.0f);
    UIPrim::drawRectFilled(r, {14, 16, 22}, listX + 1, contentY + 1, listW - 2, listHdrH);
    int listHdrFs = std::max(12, (int)(listHdrH * 0.42f));
    UIPrim::drawText(r, "AVAILABLE MAPS", listHdrFs, listX + listW / 2, contentY + listHdrH * 0.5f, "center", Theme::gold_dim, false);
    UIPrim::drawHLine(r, Theme::gold_dim, listX + 8, listX + listW - 8, contentY + listHdrH, 1);


    float row_h = std::round(u * 3.8f);
    float listBodyY = contentY + listHdrH + 2;
    float listBodyH = contentH - listHdrH - 2;


    SDL_Rect clipList = {(int)listX, (int)listBodyY, (int)listW, (int)listBodyH};
    SDL_RenderSetClipRect(r, &clipList);

    float listY = listBodyY + 2;
    for (int m = 0; m < (int)mapList_.size(); m++) {
        float ry = listY + m * row_h - scrollOffset_ * row_h;
        if (ry + row_h < listBodyY || ry > listBodyY + listBodyH) continue;

        bool isSel = (mapList_[m] == selectedMap_);
        bool isHov = mx >= listX && mx <= listX + listW && my >= ry && my <= ry + row_h;


        if (isSel) {
            UIPrim::drawRectFilled(r, {35, 38, 48}, listX + 2, ry, listW - 4, row_h);
        } else if (isHov) {
            UIPrim::drawRectFilled(r, {28, 32, 40}, listX + 2, ry, listW - 4, row_h);
        } else if (m % 2) {
            SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(r, 255, 255, 255, 3);
            SDL_Rect altBg = {(int)listX + 2, (int)ry, (int)listW - 4, (int)row_h};
            SDL_RenderFillRect(r, &altBg);
        }


        if (isSel) {
            SDL_SetRenderDrawColor(r, Theme::gold.r, Theme::gold.g, Theme::gold.b, 255);
            SDL_Rect selBar = {(int)listX + 2, (int)ry, 4, (int)row_h};
            SDL_RenderFillRect(r, &selBar);
        }


        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(r, Theme::border.r, Theme::border.g, Theme::border.b, 25);
        SDL_RenderDrawLine(r, (int)listX + 8, (int)(ry + row_h - 1), (int)(listX + listW - 8), (int)(ry + row_h - 1));

        int fs = std::max(14, (int)(row_h * 0.38f));
        Color tc = isSel ? Theme::gold_bright : (isHov ? Theme::cream : Color{190, 185, 175});
        UIPrim::drawText(r, mapList_[m], fs, listX + 16, ry + row_h / 2, "midleft", tc, isSel);
    }
    SDL_RenderSetClipRect(r, nullptr);


    UIPrim::drawRoundedRect(r, {10, 12, 16}, detailX, contentY, detailW, contentH, 6);
    SDL_SetRenderDrawColor(r, Theme::border.r, Theme::border.g, Theme::border.b, 60);
    SDL_Rect detBorder = {(int)detailX, (int)contentY, (int)detailW, (int)contentH};
    SDL_RenderDrawRect(r, &detBorder);


    float prevPad = std::round(u * 1.0f);
    float prevW = detailW - prevPad * 2;
    float prevH = std::round(prevW * 0.47f);
    if (s_menuBG.cachedTex) {

        UIPrim::drawRectFilled(r, {6, 8, 10}, detailX + prevPad - 2, contentY + prevPad - 2, prevW + 4, prevH + 4);
        SDL_Rect prevDst = {(int)(detailX + prevPad), (int)(contentY + prevPad), (int)prevW, (int)prevH};
        SDL_RenderCopy(r, s_menuBG.cachedTex, nullptr, &prevDst);

        SDL_SetRenderDrawColor(r, Theme::gold_dim.r, Theme::gold_dim.g, Theme::gold_dim.b, 100);
        SDL_RenderDrawRect(r, &prevDst);
    }


    float infoY = contentY + prevPad + prevH + std::round(u * 1.5f);
    int nameFs = std::max(16, (int)(u * 1.6f));
    UIPrim::drawText(r, selectedMap_, nameFs, detailX + detailW / 2, infoY, "topcenter", Theme::gold_bright, true);
    infoY += std::round(u * 2.5f);


    UIPrim::drawHLine(r, Theme::gold_dim, detailX + prevPad, detailX + detailW - prevPad, infoY, 1);
    infoY += std::round(u * 1.2f);


    std::string startsDir = eng.assetsPath + "starts/" + selectedMap_ + "/";
    auto readFile = [](const std::string& path) -> std::string {
        if (!fs::exists(path)) return "";
        std::ifstream f(path); std::string s; std::getline(f, s); return s;
    };
    std::string creator = readFile(startsDir + "creator.txt");
    std::string tags = readFile(startsDir + "tags.txt");
    std::string startDate = readFile(startsDir + "startDate.txt");

    float rowH = std::round(u * 2.8f);
    float infoW = detailW - prevPad * 2;
    UIPrim::drawInfoRow(r, detailX + prevPad, infoY, infoW, rowH, "Start Date", startDate.empty() ? "---" : startDate); infoY += rowH;
    UIPrim::drawInfoRow(r, detailX + prevPad, infoY, infoW, rowH, "Tags", tags.empty() ? "---" : tags); infoY += rowH;
    UIPrim::drawInfoRow(r, detailX + prevPad, infoY, infoW, rowH, "Creator", creator.empty() ? "---" : creator); infoY += rowH;


    float footerY = winY + winH - std::round(u * 6.0f);
    UIPrim::drawHLine(r, {Theme::gold_dim.r, Theme::gold_dim.g, Theme::gold_dim.b, 40},
                      winX + pad, winX + winW - pad, footerY, 1);
    float btnH2 = std::round(u * 4.8f);
    float btnW2 = std::round(winW * 0.22f);
    float btnGap = std::round(u * 1.5f);
    float totalBtnW = btnW2 * 2 + btnGap;
    float btnBaseX = winX + (winW - totalBtnW) / 2;
    float btnBaseY = footerY + std::round(u * 0.8f);

    UIPrim::drawMenuButton(r, btnBaseX, btnBaseY, btnW2, btnH2, "Back", mx, my, false);
    UIPrim::drawMenuButton(r, btnBaseX + btnW2 + btnGap, btnBaseY, btnW2, btnH2, "Continue", mx, my, false);


    UIPrim::drawOrnamentalFrame(r, winX, winY, winW, winH, 2);
}


void MainMenuScreen::renderCountriesMenu(App& app) {
    auto& eng = Engine::instance();
    auto* r = eng.renderer;
    int W = eng.WIDTH, H = eng.HEIGHT;
    float u = H / 100.0f * eng.uiScaleFactor();
    int mx, my; SDL_GetMouseState(&mx, &my);
    auto& assets = UIAssets::instance();


    UIPrim::drawMapDim(r, W, H, 190);
    UIPrim::drawVignette(r, W, H, 180);


    float winW = std::round(W * 0.72f);
    winW = std::clamp(winW, 900.0f, 1900.0f);
    float winH = std::round(H * 0.82f);
    float winX = std::round((W - winW) / 2.0f);
    float winY = std::round((H - winH) / 2.0f);
    float pad = std::round(u * 1.8f);
    float headerH = std::round(u * 5.0f);


    for (int s = 20; s >= 1; s--) {
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(r, 0, 0, 0, (uint8_t)std::min(255, (int)(80.0f * s / 20.0f * 0.4f)));
        SDL_Rect sh = {(int)winX - s/2, (int)winY + s, (int)winW + s, (int)winH};
        SDL_RenderFillRect(r, &sh);
    }
    UIPrim::drawRoundedRect(r, {18, 20, 26}, winX, winY, winW, winH, 8);
    SDL_Texture* windowTex = assets.panelBodyHeaded();
    if (!windowTex) windowTex = assets.panelBodyHeadless();
    if (windowTex) UIAssets::draw9Slice(r, windowTex, winX, winY, winW, winH, 28);


    SDL_Texture* headerTex = assets.panelHeader();
    if (headerTex) UIAssets::draw9Slice(r, headerTex, winX + 3, winY + 3, winW - 6, headerH, 14);
    int titleFs = std::max(16, (int)(headerH * 0.42f));
    UIPrim::drawText(r, "CHOOSE YOUR NATION", titleFs, winX + winW / 2, winY + headerH * 0.5f, "center", Theme::gold_bright, true);
    UIPrim::drawHLine(r, Theme::gold, winX + 8, winX + winW - 8, winY + headerH + 2, 2);


    float contentY = winY + headerH + pad;
    float footerH = std::round(u * 6.5f);
    float contentH = winH - headerH - pad - footerH;


    const char* majorCultures[] = {"French","American","British","German","Japanese","Russian","Chinese","Indian"};
    std::vector<std::string> activeMajors;
    for (int i = 0; i < 8; i++) {
        std::string cname = findMajorOnMap(majorCultures[i], countryNames_);
        if (!cname.empty()) activeMajors.push_back(cname);
    }


    int secFs = std::max(12, (int)(u * 1.1f));
    UIPrim::drawText(r, "MAJOR POWERS", secFs, winX + pad + 4, contentY, "topleft", Theme::gold_dim, false);
    float cardY = contentY + std::round(u * 2.0f);


    int numMajors = std::max(1, (int)activeMajors.size());
    float cardAreaW = winW - pad * 2;
    float cardGap = std::round(u * 0.8f);
    float cardW = std::min((cardAreaW - (numMajors - 1) * cardGap) / numMajors, std::round(u * 10.0f));
    float cardH = std::round(u * 9.0f);
    float cardsTotal = numMajors * cardW + (numMajors - 1) * cardGap;
    float cardsX = winX + (winW - cardsTotal) / 2;

    SDL_Texture* cardBg = assets.panelBodyHeadless();
    for (int i = 0; i < (int)activeMajors.size(); i++) {
        float cx = cardsX + i * (cardW + cardGap);
        bool isSel = (selectedCountry_ == activeMajors[i]);
        bool isHov = mx >= cx && mx <= cx + cardW && my >= cardY && my <= cardY + cardH;


        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(r, 0, 0, 0, 30);
        SDL_Rect csh = {(int)cx + 2, (int)cardY + 3, (int)cardW, (int)cardH};
        SDL_RenderFillRect(r, &csh);


        if (cardBg) {
            if (isSel) SDL_SetTextureColorMod(cardBg, 170, 160, 130);
            else if (isHov) SDL_SetTextureColorMod(cardBg, 180, 178, 175);
            else SDL_SetTextureColorMod(cardBg, 145, 143, 140);
            UIAssets::draw9Slice(r, cardBg, cx, cardY, cardW, cardH, 12);
            SDL_SetTextureColorMod(cardBg, 255, 255, 255);
        }


        if (isSel) {
            SDL_SetRenderDrawColor(r, Theme::gold.r, Theme::gold.g, Theme::gold.b, 255);
            for (int b = 0; b < 2; b++) {
                SDL_Rect sb = {(int)cx + b, (int)cardY + b, (int)cardW - b*2, (int)cardH - b*2};
                SDL_RenderDrawRect(r, &sb);
            }
        }


        if (i < 8) {
            SDL_Texture* cultureIcon = eng.loadTexture(eng.assetsPath + "img/" + std::string(majorCultures[i]) + ".png");
            if (cultureIcon) {
                int ciw, cih; SDL_QueryTexture(cultureIcon, nullptr, nullptr, &ciw, &cih);
                float iconSz = std::min(cardW * 0.65f, cardH * 0.35f);
                float iconW2 = iconSz, iconH2 = iconSz;
                if (ciw > cih) iconH2 = iconSz * cih / ciw;
                else iconW2 = iconSz * ciw / cih;
                SDL_Rect iDst = {(int)(cx + (cardW - iconW2) / 2), (int)(cardY + 4), (int)iconW2, (int)iconH2};
                SDL_RenderCopy(r, cultureIcon, nullptr, &iDst);
            }
        }


        std::string lowerName = activeMajors[i];
        std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
        SDL_Texture* flag = eng.loadTexture(eng.assetsPath + "flags/" + lowerName + "_flag.png");
        if (flag) {
            int fw, fh;
            SDL_QueryTexture(flag, nullptr, nullptr, &fw, &fh);
            float drawH = cardH * 0.35f;
            float drawW = drawH * fw / fh;
            drawW = std::min(drawW, cardW - 12);
            drawH = drawW * fh / fw;
            SDL_Rect fDst = {(int)(cx + (cardW - drawW) / 2), (int)(cardY + cardH * 0.38f), (int)drawW, (int)drawH};
            SDL_RenderCopy(r, flag, nullptr, &fDst);

            SDL_SetRenderDrawColor(r, Theme::border.r, Theme::border.g, Theme::border.b, 80);
            SDL_RenderDrawRect(r, &fDst);
        }


        std::string dispName = replaceAll(activeMajors[i], "_", " ");
        int nameFs = std::max(11, (int)(cardH * 0.12f));
        Color nameC = isSel ? Theme::gold_bright : (isHov ? Theme::cream : Color{185, 180, 170});
        UIPrim::drawText(r, dispName, nameFs, cx + cardW / 2, cardY + cardH - std::round(u * 1.2f), "center", nameC, isSel);
    }


    float lowerY = cardY + cardH + std::round(u * 1.5f);
    UIPrim::drawText(r, "ALL NATIONS", secFs, winX + pad + 4, lowerY, "topleft", Theme::gold_dim, false);
    lowerY += std::round(u * 1.8f);

    float listW = std::round(winW * 0.42f);
    float listH = winY + winH - lowerY - footerH;
    float row_h = std::round(u * 3.2f);
    float listX = winX + pad;


    UIPrim::drawRoundedRect(r, {10, 12, 16}, listX, lowerY, listW, listH, 6);
    SDL_SetRenderDrawColor(r, Theme::border.r, Theme::border.g, Theme::border.b, 50);
    SDL_Rect lb = {(int)listX, (int)lowerY, (int)listW, (int)listH};
    SDL_RenderDrawRect(r, &lb);


    SDL_Rect clipList = {(int)listX, (int)lowerY, (int)listW, (int)listH};
    SDL_RenderSetClipRect(r, &clipList);

    float listY = lowerY + 2;
    for (int c = 0; c < (int)countryNames_.size(); c++) {
        float ry = listY + c * row_h - scrollOffset_ * row_h;
        if (ry + row_h < lowerY || ry > lowerY + listH) continue;

        bool isSel = (countryNames_[c] == selectedCountry_);
        bool isHov = mx >= listX && mx <= listX + listW && my >= ry && my <= ry + row_h;

        if (isSel) UIPrim::drawRectFilled(r, {35, 38, 48}, listX + 2, ry, listW - 4, row_h);
        else if (isHov) UIPrim::drawRectFilled(r, {28, 32, 40}, listX + 2, ry, listW - 4, row_h);
        else if (c % 2) { SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND); SDL_SetRenderDrawColor(r, 255, 255, 255, 3); SDL_Rect a = {(int)listX+2,(int)ry,(int)listW-4,(int)row_h}; SDL_RenderFillRect(r, &a); }

        if (isSel) { SDL_SetRenderDrawColor(r, Theme::gold.r, Theme::gold.g, Theme::gold.b, 255); SDL_Rect sb = {(int)listX+2,(int)ry,4,(int)row_h}; SDL_RenderFillRect(r, &sb); }


        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(r, Theme::border.r, Theme::border.g, Theme::border.b, 20);
        SDL_RenderDrawLine(r, (int)listX + 8, (int)(ry + row_h - 1), (int)(listX + listW - 8), (int)(ry + row_h - 1));


        std::string ln = countryNames_[c];
        std::transform(ln.begin(), ln.end(), ln.begin(), ::tolower);
        SDL_Texture* ft = eng.loadTexture(eng.assetsPath + "flags/" + ln + "_flag.png");
        float flagH2 = row_h * 0.60f;
        if (ft) {
            int fw, fh; SDL_QueryTexture(ft, nullptr, nullptr, &fw, &fh);
            float flagW2 = flagH2 * fw / fh;
            SDL_Rect fd = {(int)(listX + 10), (int)(ry + (row_h - flagH2) / 2), (int)flagW2, (int)flagH2};
            SDL_RenderCopy(r, ft, nullptr, &fd);
        }

        int fs = std::max(12, (int)(row_h * 0.36f));
        std::string dn = replaceAll(countryNames_[c], "_", " ");
        Color tc = isSel ? Theme::gold_bright : (isHov ? Theme::cream : Color{185, 180, 170});
        UIPrim::drawText(r, dn, fs, listX + row_h * 1.5f, ry + row_h / 2, "midleft", tc, isSel);
    }
    SDL_RenderSetClipRect(r, nullptr);


    float detailX = listX + listW + pad;
    float detailW = winW - listW - pad * 3;
    UIPrim::drawRoundedRect(r, {10, 12, 16}, detailX, lowerY, detailW, listH, 6);
    SDL_SetRenderDrawColor(r, Theme::border.r, Theme::border.g, Theme::border.b, 50);
    SDL_Rect db = {(int)detailX, (int)lowerY, (int)detailW, (int)listH};
    SDL_RenderDrawRect(r, &db);

    if (!selectedCountry_.empty()) {
        float iy = lowerY + std::round(u * 1.2f);
        auto* rec = CountryData::instance().getRecord(selectedCountry_);
        std::string displayName = replaceAll(selectedCountry_, "_", " ");


        int fsName = std::max(16, (int)(u * 1.6f));
        UIPrim::drawText(r, displayName, fsName, detailX + detailW / 2, iy, "topcenter", Theme::gold_bright, true);
        iy += std::round(u * 2.5f);


        std::string ln3 = selectedCountry_;
        std::transform(ln3.begin(), ln3.end(), ln3.begin(), ::tolower);
        SDL_Texture* ft = eng.loadTexture(eng.assetsPath + "flags/" + ln3 + "_flag.png");
        if (ft) {
            int fw, fh; SDL_QueryTexture(ft, nullptr, nullptr, &fw, &fh);
            float dispW = std::min(detailW * 0.70f, 260.0f);
            float dispH = dispW * fh / fw;
            SDL_Rect fd = {(int)(detailX + (detailW - dispW) / 2), (int)iy, (int)dispW, (int)dispH};

            UIPrim::drawRectFilled(r, {6, 8, 10}, fd.x - 2, fd.y - 2, fd.w + 4, fd.h + 4);
            SDL_RenderCopy(r, ft, nullptr, &fd);
            SDL_SetRenderDrawColor(r, Theme::gold_dim.r, Theme::gold_dim.g, Theme::gold_dim.b, 100);
            SDL_RenderDrawRect(r, &fd);
            iy += dispH + std::round(u * 1.5f);
        }

        UIPrim::drawHLine(r, Theme::gold_dim, detailX + pad, detailX + detailW - pad, iy, 1);
        iy += std::round(u * 1.0f);

        if (rec) {
            float rowH2 = std::round(u * 2.8f);
            float infoW = detailW - pad * 2;
            UIPrim::drawInfoRow(r, detailX + pad, iy, infoW, rowH2, "Culture", rec->culture); iy += rowH2;
            UIPrim::drawInfoRow(r, detailX + pad, iy, infoW, rowH2, "Ideology", rec->ideology); iy += rowH2;
            UIPrim::drawInfoRow(r, detailX + pad, iy, infoW, rowH2, "Regions", std::to_string(rec->claims.size())); iy += rowH2;
            UIPrim::drawInfoRow(r, detailX + pad, iy, infoW, rowH2, "Stability", std::to_string(rec->baseStability)); iy += rowH2;
        }
    } else {

        SDL_Texture* starIcon = assets.icon("Star");
        if (starIcon) {
            float iSz = std::round(u * 3.0f);
            SDL_SetTextureColorMod(starIcon, 60, 60, 70);
            SDL_Rect id = {(int)(detailX + (detailW - iSz) / 2), (int)(lowerY + listH * 0.38f - iSz / 2), (int)iSz, (int)iSz};
            SDL_RenderCopy(r, starIcon, nullptr, &id);
            SDL_SetTextureColorMod(starIcon, 255, 255, 255);
        }
        int phFs = std::max(14, (int)(u * 1.2f));
        UIPrim::drawText(r, "Select a country", phFs, detailX + detailW / 2, lowerY + listH * 0.55f, "center", Theme::dark_grey);
    }


    float footerY = winY + winH - footerH;
    UIPrim::drawHLine(r, {Theme::gold_dim.r, Theme::gold_dim.g, Theme::gold_dim.b, 40}, winX + pad, winX + winW - pad, footerY, 1);
    float btnH2 = std::round(u * 4.5f);
    float btnGap2 = std::round(u * 1.2f);
    float btnW2 = (winW - pad * 2.0f - btnGap2 * 3.0f) / 4.0f;
    btnW2 = std::clamp(btnW2, std::round(u * 9.5f), std::round(u * 13.5f));
    float totalBtnW = btnW2 * 4 + btnGap2 * 3;
    float bx = winX + (winW - totalBtnW) / 2;
    float by = footerY + std::round(u * 1.0f);

    UIPrim::drawMenuButton(r, bx, by, btnW2, btnH2, "Back", mx, my, false);
    UIPrim::drawMenuButton(r, bx + btnW2 + btnGap2, by, btnW2, btnH2, "Maps", mx, my, false);

    float startX = bx + (btnW2 + btnGap2) * 2;
    if (!selectedCountry_.empty()) {
        bool startHov = mx >= startX && mx <= startX + btnW2 && my >= by && my <= by + btnH2;

        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(r, 60, 165, 80, startHov ? 50 : 25);
        SDL_Rect glow = {(int)startX - 3, (int)by - 3, (int)btnW2 + 6, (int)btnH2 + 6};
        SDL_RenderFillRect(r, &glow);
        Color sBg = startHov ? Color{50, 115, 65} : Color{38, 95, 52};
        UIPrim::drawRoundedRect(r, sBg, startX, by, btnW2, btnH2, 6, Theme::green);
        SDL_SetRenderDrawColor(r, 255, 255, 255, startHov ? 12 : 5);
        SDL_Rect shl = {(int)startX + 4, (int)by + 2, (int)btnW2 - 8, (int)(btnH2 * 0.25f)};
        SDL_RenderFillRect(r, &shl);
        Color stc = startHov ? Theme::gold_bright : Theme::cream;
        int sfs = std::max(14, (int)(btnH2 * 0.36f));
        UIPrim::drawText(r, "START GAME", sfs, startX + btnW2 / 2, by + btnH2 / 2, "center", stc, true);
    } else {
        UIPrim::drawMenuButton(r, startX, by, btnW2, btnH2, "Start Game", mx, my, false);
    }

    float mapViewX = startX + btnW2 + btnGap2;
    bool mapViewHov = mx >= mapViewX && mx <= mapViewX + btnW2 && my >= by && my <= by + btnH2;
    UIPrim::drawMenuButton(r, mapViewX, by, btnW2, btnH2, "Map View", mx, my, false);
    UIPrim::drawText(r, "Preview the scenario map and inspect countries", std::max(10, (int)(btnH2 * 0.22f)),
                     mapViewX + btnW2 * 0.5f, by - std::round(u * 0.55f), "center",
                     mapViewHov ? Theme::cream : Theme::grey);

    UIPrim::drawOrnamentalFrame(r, winX, winY, winW, winH, 2);
}


#if 0

    float content_y = base_y + Theme::s(38);


    const char* majorCultures[] = {"French","American","British","German","Japanese","Russian","Chinese","Indian"};

    std::vector<std::string> activeMajors;
    for (int i = 0; i < 8; i++) {
        std::string cname = getCountryType(majorCultures[i]);
        if (!cname.empty()) {
            for (auto& cn : countryNames_) {
                if (cn == cname) { activeMajors.push_back(majorCultures[i]); break; }
            }
        }
    }
    int numMajors = std::max(1, static_cast<int>(activeMajors.size()));
    float card_w = ui * 5.5f, card_h = ui * 11;
    float card_gap = Theme::s(6);
    float cards_total_w = numMajors * card_w + (numMajors - 1) * card_gap;
    float cards_x = base_x + (total_w - cards_total_w) / 2.0f;

    UIPrim::drawText(eng.renderer, "MAJOR POWERS", Theme::si(11),
                     base_x + total_w / 2, content_y + Theme::s(4), "center", Theme::gold_dim);
    float card_y = content_y + Theme::s(18);

    for (int i = 0; i < static_cast<int>(activeMajors.size()); i++) {
        float cx = cards_x + i * (card_w + card_gap);
        std::string cname = getCountryType(activeMajors[i].c_str());
        bool isHov = mx >= cx && mx <= cx + card_w && my >= card_y && my <= card_y + card_h;
        bool isSel = !cname.empty() && selectedCountry_ == cname;

        Color bgC = isSel ? Theme::slot_selected : (isHov ? Theme::slot_hover : Theme::slot);
        Color brdC = isSel ? Theme::gold : (isHov ? Theme::border_light : Theme::border);
        UIPrim::drawRect(eng.renderer, bgC, cx, card_y, card_w, card_h, brdC);


        if (!cname.empty()) {
            std::string flagPath = eng.assetsPath + "flags/" + cname + "_flag.png";

            std::string lowerName = cname;
            std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
            flagPath = eng.assetsPath + "flags/" + lowerName + "_flag.png";

            SDL_Texture* flag = eng.loadTexture(flagPath);
            if (flag) {
                int fw, fh;
                SDL_QueryTexture(flag, nullptr, nullptr, &fw, &fh);
                int drawW = std::min(static_cast<int>(card_w - Theme::s(8)), fw);
                int drawH = static_cast<int>(static_cast<float>(fh) * drawW / fw);
                SDL_Rect dst = {static_cast<int>(cx + (card_w - drawW) / 2),
                                static_cast<int>(card_y + card_h - drawH - ui * 2),
                                drawW, drawH};
                SDL_RenderCopy(eng.renderer, flag, nullptr, &dst);
            }


            UIPrim::drawText(eng.renderer, replaceAll(cname, "_", " "), Theme::si(9),
                             cx + card_w / 2, card_y + card_h - ui * 0.8f, "center",
                             isSel ? Theme::gold : Theme::cream);
        }
    }


    float minor_y = card_y + card_h + Theme::s(12);
    float minor_area_w = total_w * 0.58f;
    float minor_area_h = ui * 5;
    float minor_x = base_x + pad;

    UIPrim::drawRect(eng.renderer, Theme::bar, minor_x, minor_y, minor_area_w, minor_area_h, Theme::border);
    UIPrim::drawText(eng.renderer, "OTHER NATIONS", Theme::si(9),
                     minor_x + minor_area_w / 2, minor_y + Theme::s(6), "center", Theme::gold_dim);

    const char* minorOrder[] = {"Canadian","Australian","Irish","Spanish","Dutch","Belgian","Italian",
        "Swedish","Finnish","Polish","Ukrainian","Hungarian","Romanian","Turkish","Mexican","Brazilian",
        "Argentinian","Egyptian","Ethiopian","Nigerian","South_African","Israeli","Saudi","Iraqi","Iranian",
        "Pakistani","Korean","Vietnamese","Thai","Indonesian"};

    float fx = minor_x + Theme::s(8), fy = minor_y + Theme::s(18);
    for (int i = 0; i < 30; i++) {
        std::string cname = getCountryType(minorOrder[i]);
        if (cname.empty()) continue;

        std::string lowerName = cname;
        std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
        SDL_Texture* flag = eng.loadTexture(eng.assetsPath + "flags/" + lowerName + "_flag.png");
        if (!flag) continue;

        int origW, origH;
        SDL_QueryTexture(flag, nullptr, nullptr, &origW, &origH);
        int drawH = static_cast<int>(ui * 1.5f);
        int drawW = static_cast<int>(static_cast<float>(origW) / origH * drawH);

        if (fx + drawW > minor_x + minor_area_w - Theme::s(8)) {
            fx = minor_x + Theme::s(8);
            fy += drawH + Theme::s(6);
        }
        if (fy + drawH > minor_y + minor_area_h - Theme::s(4)) break;

        bool flagHov = mx >= fx - 2 && mx <= fx + drawW + 2 && my >= fy - 2 && my <= fy + drawH + 2;
        if (flagHov) {
            UIPrim::drawRect(eng.renderer, Theme::slot_hover, fx - 3, fy - 3,
                             drawW + 6.0f, drawH + 6.0f, Theme::border_light);
            if (pressed1) selectedCountry_ = cname;
        } else if (selectedCountry_ == cname) {
            UIPrim::drawRect(eng.renderer, Theme::slot_selected, fx - 3, fy - 3,
                             drawW + 6.0f, drawH + 6.0f, Theme::gold);
        }

        SDL_Rect dst = {static_cast<int>(fx), static_cast<int>(fy), drawW, drawH};
        SDL_RenderCopy(eng.renderer, flag, nullptr, &dst);
        fx += drawW + Theme::s(8);
    }


    float info_x = minor_x + minor_area_w + pad;
    float info_w = total_w - minor_area_w - pad * 3;
    float info_y = minor_y, info_h = minor_area_h;

    UIPrim::drawRect(eng.renderer, Theme::bar, info_x, info_y, info_w, info_h, Theme::border);

    if (!selectedCountry_.empty()) {
        float iy = info_y + Theme::s(8);
        int fs = Theme::si(10), row = Theme::si(16);

        UIPrim::drawText(eng.renderer, replaceAll(selectedCountry_, "_", " "),
                         Theme::si(12), info_x + info_w / 2, iy, "center", Theme::gold_bright);
        iy += row + Theme::s(4);

        auto* rec = CountryData::instance().getRecord(selectedCountry_);
        if (rec) {
            UIPrim::drawText(eng.renderer, "Ideology: " + rec->ideology, fs,
                             info_x + Theme::s(8), iy, "midleft", Theme::cream);
            iy += row;
            UIPrim::drawText(eng.renderer, "Culture: " + rec->culture, fs,
                             info_x + Theme::s(8), iy, "midleft", Theme::cream);
            iy += row;
            UIPrim::drawText(eng.renderer, "Regions: " + std::to_string(rec->claims.size()), fs,
                             info_x + Theme::s(8), iy, "midleft", Theme::cream);
            iy += row;
            UIPrim::drawText(eng.renderer, "Stability: " + std::to_string(rec->baseStability), fs,
                             info_x + Theme::s(8), iy, "midleft", Theme::cream);
        }
    } else {
        UIPrim::drawText(eng.renderer, "Select a country", Theme::si(11),
                         info_x + info_w / 2, info_y + info_h / 2, "center", Theme::dark_grey);
    }


    float nav_y = minor_y + minor_area_h + Theme::s(12);
    float nav_btn_w = ui * 6, nav_btn_h = ui * 2;
    float nav_gap = Theme::s(8);
    float nav_total = 4 * nav_btn_w + 3 * nav_gap;
    float nav_x = base_x + (total_w - nav_total) / 2.0f;

    const char* navLabels[] = {"Back", "Maps", "More Countries", "Start Game"};
    for (int i = 0; i < 4; i++) {
        float nx = nav_x + i * (nav_btn_w + nav_gap);
        bool isHov = mx >= nx && mx <= nx + nav_btn_w && my >= nav_y && my <= nav_y + nav_btn_h;

        if (i == 3 && !selectedCountry_.empty() && !isHov) {

            UIPrim::drawRect(eng.renderer, Theme::btn_confirm, nx, nav_y, nav_btn_w, nav_btn_h,
                             Theme::green);
            UIPrim::drawText(eng.renderer, navLabels[i], static_cast<int>(nav_btn_h * 0.45f),
                             nx + nav_btn_w / 2, nav_y + nav_btn_h / 2, "center", Theme::cream);
        } else {
            UIPrim::drawMenuButton(eng.renderer, nx, nav_y, nav_btn_w, nav_btn_h,
                                    navLabels[i], mx, my, isHov && pressed1);
        }
    }
}

#endif


void MainMenuScreen::renderSavesMenu(App& app) {
    auto& eng = Engine::instance();
    auto* r = eng.renderer;
    int W = eng.WIDTH, H = eng.HEIGHT;
    float u = H / 100.0f * eng.uiScaleFactor();
    int mx, my; SDL_GetMouseState(&mx, &my);
    auto& assets = UIAssets::instance();


    UIPrim::drawMapDim(r, W, H, 190);
    UIPrim::drawVignette(r, W, H, 180);


    float winW = std::round(W * 0.50f);
    winW = std::clamp(winW, 600.0f, 1300.0f);
    float winH = std::round(H * 0.65f);
    float winX = std::round((W - winW) / 2.0f);
    float winY = std::round((H - winH) / 2.0f);
    float pad = std::round(u * 1.8f);
    float headerH = std::round(u * 5.0f);


    for (int s = 20; s >= 1; s--) {
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(r, 0, 0, 0, (uint8_t)std::min(255, (int)(80.0f * s / 20.0f * 0.4f)));
        SDL_Rect sh = {(int)winX - s/2, (int)winY + s, (int)winW + s, (int)winH};
        SDL_RenderFillRect(r, &sh);
    }
    UIPrim::drawRoundedRect(r, {18, 20, 26}, winX, winY, winW, winH, 8);
    SDL_Texture* windowTex = assets.panelBodyHeaded();
    if (!windowTex) windowTex = assets.panelBodyHeadless();
    if (windowTex) UIAssets::draw9Slice(r, windowTex, winX, winY, winW, winH, 28);


    SDL_Texture* headerTex = assets.panelHeader();
    if (headerTex) UIAssets::draw9Slice(r, headerTex, winX + 3, winY + 3, winW - 6, headerH, 14);
    int titleFs = std::max(16, (int)(headerH * 0.42f));
    UIPrim::drawText(r, "LOAD GAME", titleFs, winX + winW / 2, winY + headerH * 0.5f, "center", Theme::gold_bright, true);
    UIPrim::drawHLine(r, Theme::gold, winX + 8, winX + winW - 8, winY + headerH + 2, 2);


    float footerH = std::round(u * 6.5f);
    float contentY = winY + headerH + pad;
    float contentH = winH - headerH - pad - footerH;
    float wellX = winX + pad, wellW = winW - pad * 2;

    UIPrim::drawRoundedRect(r, {10, 12, 16}, wellX, contentY, wellW, contentH, 6);
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, Theme::border.r, Theme::border.g, Theme::border.b, 40);
    SDL_Rect wb = {(int)wellX, (int)contentY, (int)wellW, (int)contentH};
    SDL_RenderDrawRect(r, &wb);


    float row_h = std::round(u * 4.0f);
    if (savesList_.empty()) {
        SDL_Texture* infoIcon = assets.icon("Info");
        if (infoIcon) {
            float iSz = std::round(u * 3.0f);
            SDL_SetTextureColorMod(infoIcon, 60, 60, 70);
            SDL_Rect id = {(int)(wellX + (wellW - iSz) / 2), (int)(contentY + contentH * 0.35f - iSz / 2), (int)iSz, (int)iSz};
            SDL_RenderCopy(r, infoIcon, nullptr, &id);
            SDL_SetTextureColorMod(infoIcon, 255, 255, 255);
        }
        int emFs = std::max(14, (int)(u * 1.3f));
        UIPrim::drawText(r, "No saved games found", emFs, wellX + wellW / 2, contentY + contentH * 0.55f, "center", Theme::dark_grey);
    }

    SDL_Rect clipSaves = {(int)wellX, (int)contentY, (int)wellW, (int)contentH};
    SDL_RenderSetClipRect(r, &clipSaves);
    for (int i = 0; i < (int)savesList_.size(); i++) {
        float ry = contentY + 2 + i * row_h - scrollOffset_ * row_h;
        if (ry + row_h < contentY || ry > contentY + contentH) continue;

        bool isHov = mx >= wellX && mx <= wellX + wellW && my >= ry && my <= ry + row_h;
        if (isHov) UIPrim::drawRectFilled(r, {28, 32, 40}, wellX + 2, ry, wellW - 4, row_h);
        else if (i % 2) { SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND); SDL_SetRenderDrawColor(r, 255, 255, 255, 3); SDL_Rect a = {(int)wellX+2,(int)ry,(int)wellW-4,(int)row_h}; SDL_RenderFillRect(r, &a); }

        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(r, Theme::border.r, Theme::border.g, Theme::border.b, 20);
        SDL_RenderDrawLine(r, (int)wellX + 8, (int)(ry + row_h - 1), (int)(wellX + wellW - 8), (int)(ry + row_h - 1));

        int fs = std::max(14, (int)(row_h * 0.36f));
        Color tc = isHov ? Theme::cream : Color{185, 180, 170};
        UIPrim::drawText(r, savesList_[i], fs, wellX + std::round(u * 1.5f), ry + row_h / 2, "midleft", tc);
    }
    SDL_RenderSetClipRect(r, nullptr);


    float footerY = winY + winH - footerH;
    UIPrim::drawHLine(r, {Theme::gold_dim.r, Theme::gold_dim.g, Theme::gold_dim.b, 40}, winX + pad, winX + winW - pad, footerY, 1);
    float btnH2 = std::round(u * 4.8f);
    float btnW2 = std::round(winW * 0.30f);
    float bx = winX + (winW - btnW2) / 2;
    float by = footerY + std::round(u * 0.8f);
    UIPrim::drawMenuButton(r, bx, by, btnW2, btnH2, "Back", mx, my, false);


    if (mx >= bx && mx <= bx + btnW2 && my >= by && my <= by + btnH2 && SDL_GetMouseState(nullptr,nullptr) & SDL_BUTTON_LMASK) {
        menuState_ = MenuState::MAIN;
        scrollOffset_ = 0;
    }

    UIPrim::drawOrnamentalFrame(r, winX, winY, winW, winH, 2);
}

void MainMenuScreen::renderSettingsMenu(App& app) {
    if (auto* settings = dynamic_cast<SettingsScreen*>(app.screen(ScreenType::SETTINGS))) {
        settings->returnScreen = ScreenType::MAIN_MENU;
    }
    nextScreen = ScreenType::SETTINGS;
}
