#include "screens/settings_screen.h"
#include "core/app.h"
#include "core/engine.h"
#include "core/audio.h"
#include "core/input.h"
#include "ui/primitives.h"
#include "ui/anim.h"
#include "ui/theme.h"
#include "ui/hoi4.h"
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
    float rowH, rowGap;
    float sliderX, sliderW, sliderH;
    float toggleW, toggleH;
    float labelColW;
    int titleFs, bodyFs, capFs, valueFs;

    void compute(int W, int H, int rows = 2) {
        uiUnit = (H / 100.0f) * Engine::instance().uiScaleFactor();
        pad = std::round(uiUnit * 1.2f);
        innerPad = std::round(uiUnit * 1.15f);

        titleFs = Theme::fsTitle(uiUnit);
        bodyFs = Theme::fsBody(uiUnit);
        capFs = Theme::fsCaption(uiUnit);
        valueFs = bodyFs;

        float titleH = (float)UIPrim::textHeight(titleFs);
        float bodyH = (float)UIPrim::textHeight(bodyFs);
        float capH = (float)UIPrim::textHeight(capFs);

        headerH = titleH + pad;
        tabsH = bodyH + std::round(uiUnit * 1.4f);
        cardHeaderH = std::round(innerPad * 0.7f) + bodyH + std::round(uiUnit * 0.2f) + capH + std::round(innerPad * 0.7f);
        rowH = std::round(innerPad * 0.7f) + bodyH + std::round(uiUnit * 0.3f) + capH + std::round(innerPad * 0.7f);
        rowGap = std::round(uiUnit * 0.7f);
        footerH = std::round(uiUnit * 5.2f);
        buttonH = std::round(uiUnit * 3.6f);

        winW = std::round(W * 0.52f);
        winW = std::clamp(winW, 760.0f, 1220.0f);
        buttonW = std::round(winW * 0.18f);

        contentH = cardHeaderH + innerPad + rowH * rows + rowGap * (rows - 1) + innerPad;
        winH = std::round(headerH + pad * 0.5f + tabsH + pad + contentH + pad + footerH + pad * 0.5f);
        winX = std::round((W - winW) * 0.5f);
        winY = std::round((H - winH) * 0.5f);

        tabsY = winY + headerH + pad * 0.5f;
        contentX = winX + pad;
        contentY = tabsY + tabsH + pad;
        contentW = winW - pad * 2.0f;
        footerY = contentY + contentH + pad;

        labelColW = contentW * 0.44f;
        sliderX = contentX + innerPad + labelColW + std::round(uiUnit * 0.8f);
        sliderW = (contentX + contentW - innerPad) - sliderX - std::round(uiUnit * 5.0f);
        sliderH = std::round(uiUnit * 0.55f);

        const char* segLabels[] = {"Left", "Right", "Fullscreen", "Windowed", "Enabled", "Disabled"};
        float maxSegW = 0.0f;
        for (const char* lbl : segLabels) {
            maxSegW = std::max(maxSegW, (float)UIPrim::textWidth(lbl, capFs + 1));
        }
        toggleW = std::max(std::round(uiUnit * 6.4f), maxSegW + std::round(uiUnit * 1.8f));
        toggleH = std::round(uiUnit * 2.6f);
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

void drawSettingsCard(SDL_Renderer* r, const SettingsLayout& L, float x, float y, float w, float h,
                      const std::string& title, const std::string& subtitle) {
    HOI4::inset(r, x, y, w, h);
    float ty = y + std::round(L.innerPad * 0.7f);
    UIPrim::drawText(r, UIPrim::fitText(title, L.bodyFs, w - L.innerPad * 2.0f), L.bodyFs,
                     x + L.innerPad, ty, "topleft", Theme::gold_bright, true);
    ty += (float)UIPrim::textHeight(L.bodyFs) + std::round(L.uiUnit * 0.2f);
    UIPrim::drawText(r, UIPrim::fitText(subtitle, L.capFs, w - L.innerPad * 2.0f), L.capFs,
                     x + L.innerPad, ty, "topleft", Theme::grey, false);
    UIPrim::drawHLine(r, Theme::hairlineStrong(), x + 1, x + w - 1, y + L.cardHeaderH, 1);
}

void drawSegmentOption(SDL_Renderer* r, float x, float y, float w, float h,
                       const std::string& label, bool active, bool hovered, int fs) {
    HOI4::button(r, x, y, w, h, hovered, false, true);
    if (active) {
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(r, Theme::gold.r, Theme::gold.g, Theme::gold.b, 230);
        SDL_Rect rim = {(int)x, (int)y, (int)w, (int)h};
        SDL_RenderDrawRect(r, &rim);
    }
    UIPrim::drawText(r, UIPrim::fitText(label, fs, w - 10.0f), fs,
                     x + w * 0.5f, y + h * 0.5f, "center",
                     active ? Theme::gold_bright : (hovered ? Theme::cream : Theme::grey), active);
}

}

