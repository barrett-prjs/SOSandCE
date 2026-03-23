#include "screens/settings_screen.h"
#include "core/app.h"
#include "core/engine.h"
#include "core/audio.h"
#include "core/input.h"
#include "ui/primitives.h"
#include "ui/theme.h"
#include "ui/ui_assets.h"
#include <algorithm>
#include <iomanip>
#include <sstream>

namespace {

struct SettingsLayout {
    float winW, winH, winX, winY;
    float pad, headerH, tabsY, tabsH;
    float contentX, contentY, contentW, contentH;
    float footerY, footerH;
    float buttonW, buttonH;
    float uiUnit, innerPad, cardHeaderH;

    void compute(int W, int H) {
        uiUnit = (H / 100.0f) * Engine::instance().uiScaleFactor();
        pad = std::round(uiUnit * 1.2f);
        innerPad = std::round(uiUnit * 1.15f);

        winW = std::round(W * 0.52f);
        winW = std::clamp(winW, 760.0f, 1220.0f);
        winH = std::round(H * 0.72f);
        winH = std::clamp(winH, 620.0f, 980.0f);
        winX = std::round((W - winW) * 0.5f);
        winY = std::round((H - winH) * 0.5f);

        headerH = std::round(uiUnit * 4.8f);
        tabsY = winY + headerH + pad * 0.5f;
        tabsH = std::round(uiUnit * 4.2f);
        footerH = std::round(uiUnit * 6.0f);
        footerY = winY + winH - footerH - pad;
        cardHeaderH = std::round(uiUnit * 4.0f);

        contentX = winX + pad;
        contentY = tabsY + tabsH + pad;
        contentW = winW - pad * 2.0f;
        contentH = footerY - contentY - pad;

        buttonW = std::round(winW * 0.18f);
        buttonH = std::round(uiUnit * 4.0f);
    }
};

std::string percentText(float value) {
    std::ostringstream oss;
    oss << static_cast<int>(std::round(std::clamp(value, 0.0f, 1.0f) * 100.0f)) << "%";
    return oss.str();
}

std::string uiSizeText(float value) {
    std::ostringstream oss;
    oss << static_cast<int>(std::round((value / 24.0f) * 100.0f)) << "%";
    return oss.str();
}

std::string tabTitle(SettingsScreen::SettingsTab tab) {
    switch (tab) {
    case SettingsScreen::SettingsTab::INTERFACE: return "Interface";
    case SettingsScreen::SettingsTab::AUDIO: return "Audio";
    case SettingsScreen::SettingsTab::VIDEO: return "Video";
    case SettingsScreen::SettingsTab::GAMEPLAY: return "Gameplay";
    }
    return "Settings";
}

std::string tabSubtitle(SettingsScreen::SettingsTab tab) {
    switch (tab) {
    case SettingsScreen::SettingsTab::INTERFACE: return "Scale and place the core HUD, menus, and strategy panels.";
    case SettingsScreen::SettingsTab::AUDIO: return "Tune music and sound levels for the whole session.";
    case SettingsScreen::SettingsTab::VIDEO: return "Control framerate and how the game uses your display.";
    case SettingsScreen::SettingsTab::GAMEPLAY: return "Adjust map visibility and campaign behavior.";
    }
    return "";
}

void drawSettingsCard(SDL_Renderer* r, float x, float y, float w, float h,
                      float headerH, float innerPad,
                      const std::string& title, const std::string& subtitle) {
    UIPrim::drawRoundedRect(r, {19, 21, 27, 255}, x, y, w, h, 7, {Theme::gold_dim.r, Theme::gold_dim.g, Theme::gold_dim.b, 140});
    UIPrim::drawRoundedRect(r, {24, 27, 34, 255}, x + 1, y + 1, w - 2, headerH, 7, {0, 0, 0, 0});
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, 255, 255, 255, 5);
    SDL_Rect glow = {(int)x + 2, (int)y + 2, std::max(0, (int)w - 4), std::max(0, (int)(h * 0.16f))};
    SDL_RenderFillRect(r, &glow);
    UIPrim::drawText(r, title, std::max(15, Theme::si(11.6f)), x + innerPad, y + headerH * 0.36f,
                     "midleft", Theme::gold_bright, true);
    UIPrim::drawText(r, subtitle, std::max(11, Theme::si(8.7f)), x + innerPad, y + headerH * 0.74f,
                     "midleft", Theme::grey, false);
    UIPrim::drawHLine(r, {Theme::gold_dim.r, Theme::gold_dim.g, Theme::gold_dim.b, 140},
                      x + 1, x + w - 1, y + headerH, 1);
}

