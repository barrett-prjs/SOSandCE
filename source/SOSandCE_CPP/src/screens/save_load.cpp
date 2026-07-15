#include "screens/save_load.h"
#include "screens/game_screen.h"
#include "core/app.h"
#include "core/engine.h"
#include "core/audio.h"
#include "core/input.h"
#include "ui/primitives.h"
#include "ui/anim.h"
#include "ui/hoi4.h"
#include "ui/theme.h"
#include "ui/toast.h"
#include "ui/ui_assets.h"
#include "save/save_system.h"

namespace {
struct SaveLoadLayout {
    float winW, winH, winX, winY;
    float pad, headerH, footerH;
    float contentX, contentY, contentW, contentH;
    float rowH, rowGap;
    float inputH, actionH;
    float backW, backH, backX, backY;

    void compute(int W, int H) {
        float u = H / 100.0f * Engine::instance().uiScaleFactor();
        winW = std::round(W * 0.50f);
        winW = std::clamp(winW, 700.0f, 1280.0f);
        winH = std::round(H * 0.74f);
        winH = std::clamp(winH, 560.0f, 920.0f);
        winX = std::round((W - winW) * 0.5f);
        winY = std::round((H - winH) * 0.5f);

        pad = std::round(u * 1.8f);
        headerH = std::round(u * 5.0f);
        footerH = std::round(u * 6.3f);

        contentX = winX + pad;
        contentY = winY + headerH + pad;
        contentW = winW - pad * 2.0f;
        contentH = winH - headerH - footerH - pad * 2.0f;

        rowH = std::round(u * 4.6f);
        rowGap = std::round(u * 0.8f);
        inputH = std::round(u * 4.2f);
        actionH = std::round(u * 4.6f);

        backW = std::round(winW * 0.28f);
        backH = std::round(u * 4.6f);
        backX = winX + (winW - backW) * 0.5f;
        backY = winY + winH - footerH + std::round(u * 0.8f);
    }
};
}


void SaveLoadScreen::enter(App& app) {
    UIAnim::trigger("screen:saveload");
    hoveredIndex_ = -1;
    newSaveName_.clear();
    enteringName_ = false;
    saves_.clear();
    printf("[SaveLoadScreen] enter mode=%s return=%d\n",
           mode == Mode::SAVE ? "SAVE" : "LOAD",
           static_cast<int>(returnScreen));
    fflush(stdout);

    SaveSystem saveSystem;
    for (const auto& info : saveSystem.listSaves()) {
        SaveEntry se;
        se.name = info.name;
        se.date = info.date;
        se.exists = info.valid;
        saves_.push_back(se);
    }

    auto isAuto = [](const SaveEntry& e) {
        return e.name.rfind("autosave_", 0) == 0;
    };
    std::sort(saves_.begin(), saves_.end(),
              [&](const SaveEntry& a, const SaveEntry& b) {
                  bool aa = isAuto(a), ab = isAuto(b);
                  if (aa != ab) return !aa;
                  return a.date > b.date;
              });
    manualCount_ = 0;
    for (auto& se : saves_) {
        if (!isAuto(se)) manualCount_++;
    }
}


