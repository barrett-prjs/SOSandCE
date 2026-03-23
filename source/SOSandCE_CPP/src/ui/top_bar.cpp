#include "ui/top_bar.h"
#include "core/app.h"
#include "core/engine.h"
#include "ui/primitives.h"
#include "ui/theme.h"
#include "ui/ui_assets.h"
#include "game/game_state.h"
#include "game/country.h"
#include "game/economy.h"
#include <algorithm>
#include <iomanip>
#include "game/helpers.h"
#include "game/buildings.h"


TopBar::TopBar() {
    auto& eng = Engine::instance();
    height_ = std::max(56, static_cast<int>(eng.HEIGHT * 0.055f));
}


void TopBar::rebuild(App& app, GameState& gs, int width) {
    width_ = width;
    auto& eng = Engine::instance();
    height_ = std::max(56, static_cast<int>(eng.HEIGHT * 0.055f));

    slotRects_.clear();

    Country* player = gs.getCountry(gs.controlledCountry);
    if (!player) return;


    if (surface_) {
        SDL_DestroyTexture(surface_);
        surface_ = nullptr;
    }


    float slotW = Theme::s(5.0f);
    float slotH = static_cast<float>(height_) - 4;
    float gap = Theme::s(0.3f);


    float startX = Theme::s(4.0f);
    float slotY = 2;


    struct SlotDef {
        std::string label;
        std::string value;
        std::vector<std::string> tip;
    };

    std::ostringstream oss;
    oss << std::fixed;
    oss.precision(0);

    std::vector<SlotDef> slots;


    oss.str(""); oss << player->money;
    slots.push_back({"$", oss.str(),
        {"Treasury", "Current funds: " + oss.str()}});


    slots.push_back({"Fac", std::to_string(player->factories),
        {"Factories", "Total: " + std::to_string(player->factories)}});


    oss.str(""); oss << player->manPower;
    slots.push_back({"MP", oss.str(),
        {"Manpower", "Available: " + oss.str()}});


    oss.str(""); oss << player->politicalPower;
    slots.push_back({"PP", oss.str(),
        {"Political Power", "Current: " + oss.str()}});


    oss.str(""); oss.precision(1); oss << player->stability << "%";
    slots.push_back({"Stab", oss.str(),
        {"Stability", "Current: " + oss.str()}});

    for (int i = 0; i < static_cast<int>(slots.size()); i++) {
        float sx = startX + i * (slotW + gap);
        SDL_Rect rect = {
            static_cast<int>(sx), static_cast<int>(slotY),
            static_cast<int>(slotW), static_cast<int>(slotH)
        };
        slotRects_.push_back({rect, slots[i].tip});
    }
}