void drawSegmentOption(SDL_Renderer* r, float x, float y, float w, float h,
                       const std::string& label, bool active, bool hovered) {
    SDL_Texture* panelTex = UIAssets::instance().panelBodyHeadless();
    if (panelTex) {
        uint8_t mod = active ? 255 : (hovered ? 246 : 228);
        SDL_SetTextureColorMod(panelTex, mod, mod, mod);
        SDL_SetTextureAlphaMod(panelTex, active || hovered ? 255 : 240);
        UIAssets::draw9Slice(r, panelTex, x, y, w, h, 18);
        SDL_SetTextureColorMod(panelTex, 255, 255, 255);
        SDL_SetTextureAlphaMod(panelTex, 255);
    } else {
        Color bg = active ? Color{46, 50, 58, 255} : (hovered ? Color{33, 36, 43, 255} : Color{24, 26, 32, 255});
        Color brd = active ? Theme::gold : Theme::border;
        UIPrim::drawRoundedRect(r, bg, x, y, w, h, 6, brd);
    }
    if (active) {
        UIPrim::drawHLine(r, Theme::gold, x + 6, x + w - 6, y + h - 2, 2);
    }
    UIPrim::drawText(r, label, std::max(12, (int)(h * 0.34f)),
                     x + w * 0.5f, y + h * 0.5f, "center",
                     active ? Theme::gold_bright : (hovered ? Theme::cream : Theme::grey), active);
}

}

void SettingsScreen::enter(App& app) {
    auto& s = app.settings();
    tempUISize_ = s.uiSize;
    tempMusicVol_ = s.musicVolume;
    tempSoundVol_ = s.soundVolume;
    tempFPS_ = s.fps;
    tempFog_ = s.fogOfWar;
    tempSidebarLeft_ = s.sidebarLeft;
    tempFullscreen_ = s.fullscreen;
    activeTab_ = SettingsTab::INTERFACE;
}

