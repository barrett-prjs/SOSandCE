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
#include "ui/anim.h"
#include "ui/ui_assets.h"
#include "ui/hoi4.h"
#include "map/map_functions.h"
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
    SDL_Texture* cachedTex = nullptr;
    float mapW = 0, mapH = 0;
    float texScale = 2.0f;
    float camx = 0, camy = 0, zoom = 2.6f;
    float driftSpeed = 2.4f;
    double phase = 0;
    Uint32 lastTicks = 0;
    bool initted = false;

    void init(SDL_Renderer* r, SDL_Surface* composite, float scaleUsed) {
        if (cachedTex) { SDL_DestroyTexture(cachedTex); cachedTex = nullptr; }
        initted = false;
        if (!composite) return;
        SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
        cachedTex = SDL_CreateTextureFromSurface(r, composite);
        if (!cachedTex) return;
        texScale = scaleUsed;
        mapW = composite->w / texScale;
        mapH = composite->h / texScale;
        camx = mapW * randFloat(0.2f, 0.8f);
        camy = mapH * 0.42f;
        phase = randFloat(0.0f, 6.28318f);
        lastTicks = SDL_GetTicks();
        initted = true;
    }

    void update(SDL_Renderer* r, int W, int H, int fps) {
        if (!initted || !cachedTex) return;
        Uint32 now = SDL_GetTicks();
        float dt = std::min(0.1f, (now - lastTicks) / 1000.0f);
        lastTicks = now;

        camx += driftSpeed * dt;
        while (camx >= mapW) camx -= mapW;
        phase += dt * 0.045;

        float visH = H / zoom;
        float minY = visH * 0.5f + mapH * 0.04f;
        float maxY = mapH * 0.96f - visH * 0.5f;
        if (maxY < minY) maxY = minY;
        float midY = (minY + maxY) * 0.5f;
        float amp = (maxY - minY) * 0.5f;
        camy = midY + std::sin(static_cast<float>(phase)) * amp;

        SDL_FRect dst;
        dst.w = mapW * zoom;
        dst.h = mapH * zoom;
        dst.x = W * 0.5f - camx * zoom;
        dst.y = H * 0.5f - camy * zoom;
        SDL_RenderCopyF(r, cachedTex, nullptr, &dst);
        SDL_FRect dstE = dst; dstE.x += dst.w;
        SDL_RenderCopyF(r, cachedTex, nullptr, &dstE);
        SDL_FRect dstW2 = dst; dstW2.x -= dst.w;
        SDL_RenderCopyF(r, cachedTex, nullptr, &dstW2);

        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(r, 4, 6, 8, 48);
        SDL_Rect dim = {0, 0, W, H};
        SDL_RenderFillRect(r, &dim);
    }

};

static SDL_Surface* buildMenuComposite(const std::string& mapPath) {
    auto& eng = Engine::instance();
    SDL_Surface* map = eng.loadSurface(mapPath);
    if (!map) return nullptr;

    std::string mapsDir = eng.assetsPath + "maps/";
    SDL_Surface* biome = eng.loadSurface(mapsDir + "biomeMap.png");
    if (!biome) biome = eng.loadSurface(mapsDir + "biomes.png");
    SDL_Surface* mask = eng.loadSurface(mapsDir + "regionsMap.png");
    if (!mask) mask = eng.loadSurface(mapsDir + "regions.png");
    SDL_Surface* grain = eng.loadSurface(mapsDir + "terrain_grain.png");

    SDL_Surface* composite = nullptr;
    if (biome) {
        SDL_Surface* terrain = MapFunc::buildTerrainSurface(biome, grain, 2);
        SDL_Surface* styled = MapFunc::stylizeForDisplay(map, mask, 2);
        if (terrain && styled) {
            SDL_SetSurfaceBlendMode(styled, SDL_BLENDMODE_BLEND);
            SDL_SetSurfaceAlphaMod(styled, 215);
            SDL_Rect full = {0, 0, terrain->w, terrain->h};
            SDL_BlitScaled(styled, nullptr, terrain, &full);
            composite = terrain;
            terrain = nullptr;
        }
        if (terrain) SDL_FreeSurface(terrain);
        if (styled) SDL_FreeSurface(styled);
    }
    if (!composite) composite = SDL_ConvertSurfaceFormat(map, SDL_PIXELFORMAT_RGBA32, 0);

    if (grain) SDL_FreeSurface(grain);
    if (mask) SDL_FreeSurface(mask);
    if (biome) SDL_FreeSurface(biome);
    SDL_FreeSurface(map);
    return composite;
}

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