void SaveLoadScreen::handleInput(App& app, const InputState& input) {
    auto& eng = Engine::instance();
    int W = eng.WIDTH;
    int H = eng.HEIGHT;
    int mx = input.mouseX;
    int my = input.mouseY;
    SaveLoadLayout L;
    L.compute(W, H);

    auto saveCurrentGame = [&]() {
        printf("[SaveLoadScreen] saveCurrentGame name='%s'\n", newSaveName_.c_str());
        fflush(stdout);
        if (newSaveName_.empty()) {
            toasts().show("Enter a save name first");
            return;
        }

        SaveSystem saver;
        auto* gameScreen = dynamic_cast<GameScreen*>(app.screen(ScreenType::GAME));
        if (gameScreen) {
            bool ok = saver.saveGame(app.gameState(), gameScreen->mapManager(), newSaveName_);
            if (ok) toasts().show("Game saved: " + newSaveName_);
            else toasts().show("Save failed!");
            printf("[SaveLoadScreen] save result=%d\n", ok ? 1 : 0);
            fflush(stdout);
        } else {
            toasts().show("Cannot save outside of game");
            printf("[SaveLoadScreen] save failed: game screen unavailable\n");
            fflush(stdout);
        }

        if (enteringName_) SDL_StopTextInput();
        enteringName_ = false;
        nextScreen = returnScreen;
    };


    if (mode == Mode::SAVE && enteringName_) {
        if (input.hasTextInput) {
            newSaveName_ += input.textInput;
            if (newSaveName_.size() > 32) newSaveName_.resize(32);
        }
        if (input.isKeyDown(SDL_SCANCODE_BACKSPACE) && !newSaveName_.empty()) {
            newSaveName_.pop_back();
        }
        if (input.isKeyDown(SDL_SCANCODE_RETURN) && !newSaveName_.empty()) {
            saveCurrentGame();
        }
    }
    float listX = L.contentX;
    float listW = L.contentW;
    float listY = L.contentY;
    float listBottom = L.contentY + L.contentH;

    if (input.mouseLeftDown) {

        if (mode == Mode::LOAD) {
            float startY = listY;
            float u2 = H / 100.0f * eng.uiScaleFactor();
            float secH = std::round(u2 * 2.8f) + std::round(u2 * 0.6f);
            for (int i = 0; i < static_cast<int>(saves_.size()); i++) {
                float by = startY + secH + i * (L.rowH + L.rowGap);
                if (i >= manualCount_) by += secH;
                if (by + L.rowH > listBottom) break;
                SDL_Point pt = {mx, my};


                float delBtnW = std::round(L.rowH * 1.4f);
                float delBtnH = std::round(L.rowH * 0.62f);
                float delBtnX = listX + listW - delBtnW - std::round(L.pad * 0.35f);
                float delBtnY = by + (L.rowH - delBtnH) * 0.5f;
                SDL_Rect delRect = {
                    static_cast<int>(delBtnX), static_cast<int>(delBtnY),
                    static_cast<int>(delBtnW), static_cast<int>(delBtnH)
                };
                if (SDL_PointInRect(&pt, &delRect)) {
                    Audio::instance().playSound("clickedSound");
                    toasts().show("Delete not implemented yet");
                    return;
                }

                SDL_Rect btnRect = {
                    static_cast<int>(listX), static_cast<int>(by),
                    static_cast<int>(listW), static_cast<int>(L.rowH)
                };
                if (SDL_PointInRect(&pt, &btnRect)) {
                    Audio::instance().playSound("clickedSound");
                    auto* gameScreen = dynamic_cast<GameScreen*>(app.screen(ScreenType::GAME));
                    if (gameScreen) {
                        bool ok = gameScreen->loadGame(app, saves_[i].name);
                        if (ok) {
                            nextScreen = ScreenType::GAME;
                        }
                    } else {
                        toasts().show("Game screen is unavailable");
                    }
                    return;
                }

            }
        }


        if (mode == Mode::SAVE) {
            float inputY = listY;
            float inputH = L.inputH;
            float saveY = inputY + inputH + L.rowGap;
            SDL_Rect inputRect = {
                static_cast<int>(listX), static_cast<int>(inputY),
                static_cast<int>(listW), static_cast<int>(inputH)
            };
            SDL_Rect saveRect = {
                static_cast<int>(listX), static_cast<int>(saveY),
                static_cast<int>(listW), static_cast<int>(L.actionH)
            };
            SDL_Point pt = {mx, my};
            if (SDL_PointInRect(&pt, &inputRect)) {
                Audio::instance().playSound("clickedSound");
                enteringName_ = true;
                SDL_StartTextInput();
                printf("[SaveLoadScreen] text input activated\n");
                fflush(stdout);
            }
            if (SDL_PointInRect(&pt, &saveRect) && !newSaveName_.empty()) {
                Audio::instance().playSound("clickedSound");
                saveCurrentGame();
                return;
            }
        }


        SDL_Rect backRect = {
            static_cast<int>(L.backX), static_cast<int>(L.backY),
            static_cast<int>(L.backW), static_cast<int>(L.backH)
        };
        SDL_Point pt = {mx, my};
        if (SDL_PointInRect(&pt, &backRect)) {
            Audio::instance().playSound("clickedSound");
            if (enteringName_) SDL_StopTextInput();
            nextScreen = returnScreen;
        }
    }


    if (input.isKeyDown(SDL_SCANCODE_ESCAPE)) {
        if (enteringName_) SDL_StopTextInput();
        nextScreen = returnScreen;
    }
}


void SaveLoadScreen::update(App& app, float dt) {

}