void SettingsScreen::applySettings(App& app) {
    auto& eng = Engine::instance();
    auto& s = app.settings();

    s.uiSize = tempUISize_;
    s.musicVolume = tempMusicVol_;
    s.soundVolume = tempSoundVol_;
    s.fps = tempFPS_;
    s.fogOfWar = tempFog_;
    s.sidebarLeft = tempSidebarLeft_;
    s.fullscreen = tempFullscreen_;

    Audio::instance().setMusicVolume(s.musicVolume);
    Audio::instance().setSoundVolume(s.soundVolume);
    eng.FPS = s.fps;

    if (eng.window) {
        SDL_SetWindowResizable(eng.window, SDL_TRUE);
        if (s.fullscreen) {
            SDL_SetWindowFullscreen(eng.window, SDL_WINDOW_FULLSCREEN_DESKTOP);
            SDL_MaximizeWindow(eng.window);
            SDL_DisplayMode dm;
            SDL_GetCurrentDisplayMode(0, &dm);
            eng.WIDTH = dm.w;
            eng.HEIGHT = dm.h;
        } else {
            SDL_SetWindowFullscreen(eng.window, 0);
            SDL_SetWindowSize(eng.window, s.windowWidth, s.windowHeight);
            SDL_SetWindowPosition(eng.window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
            SDL_GetWindowSize(eng.window, &eng.WIDTH, &eng.HEIGHT);
        }
    }

    eng.recalcUI(s.uiSize);
    Theme::setScale(eng.uiScale);
    s.save(eng.basePath + "settings.json");
}

void SettingsScreen::handleInput(App& app, const InputState& input) {
    auto& eng = Engine::instance();
    SettingsLayout L;
    L.compute(eng.WIDTH, eng.HEIGHT);
    int mx = input.mouseX;
    int my = input.mouseY;

    const char* tabLabels[] = {"Interface", "Audio", "Video", "Gameplay"};
    const SettingsTab tabValues[] = {
        SettingsTab::INTERFACE, SettingsTab::AUDIO, SettingsTab::VIDEO, SettingsTab::GAMEPLAY
    };
    float tabGap = std::round(L.uiUnit * 0.5f);
    float tabW = (L.contentW - tabGap * 3.0f) / 4.0f;
    for (int i = 0; i < 4; ++i) {
        float tx = L.contentX + i * (tabW + tabGap);
        if (input.mouseLeftDown &&
            mx >= tx && mx <= tx + tabW &&
            my >= L.tabsY && my <= L.tabsY + L.tabsH) {
            activeTab_ = tabValues[i];
            Audio::instance().playSound("clickedSound");
        }
    }

    auto sliderHit = [&](float x, float y, float w, float h, float minV, float maxV, float& outValue) {
        float hitMargin = std::max(8.0f, h * 1.5f);
        if (input.mouseLeft &&
            mx >= x && mx <= x + w &&
            my >= y - hitMargin && my <= y + h + hitMargin) {
            float pct = std::clamp((mx - x) / std::max(1.0f, w), 0.0f, 1.0f);
            outValue = minV + pct * (maxV - minV);
        }
    };

    float rowX = L.contentX + L.innerPad;
    float rowW = L.contentW - L.innerPad * 2.0f;
    float cardY = L.contentY + L.cardHeaderH + L.innerPad;
    float rowH = std::round(L.uiUnit * 5.3f);
    float rowGap = std::round(L.uiUnit * 0.6f);
    float sliderX = rowX + rowW * 0.42f;
    float sliderW = rowW * 0.38f;
    float sliderH = std::round(L.uiUnit * 0.85f);
    float toggleW = std::round(L.uiUnit * 6.4f);
    float toggleH = std::round(L.uiUnit * 2.9f);

    if (activeTab_ == SettingsTab::INTERFACE) {
        sliderHit(sliderX, cardY + rowH * 0.58f, sliderW, sliderH, 16.0f, 42.0f, tempUISize_);
        if (input.mouseLeftDown) {
            float segY = cardY + rowH + rowGap + rowH * 0.2f;
            float segX = sliderX;
            float segGap = std::round(L.uiUnit * 0.4f);
            if (mx >= segX && mx <= segX + toggleW && my >= segY && my <= segY + toggleH) {
                tempSidebarLeft_ = true;
                Audio::instance().playSound("clickedSound");
            }
            if (mx >= segX + toggleW + segGap && mx <= segX + toggleW * 2 + segGap &&
                my >= segY && my <= segY + toggleH) {
                tempSidebarLeft_ = false;
                Audio::instance().playSound("clickedSound");
            }
        }
    } else if (activeTab_ == SettingsTab::AUDIO) {
        sliderHit(sliderX, cardY + rowH * 0.58f, sliderW, sliderH, 0.0f, 1.0f, tempMusicVol_);
        sliderHit(sliderX, cardY + (rowH + rowGap) + rowH * 0.58f, sliderW, sliderH, 0.0f, 1.0f, tempSoundVol_);
    } else if (activeTab_ == SettingsTab::VIDEO) {
        float fpsValue = static_cast<float>(tempFPS_);
        sliderHit(sliderX, cardY + rowH * 0.58f, sliderW, sliderH, 30.0f, 240.0f, fpsValue);
        tempFPS_ = std::clamp((int)std::round(fpsValue), 30, 240);
        if (input.mouseLeftDown) {
            float segY = cardY + rowH + rowGap + rowH * 0.2f;
            float segX = sliderX;
            float segGap = std::round(L.uiUnit * 0.4f);
            if (mx >= segX && mx <= segX + toggleW && my >= segY && my <= segY + toggleH) {
                tempFullscreen_ = true;
                Audio::instance().playSound("clickedSound");
            }
            if (mx >= segX + toggleW + segGap && mx <= segX + toggleW * 2 + segGap &&
                my >= segY && my <= segY + toggleH) {
                tempFullscreen_ = false;
                Audio::instance().playSound("clickedSound");
            }
        }
    } else if (activeTab_ == SettingsTab::GAMEPLAY) {
        if (input.mouseLeftDown) {
            float segY = cardY + rowH * 0.2f;
            float segX = sliderX;
            float segGap = std::round(L.uiUnit * 0.4f);
            if (mx >= segX && mx <= segX + toggleW && my >= segY && my <= segY + toggleH) {
                tempFog_ = true;
                Audio::instance().playSound("clickedSound");
            }
            if (mx >= segX + toggleW + segGap && mx <= segX + toggleW * 2 + segGap &&
                my >= segY && my <= segY + toggleH) {
                tempFog_ = false;
                Audio::instance().playSound("clickedSound");
            }
        }
    }

    float btnGap = std::round(L.uiUnit * 0.7f);
    float totalBtnW = L.buttonW * 2.0f + btnGap;
    float btnX = L.winX + (L.winW - totalBtnW) * 0.5f;
    float applyX = btnX;
    float backX = btnX + L.buttonW + btnGap;
    float btnY = L.footerY + (L.footerH - L.buttonH) * 0.5f;

    if (input.mouseLeftDown) {
        if (mx >= applyX && mx <= applyX + L.buttonW && my >= btnY && my <= btnY + L.buttonH) {
            applySettings(app);
            Audio::instance().playSound("clickedSound");
        }
        if (mx >= backX && mx <= backX + L.buttonW && my >= btnY && my <= btnY + L.buttonH) {
            nextScreen = returnScreen;
            Audio::instance().playSound("clickedSound");
        }
    }

    if (input.isKeyDown(SDL_SCANCODE_ESCAPE)) {
        nextScreen = returnScreen;
    }
}

void SettingsScreen::update(App& app, float dt) {}

void SettingsScreen::render(App& app) {
    auto& eng = Engine::instance();
    auto* r = eng.renderer;
    int W = eng.WIDTH;
    int H = eng.HEIGHT;
    int mx, my;
    SDL_GetMouseState(&mx, &my);

    SettingsLayout L;
    L.compute(W, H);

    eng.clear(Theme::bg);
    UIPrim::drawVignette(r, W, H, 95);
    UIPrim::drawMenuWindow(r, L.winX, L.winY, L.winW, L.winH, "Settings");

    const char* tabLabels[] = {"Interface", "Audio", "Video", "Gameplay"};
    const SettingsTab tabValues[] = {
        SettingsTab::INTERFACE, SettingsTab::AUDIO, SettingsTab::VIDEO, SettingsTab::GAMEPLAY
    };
    float tabGap = std::round(L.uiUnit * 0.5f);
    float tabW = (L.contentW - tabGap * 3.0f) / 4.0f;
    for (int i = 0; i < 4; ++i) {
        float tx = L.contentX + i * (tabW + tabGap);
        bool hovered = mx >= tx && mx <= tx + tabW && my >= L.tabsY && my <= L.tabsY + L.tabsH;
        drawSegmentOption(r, tx, L.tabsY, tabW, L.tabsH, tabLabels[i], activeTab_ == tabValues[i], hovered);
    }

    float cardX = L.contentX;
    float cardY = L.contentY;
    float cardW = L.contentW;
    float cardH = L.contentH;
    drawSettingsCard(r, cardX, cardY, cardW, cardH, L.cardHeaderH, L.innerPad,
                     tabTitle(activeTab_), tabSubtitle(activeTab_));

    float rowX = cardX + L.innerPad;
    float rowW = cardW - L.innerPad * 2.0f;
    float rowH = std::round(L.uiUnit * 5.3f);
    float rowGap = std::round(L.uiUnit * 0.7f);
    float sliderX = rowX + rowW * 0.42f;
    float sliderW = rowW * 0.38f;
    float sliderH = std::round(L.uiUnit * 0.85f);
    float valueX = rowX + rowW;
    int labelFs = std::max(14, Theme::si(11.0f));
    int valueFs = std::max(13, Theme::si(10.2f));
    int helpFs = std::max(11, Theme::si(8.8f));
    Color rowBg{22, 24, 30, 255};
    Color rowBorder{Theme::gold_dim.r, Theme::gold_dim.g, Theme::gold_dim.b, 96};

    auto drawSliderRow = [&](float y, const std::string& label, const std::string& help,
                             float value, float minV, float maxV, const std::string& displayValue) {
        UIPrim::drawRoundedRect(r, rowBg, rowX, y, rowW, rowH, 6, rowBorder);
        UIPrim::drawText(r, label, labelFs, rowX + L.innerPad * 0.55f, y + rowH * 0.33f, "midleft", Theme::cream, false);
        UIPrim::drawText(r, help, helpFs, rowX + L.innerPad * 0.55f, y + rowH * 0.72f, "midleft", Theme::grey, false);

        UIPrim::drawRoundedRect(r, Theme::slot, sliderX, y + rowH * 0.5f, sliderW, sliderH, 4, Theme::border);
        float pct = std::clamp((value - minV) / std::max(0.01f, maxV - minV), 0.0f, 1.0f);
        float fillW = std::max(sliderH, sliderW * pct);
        UIPrim::drawRoundedRect(r, {Theme::gold_dim.r, Theme::gold_dim.g, Theme::gold_dim.b, 220},
                                sliderX, y + rowH * 0.5f, fillW, sliderH, 4);
        float knobW = std::round(L.uiUnit * 1.0f);
        float knobH = std::round(L.uiUnit * 1.5f);
        float knobX = sliderX + sliderW * pct - knobW * 0.5f;
        float knobY = y + rowH * 0.5f - (knobH - sliderH) * 0.5f;
        UIPrim::drawRoundedRect(r, Theme::gold_bright, knobX, knobY, knobW, knobH, 4, Theme::gold);

        UIPrim::drawText(r, displayValue, valueFs, valueX, y + rowH * 0.5f, "midright", Theme::gold_bright, true);
    };

    auto drawToggleRow = [&](float y, const std::string& label, const std::string& help,
                             const std::string& leftLabel, bool leftActive,
                             const std::string& rightLabel, bool rightActive) {
        UIPrim::drawRoundedRect(r, rowBg, rowX, y, rowW, rowH, 6, rowBorder);
        UIPrim::drawText(r, label, labelFs, rowX + L.innerPad * 0.55f, y + rowH * 0.33f, "midleft", Theme::cream, false);
        UIPrim::drawText(r, help, helpFs, rowX + L.innerPad * 0.55f, y + rowH * 0.72f, "midleft", Theme::grey, false);

        float segY = y + rowH * 0.18f;
        float segW = std::round(L.uiUnit * 6.3f);
        float segH = std::round(L.uiUnit * 2.9f);
        float segGap = std::round(L.uiUnit * 0.4f);
        bool leftHover = mx >= sliderX && mx <= sliderX + segW && my >= segY && my <= segY + segH;
        bool rightHover = mx >= sliderX + segW + segGap && mx <= sliderX + segW * 2 + segGap &&
                          my >= segY && my <= segY + segH;
        drawSegmentOption(r, sliderX, segY, segW, segH, leftLabel, leftActive, leftHover);
        drawSegmentOption(r, sliderX + segW + segGap, segY, segW, segH, rightLabel, rightActive, rightHover);
    };

    float y = cardY + L.cardHeaderH + L.innerPad;
    if (activeTab_ == SettingsTab::INTERFACE) {
        drawSliderRow(y, "UI Scale", "Scales the HUD, menus, popups, and strategy panels.",
                      tempUISize_, 16.0f, 42.0f, uiSizeText(tempUISize_));
        y += rowH + rowGap;
        drawToggleRow(y, "Sidebar Side", "Choose which side the in-game sidebar opens from.",
                      "Left", tempSidebarLeft_, "Right", !tempSidebarLeft_);
    } else if (activeTab_ == SettingsTab::AUDIO) {
        drawSliderRow(y, "Music Volume", "Controls music playback volume.",
                      tempMusicVol_, 0.0f, 1.0f, percentText(tempMusicVol_));
        y += rowH + rowGap;
        drawSliderRow(y, "Sound Volume", "Controls UI, combat, and feedback sounds.",
                      tempSoundVol_, 0.0f, 1.0f, percentText(tempSoundVol_));
    } else if (activeTab_ == SettingsTab::VIDEO) {
        drawSliderRow(y, "FPS Limit", "Caps the frame rate for menus and gameplay.",
                      static_cast<float>(tempFPS_), 30.0f, 240.0f, std::to_string(tempFPS_));
        y += rowH + rowGap;
        drawToggleRow(y, "Display Mode", "Switch between fullscreen desktop and windowed mode.",
                      "Fullscreen", tempFullscreen_, "Windowed", !tempFullscreen_);
    } else if (activeTab_ == SettingsTab::GAMEPLAY) {
        drawToggleRow(y, "Fog of War", "Show or hide strategic vision limits on the map.",
                      "Enabled", tempFog_, "Disabled", !tempFog_);
        y += rowH + rowGap;
        UIPrim::drawRoundedRect(r, rowBg, rowX, y, rowW, rowH, 6, rowBorder);
        UIPrim::drawText(r, "Notes", labelFs, rowX + L.innerPad * 0.55f, y + rowH * 0.33f, "midleft", Theme::cream, false);
        UIPrim::drawText(r,
                         "Interface changes apply immediately after pressing Apply. Back closes settings without saving.",
                         helpFs, rowX + L.innerPad * 0.55f, y + rowH * 0.72f, "midleft", Theme::grey, false);
    }

    float btnGap = std::round(L.uiUnit * 0.7f);
    float totalBtnW = L.buttonW * 2.0f + btnGap;
    float btnX = L.winX + (L.winW - totalBtnW) * 0.5f;
    float btnY = L.footerY + (L.footerH - L.buttonH) * 0.5f;
    bool applyDown = mx >= btnX && mx <= btnX + L.buttonW && my >= btnY && my <= btnY + L.buttonH &&
                     (SDL_GetMouseState(nullptr, nullptr) & SDL_BUTTON_LMASK);
    bool backDown = mx >= btnX + L.buttonW + btnGap && mx <= btnX + totalBtnW &&
                    my >= btnY && my <= btnY + L.buttonH &&
                    (SDL_GetMouseState(nullptr, nullptr) & SDL_BUTTON_LMASK);

    UIPrim::drawMenuButton(r, btnX, btnY, L.buttonW, L.buttonH, "Apply", mx, my, applyDown);
    UIPrim::drawMenuButton(r, btnX + L.buttonW + btnGap, btnY, L.buttonW, L.buttonH, "Back", mx, my, backDown);
}