void TopBar::render(SDL_Renderer* r) {
    if (!visible_) return;

    auto& eng = Engine::instance();
    auto& assets = UIAssets::instance();
    int W = width_ > 0 ? width_ : eng.WIDTH;


    UIPrim::drawRectFilled(r, {16, 18, 24}, 0, 0, static_cast<float>(W), static_cast<float>(height_));

    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, 255, 255, 255, 6);
    SDL_Rect topStrip = {0, 0, W, height_ / 3};
    SDL_RenderFillRect(r, &topStrip);


    UIPrim::drawHLine(r, Theme::gold_dim, 0, static_cast<float>(W), static_cast<float>(height_ - 2), 1);
    UIPrim::drawHLine(r, Theme::bg_dark, 0, static_cast<float>(W), static_cast<float>(height_ - 1), 1);


    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, 255, 255, 255, 8);
    SDL_RenderDrawLine(r, 0, 0, W, 0);


    float flagW = Theme::s(3.2f);
    float flagH = static_cast<float>(height_) - 8;
    float flagX = 4, flagY = 4;

    auto& gs = App::instance().gameState();
    Country* player = gs.getCountry(gs.controlledCountry);


    if (player) {
        std::string lowerName = gs.controlledCountry;
        std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
        SDL_Texture* flagTex = eng.loadTexture(eng.assetsPath + "flags/" + lowerName + "_flag.png");
        if (flagTex) {
            int fw, fh;
            SDL_QueryTexture(flagTex, nullptr, nullptr, &fw, &fh);
            float drawW = flagW, drawH = flagW * fh / fw;
            if (drawH > flagH) { drawH = flagH; drawW = flagH * fw / fh; }
            SDL_Rect fDst = {(int)(flagX + (flagW - drawW) / 2), (int)(flagY + (flagH - drawH) / 2),
                            (int)drawW, (int)drawH};
            SDL_RenderCopy(r, flagTex, nullptr, &fDst);
        } else {
            UIPrim::drawRectFilled(r, Theme::slot, flagX, flagY, flagW, flagH);
        }

        SDL_SetRenderDrawColor(r, Theme::gold_dim.r, Theme::gold_dim.g, Theme::gold_dim.b, 180);
        SDL_Rect flagBorder = {(int)flagX, (int)flagY, (int)flagW, (int)flagH};
        SDL_RenderDrawRect(r, &flagBorder);
    }

    if (!player) return;


    float slotW = Theme::s(5.0f);
    float slotH = static_cast<float>(height_) - 6;
    float gap = Theme::s(0.3f);
    float startX = Theme::s(4.2f);

    std::ostringstream oss;
    oss << std::fixed;

    struct SlotData {
        std::string label;
        std::string value;
        Color valueColor;
        const char* iconName;
    };

    oss.precision(0);
    std::vector<SlotData> slotsData;

    Color moneyColor = (player->money < 0) ? Theme::red_light : Theme::gold;
    slotsData.push_back({"$", Helpers::prefixNumber(std::max(0.0f, player->money)), moneyColor, "Cart"});

    int totalFac = player->buildingManager.countAll(BuildingType::CivilianFactory)
                 + player->buildingManager.countAll(BuildingType::MilitaryFactory);
    slotsData.push_back({"Fac", std::to_string(totalFac), Theme::cream, "Gear"});


    slotsData.push_back({"MP", Helpers::prefixNumber(std::max(0.0f, player->manPower)), Theme::cream, "Player"});

    slotsData.push_back({"PP", Helpers::prefixNumber(player->politicalPower), Theme::blue, "Star"});

    oss.str(""); oss << std::fixed << std::setprecision(0) << player->stability << "%";
    Color stabColor = (player->stability < 30) ? Theme::red_light :
                      (player->stability < 50) ? Theme::orange : Theme::cream;
    slotsData.push_back({"Stab", oss.str(), stabColor, "Levels"});

    int valSize = Theme::si(0.85f);

    for (int i = 0; i < static_cast<int>(slotsData.size()); i++) {
        float sx = startX + i * (slotW + gap);
        float sy = 3;


        UIPrim::drawBeveledRect(r, Theme::slot, sx, sy, slotW, slotH);


        SDL_Texture* slotIcon = assets.icon(slotsData[i].iconName);
        float iconSz = slotH * 0.32f;
        if (slotIcon) {
            SDL_SetTextureColorMod(slotIcon, Theme::dark_grey.r + 20, Theme::dark_grey.g + 20, Theme::dark_grey.b + 20);
            SDL_SetTextureAlphaMod(slotIcon, 180);
            SDL_Rect iDst = {(int)(sx + 4), (int)(sy + 3), (int)iconSz, (int)iconSz};
            SDL_RenderCopy(r, slotIcon, nullptr, &iDst);
            SDL_SetTextureColorMod(slotIcon, 255, 255, 255);
            SDL_SetTextureAlphaMod(slotIcon, 255);
        }


        UIPrim::drawText(r, slotsData[i].value, valSize,
                         sx + slotW * 0.5f, sy + slotH * 0.62f,
                         "center", slotsData[i].valueColor, true);
    }


    {
        float resSlotW = Theme::s(3.5f);
        float resStartX = startX + slotsData.size() * (slotW + gap) + gap * 2;
        float rsy = 3;
        int resFs = Theme::si(0.6f);
        int resValFs = Theme::si(0.7f);

        const char* resNames[] = {"Oil", "Stl", "Alu", "Tun", "Chr", "Rub"};
        Color resColors[] = {
            {50, 50, 60},
            {140, 140, 160},
            {170, 200, 230},
            {200, 150, 50},
            {180, 180, 200},
            {60, 140, 60},
        };
        auto& rm = player->resourceManager;

        for (int ri = 0; ri < RESOURCE_COUNT && ri < 6; ri++) {
            float rsx = resStartX + ri * (resSlotW + gap * 0.5f);


            SDL_SetRenderDrawColor(r, resColors[ri].r, resColors[ri].g, resColors[ri].b, 200);
            SDL_Rect colorBar = {(int)rsx, (int)rsy, (int)resSlotW, 3};
            SDL_RenderFillRect(r, &colorBar);


            UIPrim::drawRectFilled(r, {12, 14, 18}, rsx, rsy + 3, resSlotW, slotH - 3);
            SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(r, Theme::border.r, Theme::border.g, Theme::border.b, 40);
            SDL_Rect resBorder = {(int)rsx, (int)(rsy + 3), (int)resSlotW, (int)(slotH - 3)};
            SDL_RenderDrawRect(r, &resBorder);


            UIPrim::drawText(r, resNames[ri], resFs,
                             rsx + resSlotW * 0.5f, rsy + slotH * 0.3f,
                             "center", Theme::dark_grey);


            float stk = rm.stockpile[ri];
            float netR = rm.production[ri] + rm.tradeImports[ri] - rm.consumption[ri] - rm.tradeExports[ri];
            Color valCol = (netR >= 0) ? Theme::cream : Theme::red_light;
            std::string rvBuf = Helpers::resourceValueText(stk);
            UIPrim::drawText(r, rvBuf, resValFs,
                             rsx + resSlotW * 0.5f, rsy + slotH * 0.7f,
                             "center", valCol, true);
        }
    }


    float rightX = W - Theme::s(10.0f);


    std::string dateStr = std::to_string(gs.time.day) + "/" +
                          std::to_string(gs.time.month) + "/" +
                          std::to_string(gs.time.year);
    UIPrim::drawBeveledRect(r, Theme::slot, rightX, 3, Theme::s(5.0f), slotH);
    UIPrim::drawText(r, dateStr, valSize,
                     rightX + Theme::s(2.5f), 3 + slotH * 0.5f,
                     "center", Theme::cream, true);


    float speedX = rightX + Theme::s(5.5f);
    UIPrim::drawBeveledRect(r, Theme::slot, speedX, 3, Theme::s(4.0f), slotH);


    for (int i = 0; i < 5; i++) {
        float pipX = speedX + Theme::s(0.4f) + i * Theme::s(0.7f);
        float pipY = 3 + slotH * 0.30f;
        float pipW = Theme::s(0.5f);
        float pipH = Theme::s(0.55f);
        Color pipColor = (i < gs.speed) ? Theme::gold : Theme::dark_grey;
        UIPrim::drawRoundedRect(r, pipColor, pipX, pipY, pipW, pipH, 2);

        if (i < gs.speed) {
            SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(r, 255, 255, 255, 30);
            SDL_Rect ph = {(int)pipX + 1, (int)pipY + 1, (int)pipW - 2, (int)(pipH * 0.4f)};
            SDL_RenderFillRect(r, &ph);
        }
    }


    if (gs.speed == 0) {
        SDL_Texture* pauseIcon = assets.icon("Pause");
        if (pauseIcon) {
            float pSz = slotH * 0.45f;
            SDL_SetTextureColorMod(pauseIcon, Theme::red.r, Theme::red.g, Theme::red.b);
            SDL_Rect pDst = {(int)(speedX + Theme::s(2.0f) - pSz / 2), (int)(3 + slotH * 0.55f - pSz / 2),
                            (int)pSz, (int)pSz};
            SDL_RenderCopy(r, pauseIcon, nullptr, &pDst);
            SDL_SetTextureColorMod(pauseIcon, 255, 255, 255);
        } else {
            UIPrim::drawText(r, "||", Theme::si(0.9f),
                             speedX + Theme::s(2.0f), 3 + slotH * 0.65f,
                             "center", Theme::red);
        }
    }


    tooltip_.render(r, eng.WIDTH, eng.HEIGHT);
}


void TopBar::updateHover(int mx, int my) {
    tooltip_.hide();

    for (auto& slot : slotRects_) {
        SDL_Point pt = {mx, my};
        if (SDL_PointInRect(&pt, &slot.rect)) {
            tooltip_.show(slot.tip, static_cast<float>(mx), static_cast<float>(my));
            return;
        }
    }
}


bool TopBar::hit(int mx, int my) const {
    return my >= 0 && my < height_ && mx >= 0 && mx < width_;
}