static std::string formatStartDate(const std::string& raw) {
    std::string s = raw;
    s.erase(std::remove_if(s.begin(), s.end(),
        [](char c) { return c == '[' || c == ']' || c == '\'' || c == '"' || c == ' '; }), s.end());
    std::vector<int> parts;
    std::istringstream iss(s);
    std::string tok;
    while (std::getline(iss, tok, ',')) {
        try { parts.push_back(std::stoi(tok)); } catch (...) { return raw; }
    }
    if (parts.size() == 4 && parts[2] >= 1 && parts[2] <= 12) {
        return std::to_string(parts[1]) + " " + Helpers::getMonthName(parts[2]) + " " + std::to_string(parts[3]);
    }
    return raw;
}

namespace {

struct MainLayout {
    float panelX, panelY, panelW, totalH, headerH, versionH;
    float goldLineY;
    float wellX, wellY, wellW, wellH;
    float btnX, btnW, btnStartY, btnH, btnGap;
    float logoY, logoH;
};

MainLayout computeMainLayout(int W, int H) {
    auto& eng = Engine::instance();
    float u = H / 100.0f * eng.uiScaleFactor();
    MainLayout L{};
    L.logoH = H * 0.18f;
    L.logoY = u * 4;
    L.panelW = std::clamp(std::round(W * 0.20f), 300.0f, 440.0f);
    L.panelX = std::round((W - L.panelW) / 2.0f);
    float outerPad = std::round(u * 1.2f);
    float sectionGap = std::round(u * 1.0f);
    L.headerH = std::round(u * 4.2f);
    L.btnH = std::round(u * 3.8f);
    L.btnGap = std::round(u * 0.8f);
    float btnRegionPad = std::round(u * 1.4f);
    L.versionH = std::round(u * 2.4f);
    float btnGroupH = 4 * L.btnH + 3 * L.btnGap;
    float bodyH = btnRegionPad + btnGroupH + btnRegionPad;
    L.totalH = L.headerH + bodyH + L.versionH + sectionGap;
    L.panelY = std::round((H - L.totalH) / 2.0f);
    L.panelY = std::max(L.panelY, std::round(L.logoY + L.logoH + u * 1.5f));
    if (L.panelY + L.totalH > H - u * 6) L.panelY = H - u * 6 - L.totalH;
    L.goldLineY = L.panelY + L.headerH + 2;
    float versionY = L.panelY + L.totalH - L.versionH;
    L.wellX = L.panelX + outerPad;
    L.wellY = L.goldLineY + sectionGap;
    L.wellW = L.panelW - outerPad * 2;
    L.wellH = versionY - 8.0f - L.wellY;
    L.btnW = std::round(L.wellW - outerPad * 2);
    L.btnX = std::round(L.wellX + outerPad);
    L.btnStartY = std::round(L.wellY + (L.wellH - btnGroupH) / 2.0f);
    return L;
}

struct MapsLayout {
    float winX, winY, winW, winH, pad, headerH;
    float contentY, contentH;
    float listX, listW, listHdrH, listBodyY, listBodyH, rowH;
    float detailX, detailW;
    float btnW, btnH, btnY, backX, contX;
};

MapsLayout computeMapsLayout(int W, int H) {
    auto& eng = Engine::instance();
    float u = H / 100.0f * eng.uiScaleFactor();
    MapsLayout L{};
    L.winW = std::clamp(std::round(W * 0.60f), 700.0f, 1600.0f);
    L.winH = std::round(H * 0.72f);
    L.winX = std::round((W - L.winW) / 2.0f);
    L.winY = std::round((H - L.winH) / 2.0f);
    L.pad = std::round(u * 1.8f);
    L.headerH = std::round(u * 5.0f);
    L.contentY = L.winY + L.headerH + L.pad;
    L.contentH = L.winH - L.headerH - L.pad * 2 - u * 6.5f;
    L.listW = std::round(L.winW * 0.35f);
    L.listX = L.winX + L.pad;
    L.listHdrH = std::round(u * 3.0f);
    L.listBodyY = L.contentY + L.listHdrH + 2;
    L.listBodyH = L.contentH - L.listHdrH - 2;
    L.rowH = std::round(u * 3.8f);
    L.detailX = L.winX + L.pad + L.listW + L.pad;
    L.detailW = L.winW - L.listW - L.pad * 3;
    float footerY = L.winY + L.winH - std::round(u * 6.0f);
    L.btnH = std::round(u * 4.8f);
    L.btnW = std::round(L.winW * 0.22f);
    float btnGap = std::round(u * 1.5f);
    float totalBtnW = L.btnW * 2 + btnGap;
    L.backX = L.winX + (L.winW - totalBtnW) / 2;
    L.contX = L.backX + L.btnW + btnGap;
    L.btnY = footerY + std::round(u * 0.8f);
    return L;
}

struct CountriesLayout {
    float winX, winY, winW, winH, pad, headerH, footerH;
    float contentY;
    float cardsX, cardY, cardW, cardH, cardGap;
    float lowerY, listX, listW, listH, rowH, flagColW;
    float detailX, detailW;
    float btnW, btnH, btnGap, btnY;
    float backX, mapsX, startX, mapViewX;
};

CountriesLayout computeCountriesLayout(int W, int H, int majorCount) {
    auto& eng = Engine::instance();
    float u = H / 100.0f * eng.uiScaleFactor();
    CountriesLayout L{};
    L.winW = std::clamp(std::round(W * 0.72f), 900.0f, 1900.0f);
    L.winH = std::round(H * 0.82f);
    L.winX = std::round((W - L.winW) / 2.0f);
    L.winY = std::round((H - L.winH) / 2.0f);
    L.pad = std::round(u * 1.8f);
    L.headerH = std::round(u * 5.0f);
    L.footerH = std::round(u * 6.5f);
    L.contentY = L.winY + L.headerH + L.pad;

    int nm = std::max(1, majorCount);
    L.cardGap = std::round(u * 0.8f);
    float cardAreaW = L.winW - L.pad * 2;
    L.cardW = std::min((cardAreaW - (nm - 1) * L.cardGap) / nm, std::round(u * 10.0f));
    L.cardH = std::round(u * 9.0f);
    float cardsTotal = nm * L.cardW + (nm - 1) * L.cardGap;
    L.cardsX = L.winX + (L.winW - cardsTotal) / 2;
    L.cardY = L.contentY + std::round(u * 2.0f);

    L.lowerY = L.cardY + L.cardH + std::round(u * 1.5f) + std::round(u * 1.8f);
    L.listW = std::round(L.winW * 0.42f);
    L.listH = L.winY + L.winH - L.lowerY - L.footerH;
    L.rowH = std::round(u * 3.2f);
    L.flagColW = std::round(L.rowH * 1.35f);
    L.listX = L.winX + L.pad;
    L.detailX = L.listX + L.listW + L.pad;
    L.detailW = L.winW - L.listW - L.pad * 3;

    float footerY = L.winY + L.winH - L.footerH;
    L.btnH = std::round(u * 4.5f);
    L.btnGap = std::round(u * 1.2f);
    L.btnW = std::clamp((L.winW - L.pad * 2.0f - L.btnGap * 3.0f) / 4.0f,
                        std::round(u * 8.0f), std::round(u * 13.5f));
    float totalBtnW = L.btnW * 4 + L.btnGap * 3;
    L.backX = L.winX + (L.winW - totalBtnW) / 2;
    L.mapsX = L.backX + L.btnW + L.btnGap;
    L.startX = L.mapsX + L.btnW + L.btnGap;
    L.mapViewX = L.startX + L.btnW + L.btnGap;
    L.btnY = footerY + std::round(u * 1.0f);
    return L;
}

}