void SaveLoadScreen::render(App& app) {
    auto& eng = Engine::instance();
    int W = eng.WIDTH, H = eng.HEIGHT;
    float u = H / 100.0f * eng.uiScaleFactor();
    auto* r = eng.renderer;
    auto& assets = UIAssets::instance();
    int mx, my; SDL_GetMouseState(&mx, &my);

    eng.clear(Theme::bg);
    UIPrim::drawVignette(r, W, H, 80);

    SaveLoadLayout L;
    L.compute(W, H);
    UIAnim::pushMenuTransition(r, "screen:saveload", L.winX + L.winW / 2, L.winY + L.winH / 2);


    for (int s = 20; s >= 1; s--) {
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(r, 0, 0, 0, (uint8_t)std::min(255, (int)(80.0f * s / 20.0f * 0.4f)));
        SDL_Rect sh = {(int)L.winX - s/2, (int)L.winY + s, (int)L.winW + s, (int)L.winH};
        SDL_RenderFillRect(r, &sh);
    }
    HOI4::panel(r, L.winX, L.winY, L.winW, L.winH);

    std::string title = (mode == Mode::SAVE) ? "Save Game" : "Load Game";
    UIPrim::drawGradientV(r, L.winX + 2, L.winY + 2, L.winW - 4, L.headerH, {54, 58, 52, 255}, {34, 37, 33, 255});
    int titleFs = std::max(16, (int)(L.headerH * 0.42f));
    UIPrim::drawText(r, HOI4::tracked(title), titleFs, L.winX + L.winW / 2, L.winY + L.headerH * 0.5f, "center", Theme::gold_bright, true);
    HOI4::goldDivider(r, L.winX + 6, L.winX + L.winW - 6, L.winY + L.headerH + 2);

    HOI4::inset(r, L.contentX, L.contentY, L.contentW, L.contentH);

    auto& slAssets = UIAssets::instance();
    int bodyFs = std::max(13, (int)(u * 1.0f));
    int metaFs = std::max(11, (int)(u * 0.85f));
    int emptyFs = std::max(14, (int)(u * 1.15f));

    if (mode == Mode::SAVE) {

        float inputY = L.contentY;
        float inputH = L.inputH;

        HOI4::inset(r, L.contentX, inputY, L.contentW, inputH);

        std::string displayText = newSaveName_.empty() ? "Enter save name..." : newSaveName_ + "_";
        Color textColor = newSaveName_.empty() ? Theme::dark_grey : Theme::cream;
        UIPrim::drawText(r, displayText, bodyFs,
                         L.contentX + std::round(L.pad * 0.6f), inputY + inputH * 0.5f,
                         "midleft", textColor);


        float saveY = inputY + inputH + L.rowGap;
        bool canSave = !newSaveName_.empty();
        UIPrim::drawActionRow(r, L.contentX, saveY, L.contentW, L.actionH, "Save Game", mx, my, false, "",
                              canSave ? "Write the current campaign to disk" : "Enter a save name first", canSave);


        float listY = saveY + L.actionH + L.rowGap;
        UIPrim::drawSectionHeader(r, L.contentX, listY, L.contentW, std::round(u * 2.8f), "Existing Saves");
        listY += std::round(u * 3.4f);

        for (int i = 0; i < static_cast<int>(saves_.size()); i++) {
            float by = listY + i * (L.rowH + L.rowGap);
            if (by + L.rowH > L.contentY + L.contentH) break;

            HOI4::plate(r, L.contentX, by, L.contentW, L.rowH);


            SDL_Texture* sFileIcon = slAssets.icon("File-Alt");
            float sfSz = L.rowH * 0.42f;
            if (sFileIcon) {
                SDL_SetTextureColorMod(sFileIcon, 210, 200, 172);
                SDL_Rect sfDst = {(int)(L.contentX + std::round(L.pad * 0.35f)), (int)(by + L.rowH * 0.5f - sfSz * 0.5f), (int)sfSz, (int)sfSz};
                SDL_RenderCopy(r, sFileIcon, nullptr, &sfDst);
                SDL_SetTextureColorMod(sFileIcon, 255, 255, 255);
            }

            float sTextIndent = std::round(L.pad * 0.35f) + sfSz + std::round(L.pad * 0.3f);
            UIPrim::drawText(r, saves_[i].name, bodyFs,
                             L.contentX + sTextIndent, by + L.rowH * 0.42f,
                             "midleft", Theme::cream);
            UIPrim::drawText(r, saves_[i].date, metaFs,
                             L.contentX + L.contentW - std::round(L.pad * 0.4f), by + L.rowH * 0.42f,
                             "midright", Theme::grey);
        }
    } else {

        float startY = L.contentY;
        float secHdrH = std::round(u * 2.8f);
        float secPad = std::round(u * 0.6f);
        float secH = secHdrH + secPad;

        if (saves_.empty()) {
            UIPrim::drawText(r, "No saves found", emptyFs,
                             W * 0.5f, L.contentY + L.contentH * 0.40f, "center", Theme::grey);
            UIPrim::drawText(r, "Create one from the in-game pause menu.", metaFs,
                             W * 0.5f, L.contentY + L.contentH * 0.48f, "center", Theme::dark_grey);
        } else {
            UIPrim::drawSectionHeader(r, L.contentX, startY, L.contentW, secHdrH, "Manual Saves");
            float autoHdrY = startY + secH + manualCount_ * (L.rowH + L.rowGap);
            if (autoHdrY + secHdrH < L.contentY + L.contentH) {
                UIPrim::drawSectionHeader(r, L.contentX, autoHdrY, L.contentW, secHdrH, "Autosaves");
                if (manualCount_ == static_cast<int>(saves_.size())) {
                    UIPrim::drawText(r, "No autosaves yet", metaFs,
                                     L.contentX + L.contentW * 0.5f, autoHdrY + secH + L.rowH * 0.4f,
                                     "center", Theme::dark_grey);
                }
            }
        }

        for (int i = 0; i < static_cast<int>(saves_.size()); i++) {
            float by = startY + secH + i * (L.rowH + L.rowGap);
            if (i >= manualCount_) by += secH;
            if (by + L.rowH > L.contentY + L.contentH) break;

            bool hovered = (mx >= L.contentX && mx <= L.contentX + L.contentW && my >= by && my <= by + L.rowH);
            HOI4::button(r, L.contentX, by, L.contentW, L.rowH, hovered, false, true);


            SDL_Texture* fileIcon = slAssets.icon("File-Alt");
            float fiSz = L.rowH * 0.45f;
            if (fileIcon) {
                SDL_SetTextureColorMod(fileIcon, 210, 200, 172);
                SDL_Rect fiDst = {(int)(L.contentX + std::round(L.pad * 0.35f)), (int)(by + (L.rowH - fiSz) * 0.5f), (int)fiSz, (int)fiSz};
                SDL_RenderCopy(r, fileIcon, nullptr, &fiDst);
                SDL_SetTextureColorMod(fileIcon, 255, 255, 255);
            }

            float textIndent = std::round(L.pad * 0.35f) + fiSz + std::round(L.pad * 0.3f);
            UIPrim::drawText(r, saves_[i].name, bodyFs,
                             L.contentX + textIndent, by + L.rowH * 0.35f,
                             "midleft", Theme::cream);
            UIPrim::drawText(r, saves_[i].date, metaFs,
                             L.contentX + L.contentW - std::round(L.rowH * 1.8f), by + L.rowH * 0.65f,
                             "midright", Theme::grey);


            float delBtnW = std::round(L.rowH * 1.4f);
            float delBtnH = std::round(L.rowH * 0.62f);
            float delBtnX = L.contentX + L.contentW - delBtnW - std::round(L.pad * 0.35f);
            float delBtnY = by + (L.rowH - delBtnH) * 0.5f;
            bool delHov = (mx >= delBtnX && mx <= delBtnX + delBtnW &&
                           my >= delBtnY && my <= delBtnY + delBtnH);
            HOI4::button(r, delBtnX, delBtnY, delBtnW, delBtnH, delHov, false, true);
            UIPrim::drawText(r, "Del", metaFs,
                             delBtnX + delBtnW * 0.5f, delBtnY + delBtnH * 0.5f,
                             "center", Theme::grey);
        }
    }


    UIPrim::drawMenuButton(r, L.backX, L.backY, L.backW, L.backH,
                           "Back", mx, my, false);


    SDL_Texture* backIcon = slAssets.icon("SolidArrow-Left");
    if (backIcon) {
        float biSz = L.backH * 0.5f;
        SDL_SetTextureColorMod(backIcon, 210, 200, 172);
        SDL_Rect biDst = {(int)(L.backX + std::round(L.pad * 0.35f)), (int)(L.backY + (L.backH - biSz) * 0.5f), (int)biSz, (int)biSz};
        SDL_RenderCopy(r, backIcon, nullptr, &biDst);
        SDL_SetTextureColorMod(backIcon, 255, 255, 255);
    }
    UIAnim::popMenuTransition(r);
    UIAnim::drawTransitionFade(r, "screen:saveload", W, H);
}