void SettingsScreen::enter(App& app) {
    UIAnim::trigger("screen:settings");
    auto& s = app.settings();
    tempUISize_ = s.uiSize;
    tempMusicVol_ = s.musicVolume;
    tempSoundVol_ = s.soundVolume;
    tempFPS_ = s.fps;
    tempFog_ = s.fogOfWar;
    tempSidebarLeft_ = s.sidebarLeft;
    tempFullscreen_ = s.fullscreen;
    tempOcean_ = s.oceanDetail;
    tempDayNight_ = s.dayNight;
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
    s.oceanDetail = tempOcean_;
    s.dayNight = tempDayNight_;

    Audio::instance().setMusicVolume(s.musicVolume);
    Audio::instance().setSoundVolume(s.soundVolume);
    eng.FPS = s.fps;
    eng.oceanDetail = s.oceanDetail;
    eng.dayNight = s.dayNight;

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
    L.compute(eng.WIDTH, eng.HEIGHT, activeTab_ == SettingsTab::VIDEO ? 4 : 2);
    int mx = input.mouseX;
    int my = input.mouseY;

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
        float hitMargin = std::max(8.0f, h * 2.0f);
        if (input.mouseLeft &&
            mx >= x && mx <= x + w &&
            my >= y - hitMargin && my <= y + h + hitMargin) {
            float pct = std::clamp((mx - x) / std::max(1.0f, w), 0.0f, 1.0f);
            outValue = minV + pct * (maxV - minV);
        }
    };

    float row0Y = L.contentY + L.cardHeaderH + L.innerPad;
    float row1Y = row0Y + L.rowH + L.rowGap;
    auto sliderTrackY = [&](float rowY) { return rowY + L.rowH * 0.5f - L.sliderH * 0.5f; };
    auto toggleTopY = [&](float rowY) { return rowY + (L.rowH - L.toggleH) * 0.5f; };
    float segGap = std::round(L.uiUnit * 0.4f);

    auto toggleHit = [&](float rowY, bool& valueOut, bool leftMeansTrue) {
        if (!input.mouseLeftDown) return;
        float segY = toggleTopY(rowY);
        if (mx >= L.sliderX && mx <= L.sliderX + L.toggleW && my >= segY && my <= segY + L.toggleH) {
            valueOut = leftMeansTrue;
            Audio::instance().playSound("clickedSound");
        }
        if (mx >= L.sliderX + L.toggleW + segGap && mx <= L.sliderX + L.toggleW * 2 + segGap &&
            my >= segY && my <= segY + L.toggleH) {
            valueOut = !leftMeansTrue;
            Audio::instance().playSound("clickedSound");
        }
    };

    if (activeTab_ == SettingsTab::INTERFACE) {
        sliderHit(L.sliderX, sliderTrackY(row0Y), L.sliderW, L.sliderH, 16.0f, 42.0f, tempUISize_);
        toggleHit(row1Y, tempSidebarLeft_, true);
    } else if (activeTab_ == SettingsTab::AUDIO) {
        sliderHit(L.sliderX, sliderTrackY(row0Y), L.sliderW, L.sliderH, 0.0f, 1.0f, tempMusicVol_);
        sliderHit(L.sliderX, sliderTrackY(row1Y), L.sliderW, L.sliderH, 0.0f, 1.0f, tempSoundVol_);
    } else if (activeTab_ == SettingsTab::VIDEO) {
        float fpsValue = static_cast<float>(tempFPS_);
        sliderHit(L.sliderX, sliderTrackY(row0Y), L.sliderW, L.sliderH, 30.0f, 240.0f, fpsValue);
        tempFPS_ = std::clamp((int)std::round(fpsValue), 30, 240);
        toggleHit(row1Y, tempFullscreen_, true);
        float row2Y = row1Y + L.rowH + L.rowGap;
        float row3Y = row2Y + L.rowH + L.rowGap;
        toggleHit(row2Y, tempOcean_, true);
        toggleHit(row3Y, tempDayNight_, true);
    } else if (activeTab_ == SettingsTab::GAMEPLAY) {
        toggleHit(row0Y, tempFog_, true);
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
    L.compute(W, H, activeTab_ == SettingsTab::VIDEO ? 4 : 2);

    eng.clear(Theme::bg);
    UIPrim::drawVignette(r, W, H, 95);
    UIAnim::pushMenuTransition(r, "screen:settings", L.winX + L.winW / 2, L.winY + L.winH / 2);
    UIPrim::drawMenuWindow(r, L.winX, L.winY, L.winW, L.winH, "");

    UIPrim::drawGradientV(r, L.winX + 2, L.winY + 2, L.winW - 4, L.headerH - 2,
                          {54, 58, 52, 255}, {34, 37, 33, 255});
    UIPrim::drawText(r, HOI4::tracked("Settings"), L.titleFs, L.winX + L.winW * 0.5f, L.winY + L.headerH * 0.5f,
                     "center", Theme::gold_bright, true);
    HOI4::goldDivider(r, L.winX + 6, L.winX + L.winW - 6, L.winY + L.headerH);

    const char* tabLabels[] = {"Interface", "Audio", "Video", "Gameplay"};
    const SettingsTab tabValues[] = {
        SettingsTab::INTERFACE, SettingsTab::AUDIO, SettingsTab::VIDEO, SettingsTab::GAMEPLAY
    };
    float tabGap = std::round(L.uiUnit * 0.5f);
    float tabW = (L.contentW - tabGap * 3.0f) / 4.0f;
    for (int i = 0; i < 4; ++i) {
        float tx = L.contentX + i * (tabW + tabGap);
        bool hovered = mx >= tx && mx <= tx + tabW && my >= L.tabsY && my <= L.tabsY + L.tabsH;
        drawSegmentOption(r, tx, L.tabsY, tabW, L.tabsH, tabLabels[i],
                          activeTab_ == tabValues[i], hovered, L.bodyFs);
    }

    drawSettingsCard(r, L, L.contentX, L.contentY, L.contentW, L.contentH,
                     tabTitle(activeTab_), tabSubtitle(activeTab_));

    float rowX = L.contentX + L.innerPad;
    float rowW = L.contentW - L.innerPad * 2.0f;
    float helpMaxW = L.labelColW - L.innerPad * 0.5f;
    float valueX = L.contentX + L.contentW - L.innerPad;
    float segGap = std::round(L.uiUnit * 0.4f);
    float bodyH = (float)UIPrim::textHeight(L.bodyFs);

    auto drawRowText = [&](float y, const std::string& label, const std::string& help) {
        float ty = y + std::round(L.innerPad * 0.7f);
        UIPrim::drawText(r, UIPrim::fitText(label, L.bodyFs, helpMaxW), L.bodyFs,
                         rowX + L.innerPad * 0.55f, ty, "topleft", Theme::cream, false);
        ty += bodyH + std::round(L.uiUnit * 0.3f);
        UIPrim::drawText(r, UIPrim::fitText(help, L.capFs, helpMaxW), L.capFs,
                         rowX + L.innerPad * 0.55f, ty, "topleft", Theme::grey, false);
    };

    auto drawSliderRow = [&](float y, const std::string& label, const std::string& help,
                             float value, float minV, float maxV, const std::string& displayValue) {
        HOI4::plate(r, rowX, y, rowW, L.rowH);
        drawRowText(y, label, help);

        float trackY = y + L.rowH * 0.5f - L.sliderH * 0.5f;
        UIPrim::drawRoundedRect(r, Theme::bg_dark, L.sliderX, trackY, L.sliderW, L.sliderH, 3, Theme::hairline());
        float pct = std::clamp((value - minV) / std::max(0.01f, maxV - minV), 0.0f, 1.0f);
        float fillW = std::max(L.sliderH, L.sliderW * pct);
        UIPrim::drawRoundedRect(r, {Theme::gold_dim.r, Theme::gold_dim.g, Theme::gold_dim.b, 230},
                                L.sliderX, trackY, fillW, L.sliderH, 3);
        float knobSz = std::round(L.uiUnit * 1.4f);
        float knobX = L.sliderX + L.sliderW * pct - knobSz * 0.5f;
        float knobY = trackY + L.sliderH * 0.5f - knobSz * 0.5f;
        UIPrim::drawRoundedRect(r, Theme::gold_bright, knobX, knobY, knobSz, knobSz,
                                (int)(knobSz * 0.5f), Theme::gold);

        UIPrim::drawText(r, displayValue, L.valueFs, valueX, y + L.rowH * 0.5f, "midright", Theme::gold_bright, true);
    };

    auto drawToggleRow = [&](float y, const std::string& label, const std::string& help,
                             const std::string& leftLabel, bool leftActive,
                             const std::string& rightLabel, bool rightActive) {
        HOI4::plate(r, rowX, y, rowW, L.rowH);
        drawRowText(y, label, help);

        float segY = y + (L.rowH - L.toggleH) * 0.5f;
        bool leftHover = mx >= L.sliderX && mx <= L.sliderX + L.toggleW && my >= segY && my <= segY + L.toggleH;
        bool rightHover = mx >= L.sliderX + L.toggleW + segGap && mx <= L.sliderX + L.toggleW * 2 + segGap &&
                          my >= segY && my <= segY + L.toggleH;
        drawSegmentOption(r, L.sliderX, segY, L.toggleW, L.toggleH, leftLabel, leftActive, leftHover, L.capFs + 1);
        drawSegmentOption(r, L.sliderX + L.toggleW + segGap, segY, L.toggleW, L.toggleH,
                          rightLabel, rightActive, rightHover, L.capFs + 1);
    };

    float row0Y = L.contentY + L.cardHeaderH + L.innerPad;
    float row1Y = row0Y + L.rowH + L.rowGap;
    if (activeTab_ == SettingsTab::INTERFACE) {
        drawSliderRow(row0Y, "UI Scale", "Scales the HUD, menus, popups, and strategy panels.",
                      tempUISize_, 16.0f, 42.0f, uiSizeText(tempUISize_));
        drawToggleRow(row1Y, "Sidebar Side", "Choose which side the in-game sidebar opens from.",
                      "Left", tempSidebarLeft_, "Right", !tempSidebarLeft_);
    } else if (activeTab_ == SettingsTab::AUDIO) {
        drawSliderRow(row0Y, "Music Volume", "Controls music playback volume.",
                      tempMusicVol_, 0.0f, 1.0f, percentText(tempMusicVol_));
        drawSliderRow(row1Y, "Sound Volume", "Controls UI, combat, and feedback sounds.",
                      tempSoundVol_, 0.0f, 1.0f, percentText(tempSoundVol_));
    } else if (activeTab_ == SettingsTab::VIDEO) {
        drawSliderRow(row0Y, "FPS Limit", "Caps the frame rate for menus and gameplay.",
                      static_cast<float>(tempFPS_), 30.0f, 240.0f, std::to_string(tempFPS_));
        drawToggleRow(row1Y, "Display Mode", "Switch between fullscreen desktop and windowed mode.",
                      "Fullscreen", tempFullscreen_, "Windowed", !tempFullscreen_);
        float row2Y = row1Y + L.rowH + L.rowGap;
        float row3Y = row2Y + L.rowH + L.rowGap;
        drawToggleRow(row2Y, "Ocean Detail", "Depth shading and coastal shallows on the world map.",
                      "Enabled", tempOcean_, "Disabled", !tempOcean_);
        drawToggleRow(row3Y, "Day / Night", "Sunlight cycle that follows the game clock across the map.",
                      "Enabled", tempDayNight_, "Disabled", !tempDayNight_);
    } else if (activeTab_ == SettingsTab::GAMEPLAY) {
        drawToggleRow(row0Y, "Fog of War", "Show or hide strategic vision limits on the map.",
                      "Enabled", tempFog_, "Disabled", !tempFog_);
        HOI4::plate(r, rowX, row1Y, rowW, L.rowH);
        float noteMax = rowW - L.innerPad * 1.1f;
        float ty = row1Y + std::round(L.innerPad * 0.7f);
        UIPrim::drawText(r, "Notes", L.bodyFs, rowX + L.innerPad * 0.55f, ty, "topleft", Theme::cream, false);
        ty += bodyH + std::round(L.uiUnit * 0.3f);
        UIPrim::drawText(r,
                         UIPrim::fitText("Interface changes apply after pressing Apply. Back closes without saving.",
                                         L.capFs, noteMax),
                         L.capFs, rowX + L.innerPad * 0.55f, ty, "topleft", Theme::grey, false);
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
    UIAnim::popMenuTransition(r);
    UIAnim::drawTransitionFade(r, "screen:settings", W, H);
}