static Rml::ElementDocument* s_menuDoc = nullptr;

void MainMenuScreen::enter(App& app) {
    menuState_ = MenuState::MAIN;
    hoveredButton_ = -1;
    animTimer_ = 0;
    scrollOffset_ = 0;
    selectedCountry_.clear();
    UIAnim::trigger("menu:page");

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
        SDL_Surface* comp = buildMenuComposite(eng.assetsPath + "starts/Modern Day/map.png");
        if (comp) {
            s_menuBG.init(eng.renderer, comp, comp->w >= 2000 ? 2.0f : 1.0f);
            SDL_FreeSurface(comp);
        }
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
            MainLayout L = computeMainLayout(W, H);
            for (int i = 0; i < 4; i++) {
                float by = std::round(L.btnStartY + i * (L.btnH + L.btnGap));
                if (mx >= L.btnX && mx <= L.btnX + L.btnW && my >= by && my <= by + L.btnH) {
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
            MapsLayout L = computeMapsLayout(W, H);
            float listY2 = L.listBodyY + 2;

            for (int m2 = 0; m2 < (int)mapList_.size(); m2++) {
                float ry = listY2 + m2 * L.rowH - scrollOffset_ * L.rowH;
                if (ry + L.rowH < L.listBodyY || ry > L.listBodyY + L.listBodyH) continue;
                if (mx >= L.listX && mx <= L.listX + L.listW && my >= ry && my <= ry + L.rowH) {
                    selectedMap_ = mapList_[m2];
                    Audio::instance().playSound("clickedSound");
                    std::string mapPath = Engine::instance().assetsPath + "starts/" + selectedMap_ + "/map.png";
                    SDL_Surface* comp = buildMenuComposite(mapPath);
                    if (comp) {
                        s_menuBG.init(Engine::instance().renderer, comp, comp->w >= 2000 ? 2.0f : 1.0f);
                        SDL_FreeSurface(comp);
                    }
                }
            }

            if (mx >= L.backX && mx <= L.backX + L.btnW && my >= L.btnY && my <= L.btnY + L.btnH) {
                Audio::instance().playSound("clickedSound");
                menuState_ = MenuState::MAIN; scrollOffset_ = 0;
            }

            if (mx >= L.contX && mx <= L.contX + L.btnW && my >= L.btnY && my <= L.btnY + L.btnH && !selectedMap_.empty()) {
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
            const char* mc[] = {"French","American","British","German","Japanese","Russian","Chinese","Indian"};
            std::vector<std::string> am;
            for (int i2 = 0; i2 < 8; i2++) {
                std::string cn2 = findMajorOnMap(mc[i2], countryNames_);
                if (!cn2.empty()) am.push_back(cn2);
            }
            CountriesLayout L = computeCountriesLayout(W, H, (int)am.size());

            for (int i2 = 0; i2 < (int)am.size(); i2++) {
                float cx2 = L.cardsX + i2 * (L.cardW + L.cardGap);
                if (mx >= cx2 && mx <= cx2 + L.cardW && my >= L.cardY && my <= L.cardY + L.cardH) {
                    selectedCountry_ = am[i2];
                    Audio::instance().playSound("clickedSound");
                }
            }

            float listY2 = L.lowerY + 2;
            for (int c = 0; c < (int)countryNames_.size(); c++) {
                float ry = listY2 + c * L.rowH - scrollOffset_ * L.rowH;
                if (ry + L.rowH < L.lowerY || ry > L.lowerY + L.listH) continue;
                if (mx >= L.listX && mx <= L.listX + L.listW && my >= ry && my <= ry + L.rowH) {
                    selectedCountry_ = countryNames_[c];
                    Audio::instance().playSound("clickedSound");
                }
            }

            auto footerHit = [&](float bx3) {
                return mx >= bx3 && mx <= bx3 + L.btnW && my >= L.btnY && my <= L.btnY + L.btnH;
            };
            if (footerHit(L.backX) || footerHit(L.mapsX)) {
                Audio::instance().playSound("clickedSound");
                menuState_ = MenuState::MAPS; scrollOffset_ = 0;
            }
            if (footerHit(L.startX) && !selectedCountry_.empty()) {
                Audio::instance().playSound("startGameSound");
                auto& gs = app.gameState();
                gs.controlledCountry = selectedCountry_;
                gs.mapViewOnly = false;
                gs.mapName = selectedMap_;
                gs.inGame = false;
                nextScreen = ScreenType::GAME;
            }
            if (footerHit(L.mapViewX)) {
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

    static MenuState lastAnimState = MenuState::SAVES;
    if (menuState_ != lastAnimState) {
        UIAnim::trigger("menu:page");
        lastAnimState = menuState_;
    }

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

    UIPrim::drawVignette(r, W, H, 110);

    UIPrim::drawGradientV(r, 0, 0, (float)W, H * 0.35f,
                          {0, 0, 0, 110}, {0, 0, 0, 0});

    UIPrim::drawGradientV(r, 0, H * 0.80f, (float)W, H * 0.20f,
                          {0, 0, 0, 0}, {0, 0, 0, 120});


    MainLayout L = computeMainLayout(W, H);

    if (!backgroundTex_) {
        backgroundTex_ = eng.loadTexture(eng.assetsPath + "img/logo.png");
    }
    if (backgroundTex_) {
        int lw, lh;
        SDL_QueryTexture(backgroundTex_, nullptr, nullptr, &lw, &lh);
        float logoW = static_cast<float>(lw) / lh * L.logoH;
        SDL_Rect dst = {(int)(W / 2.0f - logoW / 2), (int)L.logoY, (int)logoW, (int)L.logoH};
        SDL_RenderCopy(r, backgroundTex_, nullptr, &dst);
    }

    UIAnim::pushMenuTransition(r, "menu:page", L.panelX + L.panelW / 2, L.panelY + L.totalH / 2);

    float panelW = L.panelW;
    float panelX = L.panelX;
    float panelY = L.panelY;
    float totalH = L.totalH;
    float headerH = L.headerH;
    float versionH = L.versionH;
    float btnH = L.btnH;
    float btnGap = L.btnGap;


    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    for (int s = 24; s >= 1; s--) {
        int a = static_cast<int>(90.0f * s / 24.0f * 0.4f);
        SDL_SetRenderDrawColor(r, 0, 0, 0, (uint8_t)std::min(255, a));
        SDL_Rect sh = {(int)panelX - s/2, (int)panelY + s, (int)panelW + s, (int)totalH};
        SDL_RenderFillRect(r, &sh);
    }


    HOI4::panel(r, panelX, panelY, panelW, totalH);


    UIPrim::drawGradientV(r, panelX + 2, panelY + 2, panelW - 4, headerH,
                          {54, 58, 52, 255}, {34, 37, 33, 255});
    HOI4::goldDivider(r, panelX + 6, panelX + panelW - 6, panelY + headerH + 2);


    int headerFs = std::max(16, (int)(headerH * 0.38f));
    UIPrim::drawText(r, HOI4::tracked("Community Edition"), headerFs,
                     panelX + panelW / 2, panelY + headerH * 0.38f, "center", Theme::gold_bright, true);
    int subFs = std::max(12, (int)(headerH * 0.24f));
    UIPrim::drawText(r, "Version 2.0", subFs,
                     panelX + panelW / 2, panelY + headerH * 0.72f, "center", Theme::gold_dim, false);


    float wellX = L.wellX;
    float wellY = L.wellY;
    float wellW = L.wellW;
    float wellH = L.wellH;


    HOI4::inset(r, wellX, wellY, wellW, wellH);


    float btnW = L.btnW;
    float btnX = L.btnX;
    float btnStartY = L.btnStartY;

    struct MenuBtn { const char* label; const char* icon; };
    MenuBtn btns[] = {
        {"NEW GAME",   "Play"},
        {"LOAD GAME",  "Home"},
        {"SETTINGS",   "Gear"},
        {"QUIT",       "Exit"},
    };

    for (int i = 0; i < 4; i++) {
        float by = std::round(btnStartY + i * (btnH + btnGap));
        bool hov = mx >= btnX && mx <= btnX + btnW && my >= by && my <= by + btnH;
        bool isPress = hov && pressed;
        float lift = UIAnim::springOffset("main:btn" + std::to_string(i), hov);


        bool wasHov = (hoveredButton_ == i);
        if (hov && !wasHov) {
            hoveredButton_ = i;
            Audio::instance().playSound("hoveredSound");
        }
        if (!hov && wasHov && hoveredButton_ == i) {
            hoveredButton_ = -1;
        }

        float drawY = (isPress ? by + 2 : by) + lift;


        HOI4::button(r, btnX, drawY, btnW, btnH, hov, isPress, true);

        int fs = std::max(13, (int)(btnH * 0.40f));
        Color tc = isPress ? Theme::gold : (hov ? Theme::gold_bright : Theme::cream);
        UIPrim::drawText(r, btns[i].label, fs, btnX + btnW * 0.5f, drawY + btnH * 0.5f,
                         "center", tc, true);
    }


    float verY = panelY + totalH - versionH;

    UIPrim::drawRectFilled(r, {15, 17, 16}, panelX + 3, verY, panelW - 6, versionH - 3);

    UIPrim::drawHLine(r, {Theme::gold_dim.r, Theme::gold_dim.g, Theme::gold_dim.b, 60},
                      panelX + 16, panelX + panelW - 16, verY, 1);
    int verFs = std::max(11, (int)(versionH * 0.35f));
    UIPrim::drawText(r, "Spirits of Steel: A New Dawn", verFs,
                     panelX + panelW / 2, verY + versionH * 0.5f, "center",
                     {Theme::grey.r, Theme::grey.g, Theme::grey.b, 180}, false);


    UIAnim::popMenuTransition(r);
    UIAnim::drawTransitionFade(r, "menu:page", W, H);


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

    MapsLayout L = computeMapsLayout(W, H);
    float winW = L.winW;
    float winH = L.winH;
    float winX = L.winX;
    float winY = L.winY;
    float pad = L.pad;
    float headerH = L.headerH;

    UIAnim::pushMenuTransition(r, "menu:page", winX + winW / 2, winY + winH / 2);


    for (int s = 20; s >= 1; s--) {
        int a = (int)(80.0f * s / 20.0f * 0.4f);
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(r, 0, 0, 0, (uint8_t)std::min(255, a));
        SDL_Rect sh = {(int)winX - s/2, (int)winY + s, (int)winW + s, (int)winH};
        SDL_RenderFillRect(r, &sh);
    }


    HOI4::panel(r, winX, winY, winW, winH);


    UIPrim::drawGradientV(r, winX + 2, winY + 2, winW - 4, headerH, {54, 58, 52, 255}, {34, 37, 33, 255});
    int titleFs = std::max(16, (int)(headerH * 0.42f));
    UIPrim::drawText(r, HOI4::tracked("Select Scenario"), titleFs, winX + winW / 2, winY + headerH * 0.5f, "center", Theme::gold_bright, true);
    HOI4::goldDivider(r, winX + 6, winX + winW - 6, winY + headerH + 2);


    float contentY = L.contentY;
    float contentH = L.contentH;
    float listW = L.listW;
    float detailW = L.detailW;
    float detailX = L.detailX;


    float listX = L.listX;
    HOI4::inset(r, listX, contentY, listW, contentH);


    float listHdrH = L.listHdrH;
    UIPrim::drawGradientV(r, listX + 1, contentY + 1, listW - 2, listHdrH, {44, 48, 43, 255}, {28, 31, 28, 255});
    int listHdrFs = std::max(12, (int)(listHdrH * 0.42f));
    UIPrim::drawText(r, HOI4::tracked("Available Maps"), listHdrFs, listX + listW / 2, contentY + listHdrH * 0.5f, "center", Theme::gold, false);
    HOI4::goldDivider(r, listX + 6, listX + listW - 6, contentY + listHdrH);


    float row_h = L.rowH;
    float listBodyY = L.listBodyY;
    float listBodyH = L.listBodyH;


    SDL_Rect clipList = {(int)listX, (int)listBodyY, (int)listW, (int)listBodyH};
    SDL_RenderSetClipRect(r, &clipList);

    float listY = listBodyY + 2;
    for (int m = 0; m < (int)mapList_.size(); m++) {
        float ry = listY + m * row_h - scrollOffset_ * row_h;
        if (ry + row_h < listBodyY || ry > listBodyY + listBodyH) continue;

        bool isSel = (mapList_[m] == selectedMap_);
        bool isHov = mx >= listX && mx <= listX + listW && my >= ry && my <= ry + row_h;


        if (isSel) {
            UIPrim::drawRectFilled(r, {52, 56, 50}, listX + 2, ry, listW - 4, row_h);
        } else if (isHov) {
            UIPrim::drawRectFilled(r, {38, 42, 39}, listX + 2, ry, listW - 4, row_h);
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
        UIPrim::drawText(r, UIPrim::fitText(mapList_[m], fs, listW - 32.0f), fs,
                         listX + 16, ry + row_h / 2, "midleft", tc, isSel);
    }
    SDL_RenderSetClipRect(r, nullptr);


    HOI4::inset(r, detailX, contentY, detailW, contentH);


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
    std::string startDate = formatStartDate(readFile(startsDir + "startDate.txt"));
    tags.erase(std::remove_if(tags.begin(), tags.end(),
        [](char c) { return c == '[' || c == ']' || c == '\''; }), tags.end());

    float rowH = std::round(u * 2.8f);
    float infoW = detailW - prevPad * 2;
    UIPrim::drawInfoRow(r, detailX + prevPad, infoY, infoW, rowH, "Start Date", startDate.empty() ? "---" : startDate); infoY += rowH;
    UIPrim::drawInfoRow(r, detailX + prevPad, infoY, infoW, rowH, "Tags", tags.empty() ? "---" : tags); infoY += rowH;
    UIPrim::drawInfoRow(r, detailX + prevPad, infoY, infoW, rowH, "Creator", creator.empty() ? "---" : creator); infoY += rowH;


    float footerY = winY + winH - std::round(u * 6.0f);
    UIPrim::drawHLine(r, {Theme::gold_dim.r, Theme::gold_dim.g, Theme::gold_dim.b, 40},
                      winX + pad, winX + winW - pad, footerY, 1);

    UIPrim::drawMenuButton(r, L.backX, L.btnY, L.btnW, L.btnH, "Back", mx, my, false);
    UIPrim::drawMenuButton(r, L.contX, L.btnY, L.btnW, L.btnH, "Continue", mx, my, false);

    UIAnim::popMenuTransition(r);
    UIAnim::drawTransitionFade(r, "menu:page", W, H);
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


    const char* majorCultures[] = {"French","American","British","German","Japanese","Russian","Chinese","Indian"};
    std::vector<std::string> activeMajors;
    for (int i = 0; i < 8; i++) {
        std::string cname = findMajorOnMap(majorCultures[i], countryNames_);
        if (!cname.empty()) activeMajors.push_back(cname);
    }

    CountriesLayout L = computeCountriesLayout(W, H, (int)activeMajors.size());
    float winW = L.winW;
    float winH = L.winH;
    float winX = L.winX;
    float winY = L.winY;
    float pad = L.pad;
    float headerH = L.headerH;

    UIAnim::pushMenuTransition(r, "menu:page", winX + winW / 2, winY + winH / 2);


    for (int s = 20; s >= 1; s--) {
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(r, 0, 0, 0, (uint8_t)std::min(255, (int)(80.0f * s / 20.0f * 0.4f)));
        SDL_Rect sh = {(int)winX - s/2, (int)winY + s, (int)winW + s, (int)winH};
        SDL_RenderFillRect(r, &sh);
    }
    HOI4::panel(r, winX, winY, winW, winH);


    UIPrim::drawGradientV(r, winX + 2, winY + 2, winW - 4, headerH, {54, 58, 52, 255}, {34, 37, 33, 255});
    int titleFs = std::max(16, (int)(headerH * 0.42f));
    UIPrim::drawText(r, HOI4::tracked("Choose Your Nation"), titleFs, winX + winW / 2, winY + headerH * 0.5f, "center", Theme::gold_bright, true);
    HOI4::goldDivider(r, winX + 6, winX + winW - 6, winY + headerH + 2);


    float contentY = L.contentY;
    float footerH = L.footerH;


    int secFs = std::max(12, (int)(u * 1.1f));
    UIPrim::drawText(r, "MAJOR POWERS", secFs, winX + pad + 4, contentY, "topleft", Theme::gold_dim, false);
    float cardY = L.cardY;
    float cardGap = L.cardGap;
    float cardW = L.cardW;
    float cardH = L.cardH;
    float cardsX = L.cardsX;

    for (int i = 0; i < (int)activeMajors.size(); i++) {
        float cx = cardsX + i * (cardW + cardGap);
        bool isSel = (selectedCountry_ == activeMajors[i]);
        bool isHov = mx >= cx && mx <= cx + cardW && my >= L.cardY && my <= L.cardY + cardH;
        float cardY = L.cardY + UIAnim::springOffset("major:" + activeMajors[i], isHov);


        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(r, 0, 0, 0, 40);
        SDL_Rect csh = {(int)cx + 2, (int)cardY + 3, (int)cardW, (int)cardH};
        SDL_RenderFillRect(r, &csh);


        HOI4::button(r, cx, cardY, cardW, cardH, isHov, false, true);
        if (isSel) {
            SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(r, Theme::gold.r, Theme::gold.g, Theme::gold.b, 235);
            SDL_Rect selRim = {(int)cx, (int)cardY, (int)cardW, (int)cardH};
            SDL_RenderDrawRect(r, &selRim);
            SDL_Rect selRim2 = {(int)cx + 1, (int)cardY + 1, (int)cardW - 2, (int)cardH - 2};
            SDL_SetRenderDrawColor(r, Theme::gold_dim.r, Theme::gold_dim.g, Theme::gold_dim.b, 160);
            SDL_RenderDrawRect(r, &selRim2);
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
        UIPrim::drawText(r, UIPrim::fitText(dispName, nameFs, cardW - 8.0f), nameFs,
                         cx + cardW / 2, cardY + cardH - std::round(u * 1.2f), "center", nameC, isSel);
    }


    UIPrim::drawText(r, "ALL NATIONS", secFs, winX + pad + 4,
                     L.cardY + L.cardH + std::round(u * 1.5f), "topleft", Theme::gold_dim, false);
    float lowerY = L.lowerY;

    float listW = L.listW;
    float listH = L.listH;
    float row_h = L.rowH;
    float listX = L.listX;


    HOI4::inset(r, listX, lowerY, listW, listH);


    SDL_Rect clipList = {(int)listX, (int)lowerY, (int)listW, (int)listH};
    SDL_RenderSetClipRect(r, &clipList);

    float listY = lowerY + 2;
    for (int c = 0; c < (int)countryNames_.size(); c++) {
        float ry = listY + c * row_h - scrollOffset_ * row_h;
        if (ry + row_h < lowerY || ry > lowerY + listH) continue;

        bool isSel = (countryNames_[c] == selectedCountry_);
        bool isHov = mx >= listX && mx <= listX + listW && my >= ry && my <= ry + row_h;

        if (isSel) UIPrim::drawRectFilled(r, {52, 56, 50}, listX + 2, ry, listW - 4, row_h);
        else if (isHov) UIPrim::drawRectFilled(r, {38, 42, 39}, listX + 2, ry, listW - 4, row_h);
        else if (c % 2) { SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND); SDL_SetRenderDrawColor(r, 255, 255, 255, 3); SDL_Rect a = {(int)listX+2,(int)ry,(int)listW-4,(int)row_h}; SDL_RenderFillRect(r, &a); }

        if (isSel) { SDL_SetRenderDrawColor(r, Theme::gold.r, Theme::gold.g, Theme::gold.b, 255); SDL_Rect sb = {(int)listX+2,(int)ry,4,(int)row_h}; SDL_RenderFillRect(r, &sb); }


        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(r, Theme::border.r, Theme::border.g, Theme::border.b, 20);
        SDL_RenderDrawLine(r, (int)listX + 8, (int)(ry + row_h - 1), (int)(listX + listW - 8), (int)(ry + row_h - 1));


        std::string ln = countryNames_[c];
        std::transform(ln.begin(), ln.end(), ln.begin(), ::tolower);
        SDL_Texture* ft = eng.loadTexture(eng.assetsPath + "flags/" + ln + "_flag.png");
        float flagColW = L.flagColW;
        if (ft) {
            int fw, fh; SDL_QueryTexture(ft, nullptr, nullptr, &fw, &fh);
            float flagH2 = row_h * 0.60f;
            float flagW2 = flagH2 * fw / fh;
            if (flagW2 > flagColW - 4.0f) {
                flagW2 = flagColW - 4.0f;
                flagH2 = flagW2 * fh / fw;
            }
            SDL_Rect fd = {(int)(listX + 10), (int)(ry + (row_h - flagH2) / 2), (int)flagW2, (int)flagH2};
            SDL_RenderCopy(r, ft, nullptr, &fd);
        }

        int fs = std::max(12, (int)(row_h * 0.36f));
        std::string dn = replaceAll(countryNames_[c], "_", " ");
        Color tc = isSel ? Theme::gold_bright : (isHov ? Theme::cream : Color{185, 180, 170});
        float textX2 = listX + 10 + flagColW + 6;
        UIPrim::drawText(r, UIPrim::fitText(dn, fs, listX + listW - 14.0f - textX2), fs,
                         textX2, ry + row_h / 2, "midleft", tc, isSel);
    }
    SDL_RenderSetClipRect(r, nullptr);


    float detailX = L.detailX;
    float detailW = L.detailW;
    HOI4::inset(r, detailX, lowerY, detailW, listH);

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
    float btnH2 = L.btnH;
    float btnW2 = L.btnW;
    float by = L.btnY;

    UIPrim::drawMenuButton(r, L.backX, by, btnW2, btnH2, "Back", mx, my, false);
    UIPrim::drawMenuButton(r, L.mapsX, by, btnW2, btnH2, "Maps", mx, my, false);

    float startX = L.startX;
    if (!selectedCountry_.empty()) {
        bool startHov = mx >= startX && mx <= startX + btnW2 && my >= by && my <= by + btnH2;
        float startY = by + UIAnim::springOffset("countries:start", startHov);

        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(r, 60, 165, 80, startHov ? 50 : 25);
        SDL_Rect glow = {(int)startX - 3, (int)startY - 3, (int)btnW2 + 6, (int)btnH2 + 6};
        SDL_RenderFillRect(r, &glow);
        Color sTop = startHov ? UIAnim::brighten(Color{52, 106, 62}, 1.2f) : Color{52, 106, 62};
        Color sBot = startHov ? UIAnim::brighten(Color{32, 72, 42}, 1.2f) : Color{32, 72, 42};
        UIPrim::drawGradientV(r, startX, startY, btnW2, btnH2, sTop, sBot);
        HOI4::frame(r, startX, startY, btnW2, btnH2, startHov);
        Color stc = startHov ? Theme::gold_bright : Theme::cream;
        int sfs = std::max(14, (int)(btnH2 * 0.36f));
        UIPrim::drawText(r, UIPrim::fitText("START GAME", sfs, btnW2 - 12.0f), sfs,
                         startX + btnW2 / 2, startY + btnH2 / 2, "center", stc, true);
    } else {
        UIPrim::drawMenuButton(r, startX, by, btnW2, btnH2, "Start Game", mx, my, false);
    }

    float mapViewX = L.mapViewX;
    bool mapViewHov = mx >= mapViewX && mx <= mapViewX + btnW2 && my >= by && my <= by + btnH2;
    UIPrim::drawMenuButton(r, mapViewX, by, btnW2, btnH2, "Map View", mx, my, false);

    float hintMax = L.backX - (winX + pad) - std::round(u * 1.0f);
    if (hintMax > u * 6.0f) {
        std::string hint = mapViewHov
            ? "Map View: preview the scenario map and inspect countries"
            : (selectedCountry_.empty() ? "Select a nation to begin" : replaceAll(selectedCountry_, "_", " ") + " selected");
        int hintFs = std::max(10, (int)(btnH2 * 0.26f));
        UIPrim::drawText(r, UIPrim::fitText(hint, hintFs, hintMax), hintFs,
                         winX + pad, by + btnH2 / 2, "midleft",
                         mapViewHov ? Theme::cream : Theme::grey);
    }

    UIAnim::popMenuTransition(r);
    UIAnim::drawTransitionFade(r, "menu:page", W, H);
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
    HOI4::panel(r, winX, winY, winW, winH);


    UIPrim::drawGradientV(r, winX + 2, winY + 2, winW - 4, headerH, {54, 58, 52, 255}, {34, 37, 33, 255});
    int titleFs = std::max(16, (int)(headerH * 0.42f));
    UIPrim::drawText(r, HOI4::tracked("Load Game"), titleFs, winX + winW / 2, winY + headerH * 0.5f, "center", Theme::gold_bright, true);
    HOI4::goldDivider(r, winX + 6, winX + winW - 6, winY + headerH + 2);


    float footerH = std::round(u * 6.5f);
    float contentY = winY + headerH + pad;
    float contentH = winH - headerH - pad - footerH;
    float wellX = winX + pad, wellW = winW - pad * 2;

    HOI4::inset(r, wellX, contentY, wellW, contentH);


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
        if (isHov) UIPrim::drawRectFilled(r, {38, 42, 39}, wellX + 2, ry, wellW - 4, row_h);
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

}

void MainMenuScreen::renderSettingsMenu(App& app) {
    if (auto* settings = dynamic_cast<SettingsScreen*>(app.screen(ScreenType::SETTINGS))) {
        settings->returnScreen = ScreenType::MAIN_MENU;
    }
    nextScreen = ScreenType::SETTINGS;
}
