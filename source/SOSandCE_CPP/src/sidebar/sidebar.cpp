#include "sidebar/sidebar.h"
#include "core/app.h"
#include "core/engine.h"
#include "core/input.h"
#include "ui/primitives.h"
#include "ui/theme.h"
#include "ui/ui_assets.h"
#include "game/game_state.h"
#include "game/country.h"
#include "game/faction.h"
#include "game/political_options.h"
#include "game/effect_system.h"
#include "game/puppet.h"
#include "ui/toast.h"
#include "game/division.h"
#include "game/helpers.h"
#include "data/country_data.h"
#include "data/region_data.h"
#include "game/buildings.h"
#include "game/economy.h"
#include "core/audio.h"
#include "screens/game_screen.h"
#include <sstream>

namespace {

void drawSidebarCard(SDL_Renderer* r, float x, float y, float w, float h,
                     Color fill = {24, 25, 28, 255},
                     Color border = {156, 126, 54, 165},
                     int radius = 3) {
    UIPrim::drawRoundedRect(r, fill, x, y, w, h, radius, border);
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, 255, 255, 255, 5);
    SDL_Rect topGlow = {
        (int)(x + 1),
        (int)(y + 1),
        std::max(0, (int)(w - 2)),
        std::max(0, (int)(h * 0.16f))
    };
    SDL_RenderFillRect(r, &topGlow);
    SDL_SetRenderDrawColor(r, 0, 0, 0, 18);
    SDL_Rect bottomShade = {
        (int)(x + 1),
        (int)(y + h * 0.72f),
        std::max(0, (int)(w - 2)),
        std::max(0, (int)(h * 0.26f))
    };
    SDL_RenderFillRect(r, &bottomShade);
}

void drawSidebarCardTitle(SDL_Renderer* r, float x, float y, const std::string& title, int fontSize) {
    UIPrim::drawText(r, title, fontSize, x, y, "topleft", Theme::gold_bright, true);
}

std::pair<std::string, std::string> countryProfileLines(const Country& country) {
    if (!country.atWarWith.empty()) {
        if (!country.faction.empty()) {
            return {"Wartime state backed by", "allied coordination"};
        }
        return {"Mobilized for conflict and", "self-reliant planning"};
    }

    if (country.stability >= 65.0f) {
        return {"Stable institutions with", "long-term development focus"};
    }

    return {"Domestic politics remain", "in active realignment"};
}

std::string joinParts(const std::vector<std::string>& parts, const std::string& sep) {
    std::ostringstream oss;
    for (size_t i = 0; i < parts.size(); ++i) {
        if (i > 0) oss << sep;
        oss << parts[i];
    }
    return oss.str();
}

std::string formatResourceBundle(const std::array<float, RESOURCE_COUNT>& cost) {
    static const char* shortNames[] = {"Oil", "Stl", "Alu", "Tun", "Chr", "Rub"};
    std::vector<std::string> parts;
    for (int i = 0; i < RESOURCE_COUNT; ++i) {
        if (cost[i] <= 0.001f) continue;
        parts.push_back(std::string(shortNames[i]) + " " + Helpers::resourceValueText(cost[i]));
    }
    return parts.empty() ? "No resources" : joinParts(parts, " | ");
}

bool canAffordResourceBundle(const Country* country, const std::array<float, RESOURCE_COUNT>& cost) {
    if (!country) return false;
    for (int i = 0; i < RESOURCE_COUNT; ++i) {
        if (country->resourceManager.stockpile[i] + 0.001f < cost[i]) {
            return false;
        }
    }
    return true;
}

TradeContract* findImportContract(GameState& gs, const std::string& importer, Resource resource) {
    for (auto& contract : gs.tradeContracts) {
        if (contract.active && contract.importer == importer && contract.resource == resource) {
            return &contract;
        }
    }
    return nullptr;
}

float tradeDailyCost(float amount, float price) {
    return amount * price * 4.0f;
}

}

Sidebar::Sidebar() { sideBarSize_ = 0.27f; }

void Sidebar::toggle() { targetAnimation_ = (targetAnimation_ > 0.5f) ? 0.0f : 1.0f; }
void Sidebar::open()   { targetAnimation_ = 1.0f; }
void Sidebar::close()  { targetAnimation_ = 0.0f; }
void Sidebar::clearOverlayInfo() {
    overlayTitle.clear();
    overlayLines.clear();
}
void Sidebar::setOverlayInfo(const std::string& title, const std::vector<std::string>& lines, Color accent) {
    overlayTitle = title;
    overlayLines = lines;
    overlayAccent = accent;
    selectedCountry.clear();
}

bool Sidebar::showsPlayerTabs(const GameState& gs) const {
    if (gs.controlledCountry.empty()) return false;
    if (!overlayTitle.empty()) return false;
    if (!selectedCountry.empty() && selectedCountry != gs.controlledCountry) return false;
    return true;
}

float Sidebar::getWidth(int screenW) const { return screenW * sideBarSize_; }
float Sidebar::getX(int screenW) const {
    float sideW = getWidth(screenW);
    return leftSide ? (-sideW * (1.0f - animation)) : (screenW - sideW * animation);
}

void Sidebar::handleInput(App& app, const InputState& input) {}

void Sidebar::update(App& app, float dt) {
    float speed = 8.0f;
    if (animation < targetAnimation_)
        animation = std::min(animation + speed * dt, targetAnimation_);
    else if (animation > targetAnimation_)
        animation = std::max(animation - speed * dt, targetAnimation_);
}

void Sidebar::render(SDL_Renderer* r, App& app) {
    if (animation < 0.01f) return;

    auto& eng = Engine::instance();
    auto& gs = app.gameState();
    int W = eng.WIDTH, H = eng.HEIGHT;
    float ui = eng.uiScale;
    float u = H / 100.0f * Engine::instance().uiScaleFactor();

    float topBarH = std::max(50.0f, H * 0.06f);
    float sideW = W * sideBarSize_;
    float sideX = getX(W);
    float sideY = topBarH;
    float sideH = H - sideY;
    auto& assets = UIAssets::instance();
    float panelX = sideX;
    float panelY = sideY;
    float panelW = sideW;
    float panelH = sideH;

    UIPrim::drawRectFilled(r, {21, 22, 24, 252}, panelX, panelY, panelW, panelH);
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, 255, 255, 255, 5);
    SDL_Rect panelGlow = {(int)panelX, (int)panelY, (int)panelW, (int)(panelH * 0.08f)};
    SDL_RenderFillRect(r, &panelGlow);
    SDL_SetRenderDrawColor(r, Theme::gold_dim.r, Theme::gold_dim.g, Theme::gold_dim.b, 120);
    SDL_RenderDrawLine(r, (int)panelX, (int)panelY, (int)panelX, (int)(panelY + panelH));
    SDL_SetRenderDrawColor(r, Theme::border.r, Theme::border.g, Theme::border.b, 90);
    SDL_RenderDrawLine(r, (int)panelX + 1, (int)panelY, (int)panelX + 1, (int)(panelY + panelH));

    int mx, my; SDL_GetMouseState(&mx, &my);
    bool showTabs = showsPlayerTabs(gs);
    float tabH = 0.0f;
    float contentTop = panelY;
    float contentInset = std::round(u * 1.25f);

    if (showTabs) {
        float tabsX = panelX;
        float tabsY = panelY;
        float tabsW = panelW;
        tabH = std::max(40.0f, H * 0.06f);
        float tabW = tabsW / 3.0f;

        const char* tabs[] = {"Political", "Military", "Industry"};
        const char* tabKeys[] = {"political", "military", "industry"};
        for (int i = 0; i < 3; i++) {
            float tx = tabsX + i * tabW;
            float ty = tabsY;
            bool active = (activeTab == tabKeys[i]);
            bool hovered = (mx >= tx && mx <= tx + tabW && my >= ty && my <= ty + tabH);

            if (active) {
                UIPrim::drawRectFilled(r, {28, 28, 30, 255}, tx, ty, tabW, tabH);
                UIPrim::drawHLine(r, Theme::gold, tx + 8, tx + tabW - 8, ty + tabH - 2, 2);
            } else {
                Color tabColor = hovered ? Color{30, 30, 33, 255} : Color{24, 24, 27, 255};
                UIPrim::drawRectFilled(r, tabColor, tx, ty, tabW, tabH);
            }
            if (i > 0) UIPrim::drawVLine(r, {Theme::border.r, Theme::border.g, Theme::border.b, 110},
                                         tx, ty + 6, ty + tabH - 6, 1);

            int tabFs = std::max(13, static_cast<int>(tabH * 0.35f));
            Color tabTextColor = active ? Theme::gold_bright : (hovered ? Theme::cream : Theme::grey);
            UIPrim::drawText(r, tabs[i], tabFs, tx + tabW * 0.5f, ty + tabH * 0.5f,
                             "center", tabTextColor, active);
        }
        UIPrim::drawHLine(r, Theme::gold_dim, tabsX, tabsX + tabsW, tabsY + tabH, 1);
        contentTop = tabsY + tabH;
    } else {
        float headerH = std::max(42.0f, H * 0.05f);
        std::string headerTitle = "Country Overview";
        if (!overlayTitle.empty()) {
            headerTitle = "Map Overlay";
        } else if (gs.mapViewOnly && gs.controlledCountry.empty()) {
            headerTitle = "Map Preview";
        } else if (gs.controlledCountry.empty()) {
            headerTitle = "Spectator Panel";
        }
        float headerX = panelX + contentInset;
        float headerY = panelY + contentInset;
        float headerW = panelW - contentInset * 2.0f;
        drawSidebarCard(r, headerX, headerY, headerW, headerH, {28, 28, 30, 245}, {Theme::gold_dim.r, Theme::gold_dim.g, Theme::gold_dim.b, 150}, 2);
        UIPrim::drawHLine(r, overlayAccent, headerX + u * 0.8f, headerX + headerW - u * 0.8f, headerY + headerH - 2, 2);
        UIPrim::drawText(r, headerTitle, std::max(14, (int)(headerH * 0.33f)),
                         headerX + headerW * 0.5f, headerY + headerH * 0.5f, "center",
                         !overlayTitle.empty() ? overlayAccent : Theme::gold_bright, true);
        contentTop = headerY + headerH;
    }

    float pad = std::round(H * 0.014f);
    float contentY = contentTop + pad;
    float w = panelW - (contentInset + pad) * 2.0f;
    float x = panelX + contentInset + pad;

    Country* cc = gs.getCountry(gs.controlledCountry);
    float y = contentY;
    int fs = std::max(14, H / 60);
    int fsLarge = std::max(16, H / 50);
    float row_h = std::max(26.0f, H * 0.028f);
    float btn_h = std::max(34.0f, H * 0.042f);

    if (showTabs) {
        if (!cc) return;
        bool dtHovered = (mx >= x && mx <= x + w && my >= y && my <= y + btn_h);
        bool dtClicked = dtHovered && app.input().mouseLeftDown;
        UIPrim::drawActionRow(r, x, y, w, btn_h, "Decision Tree", mx, my, dtClicked);
        if (dtClicked) {
            Audio::instance().playSound("clickedSound");
            app.switchScreen(ScreenType::DECISION_TREE);
        }
        y += btn_h + H * 0.01f;
    }


    if (!overlayTitle.empty()) {
        y = renderOverlayInfo(r, app, x, y, w);
    } else if (!selectedCountry.empty() && selectedCountry != gs.controlledCountry) {
        y = renderForeignTab(r, app, x, y, w);
    } else {
        if (!cc) return;
        if (activeTab == "political") y = renderPoliticalTab(r, app, x, y, w);
        else if (activeTab == "military") y = renderMilitaryTab(r, app, x, y, w);
        else if (activeTab == "industry") y = renderIndustryTab(r, app, x, y, w);
    }
}

float Sidebar::renderPoliticalTab(SDL_Renderer* r, App& app, float x, float y, float w) {
    auto& gs = app.gameState();
    Country* cc = gs.getCountry(gs.controlledCountry);
    if (!cc) return y;

    int H = Engine::instance().HEIGHT;
    float u = H / 100.0f * Engine::instance().uiScaleFactor();
    int fsValue = std::max(14, (int)(u * 1.1f));
    int fsLabel = std::max(11, (int)(u * 0.86f));
    int fsSmall = std::max(10, (int)(u * 0.78f));
    float gap = std::round(u * 0.8f);
    float cardPad = std::round(u * 0.9f);
    float infoH = std::round(u * 4.6f);
    float colW = (w - gap) * 0.5f;
    int mx, my; SDL_GetMouseState(&mx, &my);

    auto clipText = [](const std::string& text, size_t maxLen) {
        if (text.size() <= maxLen) return text;
        return text.substr(0, maxLen - 3) + "...";
    };
    auto formatOneDecimal = [](float value) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%.1f", value);
        return std::string(buf);
    };

    auto stabilityLabel = [&](float stability) {
        if (stability >= 75.0f) return std::string("Very High");
        if (stability >= 55.0f) return std::string("High");
        if (stability >= 40.0f) return std::string("Unsteady");
        return std::string("Low");
    };

    auto drawInfoCard = [&](float cx, float cy, float cw, float ch,
                            const std::string& title, const std::string& value, Color valueColor) {
        drawSidebarCard(r, cx, cy, cw, ch);
        drawSidebarCardTitle(r, cx + cardPad, cy + cardPad * 0.7f, title, fsLabel);
        int localValueFs = std::max(fsLabel + 1, value.size() > 24 ? fsValue - 2 : fsValue);
        UIPrim::drawText(r, clipText(value, 32), localValueFs, cx + cardPad, cy + ch - cardPad * 1.05f,
                         "bottomleft", valueColor, false);
    };

    auto drawMetricCell = [&](float cx, float cy, float cw, float ch,
                              const std::string& label, const std::string& value, Color valueColor) {
        drawSidebarCard(r, cx, cy, cw, ch, {22, 24, 31, 255}, {48, 52, 60, 140}, 6);
        UIPrim::drawText(r, label, fsSmall, cx + cardPad * 0.9f, cy + cardPad * 0.75f, "topleft", Theme::grey);
        UIPrim::drawText(r, clipText(value, 24), fsValue, cx + cardPad * 0.9f, cy + ch - cardPad * 0.8f,
                         "bottomleft", valueColor, false);
    };

    std::string faction = cc->faction.empty() ? "None" : replaceAll(cc->faction, "_", " ");
    std::string ideology = Helpers::getIdeologyName(cc->ideology[0], cc->ideology[1]);
    if (!ideology.empty()) ideology[0] = std::toupper(ideology[0]);
    std::string focusStr = "None";
    if (cc->focus.has_value()) {
        auto& [fn, fd, fe] = *cc->focus;
        focusStr = replaceAll(fn, "_", " ");
    }
    std::string leaderName = cc->leader.name.empty() ? "Vacant" : cc->leader.name;

    drawInfoCard(x, y, colW, infoH, "Faction", faction, cc->faction.empty() ? Theme::grey : Theme::gold_bright);
    drawInfoCard(x + colW + gap, y, colW, infoH, "Ideology", ideology, Theme::cream);
    y += infoH + gap;

    drawInfoCard(x, y, colW, infoH, "Current Focus", focusStr, cc->focus.has_value() ? Theme::cream : Theme::grey);
    drawInfoCard(x + colW + gap, y, colW, infoH, "Leader", leaderName, Theme::cream);
    y += infoH + gap;

    float profileH = std::round(u * 8.8f);
    drawSidebarCard(r, x, y, w, profileH);
    drawSidebarCardTitle(r, x + cardPad, y + cardPad * 0.75f, "National Profile", fsLabel);

    float previewSize = profileH - cardPad * 2.4f;
    float previewX = x + cardPad;
    float previewY = y + cardPad * 1.8f;
    float textX = previewX + previewSize + gap;
    std::string lowerName = gs.controlledCountry;
    std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
    SDL_Texture* flagTex = Engine::instance().loadTexture(Engine::instance().assetsPath + "flags/" + lowerName + "_flag.png");
    if (flagTex) {
        drawSidebarCard(r, previewX, previewY, previewSize, previewSize, {12, 14, 18, 255}, {78, 70, 44, 160}, 6);
        SDL_Rect dst = {(int)(previewX + 4), (int)(previewY + 4), (int)(previewSize - 8), (int)(previewSize - 8)};
        SDL_RenderCopy(r, flagTex, nullptr, &dst);
    } else {
        drawSidebarCard(r, previewX, previewY, previewSize, previewSize, {12, 14, 18, 255}, {78, 70, 44, 160}, 6);
        float half = previewSize * 0.5f;
        UIPrim::drawRectFilled(r, {160, 78, 78}, previewX + 4, previewY + 4, half - 4, half - 4);
        UIPrim::drawRectFilled(r, {65, 118, 185}, previewX + half, previewY + 4, half - 4, half - 4);
        UIPrim::drawRectFilled(r, {82, 160, 82}, previewX + 4, previewY + half, half - 4, half - 4);
        UIPrim::drawRectFilled(r, {136, 108, 178}, previewX + half, previewY + half, half - 4, half - 4);
    }

    auto [profileLine1, profileLine2] = countryProfileLines(*cc);
    UIPrim::drawText(r, ideology, fsValue + 1, textX, previewY + 4, "topleft", Theme::cream, true);
    UIPrim::drawText(r, profileLine1, fsSmall, textX, previewY + previewSize * 0.42f, "topleft", Theme::grey);
    UIPrim::drawText(r, profileLine2, fsSmall, textX, previewY + previewSize * 0.68f, "topleft", Theme::grey);

    int puppetCount = 0;
    for (auto& ps : gs.puppetStates) {
        if (ps.active && ps.overlord == gs.controlledCountry) puppetCount++;
    }
    std::string profileMeta = cc->puppetTo.empty()
        ? ("Focus: " + clipText(focusStr, 20))
        : ("Subject of " + clipText(replaceAll(cc->puppetTo, "_", " "), 16));
    UIPrim::drawText(r, profileMeta, fsSmall, textX, previewY + previewSize - 2, "bottomleft",
                     cc->puppetTo.empty() ? Theme::gold_dim : Theme::red_light);
    y += profileH + gap;

    float infoRowH = std::round(u * 2.35f);
    float statsH = cardPad * 2.4f + infoRowH * 6.0f;
    drawSidebarCard(r, x, y, w, statsH);
    drawSidebarCardTitle(r, x + cardPad, y + cardPad * 0.7f, "Statistics", fsLabel);
    float statsTop = y + cardPad * 1.8f;
    float statsW = w - cardPad * 2.0f;
    UIPrim::drawInfoRow(r, x + cardPad, statsTop, statsW, infoRowH, "Population", Helpers::prefixNumber(cc->population)); statsTop += infoRowH;
    UIPrim::drawInfoRow(r, x + cardPad, statsTop, statsW, infoRowH, "Stability", stabilityLabel(cc->stability),
                        {0,0,0,0}, cc->stability >= 55.0f ? Theme::green_light : Theme::red_light); statsTop += infoRowH;
    UIPrim::drawInfoRow(r, x + cardPad, statsTop, statsW, infoRowH, "Political Power", std::to_string((int)cc->politicalPower),
                        {0,0,0,0}, Theme::gold_bright); statsTop += infoRowH;
    UIPrim::drawInfoRow(r, x + cardPad, statsTop, statsW, infoRowH, "Regions", std::to_string(cc->regions.size())); statsTop += infoRowH;
    UIPrim::drawInfoRow(r, x + cardPad, statsTop, statsW, infoRowH, "At War With", std::to_string(cc->atWarWith.size()),
                        {0,0,0,0}, cc->atWarWith.empty() ? Theme::grey : Theme::red_light); statsTop += infoRowH;
    UIPrim::drawInfoRow(r, x + cardPad, statsTop, statsW, infoRowH, "Puppets", std::to_string(puppetCount),
                        {0,0,0,0}, puppetCount > 0 ? Theme::green_light : Theme::grey);
    y += statsH + gap;

    float atkVal = std::round(cc->combatStats.attack * cc->attackMultiplier * 10.0f) / 10.0f;
    float defVal = std::round(cc->combatStats.defense * cc->defenseMultiplier * 10.0f) / 10.0f;
    float combatH = cardPad * 2.4f + infoRowH * 5.0f;
    drawSidebarCard(r, x, y, w, combatH);
    drawSidebarCardTitle(r, x + cardPad, y + cardPad * 0.7f, "Combat Stats", fsLabel);
    float combatTop = y + cardPad * 1.8f;
    float combatW = w - cardPad * 2.0f;
    UIPrim::drawInfoRow(r, x + cardPad, combatTop, combatW, infoRowH, "Attack", formatOneDecimal(atkVal)); combatTop += infoRowH;
    UIPrim::drawInfoRow(r, x + cardPad, combatTop, combatW, infoRowH, "Defense", formatOneDecimal(defVal),
                        {0,0,0,0}, Theme::green_light); combatTop += infoRowH;
    UIPrim::drawInfoRow(r, x + cardPad, combatTop, combatW, infoRowH, "Armor", formatOneDecimal(cc->combatStats.armor)); combatTop += infoRowH;
    UIPrim::drawInfoRow(r, x + cardPad, combatTop, combatW, infoRowH, "Piercing", formatOneDecimal(cc->combatStats.piercing)); combatTop += infoRowH;
    UIPrim::drawInfoRow(r, x + cardPad, combatTop, combatW, infoRowH, "Speed", formatOneDecimal(cc->combatStats.speed),
                        {0,0,0,0}, Theme::gold_bright);
    y += combatH + gap;

    auto options = getOptions(gs.controlledCountry, gs);
    if (options.empty()) return y;

    float actH = std::round(u * 3.35f);
    float catH = std::round(u * 2.8f);
    bool mouseDown = app.input().mouseLeftDown;
    std::map<std::string, std::vector<PoliticalOption*>> grouped;
    for (auto& opt : options) {
        grouped[opt.category].push_back(&opt);
    }

    static std::string openCategory;
    float actionsH = cardPad * 2.4f + std::round(u * 1.1f);
    for (auto& [category, catOpts] : grouped) {
        actionsH += catH + gap * 0.45f;
        if (openCategory == category) {
            actionsH += catOpts.size() * (actH + gap * 0.35f);
        }
    }

    drawSidebarCard(r, x, y, w, actionsH);
    drawSidebarCardTitle(r, x + cardPad, y + cardPad * 0.7f, "Political Actions", fsLabel);
    float actY = y + cardPad * 1.9f;

    for (auto& [category, catOpts] : grouped) {
        std::string catTitle = category.empty() ? "General" : category;
        if (!catTitle.empty()) catTitle[0] = std::toupper(catTitle[0]);
        bool catHover = mx >= x + cardPad && mx <= x + w - cardPad && my >= actY && my <= actY + catH;
        drawSidebarCard(r, x + cardPad, actY, w - cardPad * 2.0f, catH,
                        catHover ? Color{28, 31, 39, 255} : Color{20, 22, 28, 255},
                        {48, 52, 60, 150}, 6);
        UIPrim::drawText(r, catTitle, fsLabel, x + cardPad * 1.9f, actY + catH * 0.5f, "midleft",
                         catHover ? Theme::gold : Theme::cream);
        UIPrim::drawText(r, openCategory == category ? "v" : ">", fsLabel,
                         x + w - cardPad * 1.7f, actY + catH * 0.5f, "center", Theme::grey);

        if (catHover && mouseDown) {
            openCategory = (openCategory == category) ? "" : category;
        }
        actY += catH + gap * 0.45f;

        if (openCategory == category) {
            for (auto* opt : catOpts) {
                bool canAfford = cc->politicalPower >= opt->ppCost;
                std::string costStr = std::to_string(opt->ppCost) + " PP";
                bool hov = mx >= x + cardPad && mx <= x + w - cardPad && my >= actY && my <= actY + actH;
                UIPrim::drawActionRow(r, x + cardPad, actY, w - cardPad * 2.0f, actH,
                                      opt->name, mx, my, hov && mouseDown,
                                      costStr, opt->description, canAfford);

                if (hov && mouseDown && canAfford) {
                    cc->politicalPower -= opt->ppCost;
                    EffectSystem efx;
                    auto effects = efx.parse(opt->effects, gs.controlledCountry);
                    efx.executeBatch(effects, gs);
                    Audio::instance().playSound("clickedSound");
                    toasts().show("Enacted: " + opt->name, 2000);
                }

                actY += actH + gap * 0.35f;
            }
        }
    }

    return y + actionsH;
}

float Sidebar::renderMilitaryTab(SDL_Renderer* r, App& app, float x, float y, float w) {
    auto& gs = app.gameState();
    Country* cc = gs.getCountry(gs.controlledCountry);
    if (!cc) return y;
    int H = Engine::instance().HEIGHT;
    float u = H / 100.0f * Engine::instance().uiScaleFactor();

    int fs = std::max(14, (int)(u * 1.2f));
    float row_h = std::round(u * 2.6f);
    float pad = std::round(u * 0.8f);
    float btn_h = std::round(u * 4.5f);
    float secH = std::round(u * 3.2f);
    int mx, my; SDL_GetMouseState(&mx, &my);


    float ovH = row_h * 5.2f + pad * 2;
    drawSidebarCard(r, x, y, w, ovH);

    float iy = y + pad;
    float iw = w - pad * 2;
    UIPrim::drawInfoRow(r, x + pad, iy, iw, row_h, "Size", Helpers::getMilitarySizeName(cc->militarySize)); iy += row_h;
    std::string deploy = cc->deployRegion.empty() ? "None" : cc->deployRegion;
    UIPrim::drawInfoRow(r, x + pad, iy, iw, row_h, "Deployment", deploy); iy += row_h;

    float totalStacks = 0;
    for (auto& d : cc->divisions) totalStacks += d->divisionStack;
    float dailyUp = totalStacks * gs.DIVISION_UPKEEP_PER_DAY;
    int armsC = cc->buildingManager.countAll(BuildingType::MilitaryFactory);
    int tDays = std::max(3, (int)(14 * std::max(0.25f, 1.0f - armsC * 0.15f)));
    float trainScale = 1.0f + static_cast<float>(cc->divisions.size()) * 0.05f;
    float trainDiscount = std::max(0.5f, 1.0f - armsC * 0.05f);
    float trainOneMoney = gs.TRAINING_COST_PER_DIV * trainScale * trainDiscount;
    auto trainOneBundle = Country::trainingCostBundle(1);

    char buf[128];
    snprintf(buf, sizeof(buf), "$%s/day", Helpers::prefixNumber(dailyUp).c_str());
    UIPrim::drawInfoRow(r, x + pad, iy, iw, row_h, "Upkeep", buf, {0,0,0,0}, Theme::orange); iy += row_h;
    snprintf(buf, sizeof(buf), "$%s | %dd", Helpers::prefixNumber(trainOneMoney).c_str(), tDays);
    UIPrim::drawInfoRow(r, x + pad, iy, iw, row_h, "Train Cost", buf); iy += row_h;
    UIPrim::drawInfoRow(r, x + pad, iy, iw, row_h, "Train Res.", formatResourceBundle(trainOneBundle)); iy += row_h;

    y += ovH + std::round(u * 0.5f);


    {
        float cdH = std::round(u * 3.5f);
        bool cdHov = mx >= x && mx <= x + w && my >= y && my <= y + cdH;
        bool mouseDown2 = app.input().mouseLeftDown;
        UIPrim::drawActionRow(r, x, y, w, cdH, "Change Deployment", mx, my, cdHov && mouseDown2, "", cc->deployRegion.empty() ? "None" : cc->deployRegion);
        if (cdHov && mouseDown2) {
            cc->changeDeployment(gs);
            Audio::instance().playSound("clickedSound");
        }
        y += cdH + std::round(u * 0.5f);
    }


    float tall = std::max(40.0f, H * 0.05f);
    bool canTrain1 = (cc->money >= trainOneMoney && cc->manPower >= 10000 &&
                      canAffordResourceBundle(cc, trainOneBundle));
    bool mouseDown = app.input().mouseLeftDown;

    snprintf(buf, sizeof(buf), "Train 1 Division");
    bool t1Hovered = (mx >= x && mx <= x + w && my >= y && my <= y + tall);
    bool t1Clicked = t1Hovered && mouseDown;
    UIPrim::drawActionRow(r, x, y, w, tall, buf, mx, my, t1Clicked,
                           "$" + Helpers::prefixNumber(trainOneMoney),
                           "10k MP | " + std::to_string(tDays) + "d | " + formatResourceBundle(trainOneBundle), canTrain1);
    if (t1Clicked) {
        if (canTrain1) {
            cc->trainDivision(gs, 1);
            Audio::instance().playSound("clickedSound");
        } else {
            Audio::instance().playSound("failedClickSound");
        }
    }
    y += tall + H * 0.005f;

    int allDiv = (int)(cc->manPower / 10000);
    float trainAllMoney = trainOneMoney * allDiv;
    auto trainAllBundle = Country::trainingCostBundle(std::max(0, allDiv));
    bool canTrainAll = (allDiv > 0 && cc->money >= trainAllMoney &&
                        canAffordResourceBundle(cc, trainAllBundle));
    snprintf(buf, sizeof(buf), "Train All (%d)", allDiv);
    bool tAllHovered = (mx >= x && mx <= x + w && my >= y && my <= y + tall);
    bool tAllClicked = tAllHovered && mouseDown;
    UIPrim::drawActionRow(r, x, y, w, tall, buf, mx, my, tAllClicked,
                           "$" + Helpers::prefixNumber(trainAllMoney),
                           Helpers::prefixNumber(allDiv * 10000.0f) + " MP | " + formatResourceBundle(trainAllBundle), canTrainAll);
    if (tAllClicked) {
        if (canTrainAll) {
            cc->trainDivision(gs, allDiv);
            Audio::instance().playSound("clickedSound");
        } else {
            Audio::instance().playSound("failedClickSound");
        }
    }
    y += tall + H * 0.01f;


    y = UIPrim::drawSectionHeader(r, x - pad, y, w + pad * 2, secH, "Training");
    y += H * 0.005f;
    if (cc->training.empty()) {
        UIPrim::drawText(r, "No units training", fs, x, y + H * 0.005f, "midleft", Theme::dark_grey);
        y += row_h;
    } else {
        for (int i = 0; i < std::min((int)cc->training.size(), 6); i++) {
            bool ready = (cc->training[i][1] == 0);
            auto deployBundle = Country::deploymentCostBundle(cc->training[i][0]);
            bool canDeploy = ready && canAffordResourceBundle(cc, deployBundle);
            std::string label = ready
                ? "Deploy " + std::to_string(cc->training[i][0]) + " Div"
                : "Training " + std::to_string(cc->training[i][0]) + " Div (" + std::to_string(cc->training[i][1]) + "d)";
            bool trHov = canDeploy && mx >= x && mx <= x + w && my >= y && my <= y + btn_h;
            bool trClick = trHov && mouseDown;
            UIPrim::drawActionRow(r, x, y, w, btn_h, label, mx, my, trClick,
                                  ready ? (canDeploy ? "Ready" : "Need stock") : (std::to_string(cc->training[i][1]) + "d"),
                                  "Deploy: " + formatResourceBundle(deployBundle), canDeploy);
            if (trClick) {
                if (cc->deployTrainingDivision(gs, i)) {
                    Audio::instance().playSound("clickedSound");
                } else {
                    Audio::instance().playSound("failedClickSound");
                }
                break;
            }
            y += btn_h + H * 0.004f;
        }
        if ((int)cc->training.size() > 6) {
            UIPrim::drawText(r, "+" + std::to_string(cc->training.size() - 6) + " more...", fs,
                             x + w / 2, y, "topcenter", Theme::grey);
            y += row_h;
        }
    }

    y += H * 0.008f;
    y = UIPrim::drawSectionHeader(r, x - pad, y, w + pad * 2, secH, "Divisions");
    y += H * 0.005f;
    int totalDivisions = std::max(0, (int)std::round(totalStacks));
    snprintf(buf, sizeof(buf), "%d divisions in %d armies (%s troops)",
             totalDivisions, (int)cc->divisions.size(), Helpers::prefixNumber(cc->totalMilitary).c_str());
    UIPrim::drawText(r, buf, fs, x, y + H * 0.005f, "midleft", Theme::cream);
    y += row_h;

    return y;
}

float Sidebar::renderIndustryTab(SDL_Renderer* r, App& app, float x, float y, float w) {
    auto& gs = app.gameState();
    Country* cc = gs.getCountry(gs.controlledCountry);
    if (!cc) return y;
    int H = Engine::instance().HEIGHT;
    float u = H / 100.0f * Engine::instance().uiScaleFactor();

    int fs = std::max(14, (int)(u * 1.2f));
    int fsSm = std::max(12, (int)(u * 0.9f));
    float row_h = std::round(u * 2.6f);
    float pad = std::round(u * 0.8f);
    float secH = std::round(u * 3.2f);
    float constructSpeed = cc->buildingManager.getConstructionSpeed(cc) * cc->resourceManager.getProductionPenalty();
    int mx, my; SDL_GetMouseState(&mx, &my);


    int civC = cc->buildingManager.countAll(BuildingType::CivilianFactory);
    float dailyIncome = 5000.0f * (civC + 1) * cc->moneyMultiplier;
    float totalStacks = 0;
    for (auto& d : cc->divisions) totalStacks += d->divisionStack;
    float dailyUpkeep = totalStacks * gs.DIVISION_UPKEEP_PER_DAY;
    float net = dailyIncome - dailyUpkeep;
    Color nc = net >= 0 ? Theme::green_light : Theme::red_light;

    float econH = row_h * 3 + pad * 2;
    drawSidebarCard(r, x, y, w, econH);

    float iy = y + pad;
    float iw = w - pad * 2;
    UIPrim::drawInfoRow(r, x + pad, iy, iw, row_h, "Income", Helpers::prefixNumber(dailyIncome) + "/d", {0,0,0,0}, Theme::green_light); iy += row_h;
    UIPrim::drawInfoRow(r, x + pad, iy, iw, row_h, "Upkeep", "-" + Helpers::prefixNumber(dailyUpkeep) + "/d", {0,0,0,0}, Theme::orange); iy += row_h;
    UIPrim::drawInfoRow(r, x + pad, iy, iw, row_h, "Net", Helpers::prefixNumber(net) + "/d", {0,0,0,0}, nc); iy += row_h;

    y += econH + std::round(u * 0.8f);


    y = UIPrim::drawSectionHeader(r, x - pad, y, w + pad * 2, secH, "Resources");
    y += H * 0.005f;
    const char* resNames[] = {"Oil", "Steel", "Aluminum", "Tungsten", "Chromium", "Rubber"};
    auto& rm = cc->resourceManager;
    for (int ri = 0; ri < RESOURCE_COUNT && ri < 6; ri++) {
        float prod = rm.production[ri], cons = rm.consumption[ri];
        float imp = rm.tradeImports[ri], exp = rm.tradeExports[ri];
        float stk = rm.stockpile[ri], netR = prod + imp - cons - exp;
        Color rc = netR >= 0 ? Theme::green_light : Theme::red_light;
        char buf[128];
        std::string stockText = std::string(resNames[ri]) + ": " + Helpers::resourceValueText(stk);
        UIPrim::drawText(r, stockText, fs, x, y, "midleft", rc); y += H * 0.022f;
        if (imp > 0.0f || exp > 0.0f)
            snprintf(buf, sizeof(buf), "  +%.1f +%.1fi -%.1f -%.1fe = %+.1f/d", prod, imp, cons, exp, netR);
        else
            snprintf(buf, sizeof(buf), "  +%.1f -%.1f = %+.1f/d", prod, cons, netR);
        UIPrim::drawText(r, buf, fsSm, x, y, "midleft", Theme::grey); y += H * 0.022f;
    }
    y += H * 0.008f;


    {
        float prodPen = rm.getProductionPenalty();
        float combatPen = rm.getCombatPenalty();
        if (prodPen < 1.0f) {
            char penBuf[64];
            snprintf(penBuf, sizeof(penBuf), "Production penalty: %.0f%%", prodPen * 100);
            UIPrim::drawText(r, penBuf, fsSm, x, y, "midleft", Theme::orange);
            y += H * 0.022f;
        }
        if (combatPen < 1.0f) {
            char penBuf[64];
            snprintf(penBuf, sizeof(penBuf), "Combat penalty: %.0f%%", combatPen * 100);
            UIPrim::drawText(r, penBuf, fsSm, x, y, "midleft", Theme::red_light);
            y += H * 0.022f;
        }
    }
    y += H * 0.005f;


    y = UIPrim::drawSectionHeader(r, x, y, w, secH, "Construction");
    y += std::round(u * 0.5f);
    {
        struct CBtn { const char* label; const char* key; };
        CBtn cbtns[] = {
            {"Factory", "civilian_factory"}, {"Arms", "arms_factory"}, {"Dock", "dockyard"},
            {"Mine", "mine"}, {"Oil", "oil_well"}, {"Refine", "refinery"},
            {"Infra", "infrastructure"}, {"Port", "port"}, {"Destroy", "destroy"}
        };
        int numBtns = 9;
        int cols = 3;
        float cbH = std::round(u * 2.8f);
        float cbGap = std::round(u * 0.3f);
        float cbW = (w - (cols - 1) * cbGap) / cols;
        bool mouseDown = app.input().mouseLeftDown;

        for (int ci = 0; ci < numBtns; ci++) {
            int row = ci / cols;
            int col = ci % cols;
            float cbX = x + col * (cbW + cbGap);
            float cbY = y + row * (cbH + cbGap);
            bool isActive = (currentlyBuilding == cbtns[ci].key);
            bool cbHov = mx >= cbX && mx <= cbX + cbW && my >= cbY && my <= cbY + cbH;
            Color cbBg = isActive ? Color{35, 50, 40} : (cbHov ? Color{30, 33, 42} : Color{18, 20, 26});
            Color cbBrd = isActive ? Theme::gold : Theme::border;
            UIPrim::drawRoundedRect(r, cbBg, cbX, cbY, cbW, cbH, 4, cbBrd);
            if (isActive) UIPrim::drawHLine(r, Theme::gold, cbX + 4, cbX + cbW - 4, cbY + 1, 2);
            int cbFs = std::max(10, (int)(cbH * 0.34f));
            Color cbTc = isActive ? Theme::gold_bright : (cbHov ? Theme::cream : Theme::grey);
            UIPrim::drawText(r, cbtns[ci].label, cbFs, cbX + cbW / 2, cbY + cbH / 2, "center", cbTc, isActive);
            if (cbHov && mouseDown) currentlyBuilding = cbtns[ci].key;
        }
        int totalRows = (numBtns + cols - 1) / cols;
        y += totalRows * (cbH + cbGap) + std::round(u * 0.3f);


        int tipFs = std::max(10, (int)(u * 0.8f));
        BuildingType selBt = buildingTypeFromString(currentlyBuilding);
        float baseCost = (float)buildingBaseCost(selBt);
        float dynamicCost = baseCost * (1.0f + cc->regions.size() / 150.0f);
        int bDays = std::max(1, static_cast<int>(std::ceil(buildingDays(selBt) / std::max(0.01f, constructSpeed))));
        const char* bDesc = buildingDescription(selBt);

        char costBuf[128];
        if (currentlyBuilding == "destroy") {
            snprintf(costBuf, sizeof(costBuf), "Selected: Destroy");
        } else {
            snprintf(costBuf, sizeof(costBuf), "Selected: %s ($%s | %dd)", buildingTypeName(selBt),
                     Helpers::prefixNumber(dynamicCost).c_str(), bDays);
        }
        UIPrim::drawText(r, costBuf, tipFs, x + pad, y, "midleft", Theme::cream);
        y += std::round(u * 1.5f);
        if (currentlyBuilding != "destroy") {
            UIPrim::drawText(r, "Resources: " + formatResourceBundle(buildingResourceCost(selBt)),
                             tipFs, x + pad, y, "midleft", Theme::gold_dim);
            y += std::round(u * 1.5f);
        }
        if (bDesc[0] != '\0') {
            UIPrim::drawText(r, bDesc, tipFs, x + pad, y, "midleft", Theme::grey);
            y += std::round(u * 1.5f);
        }
        UIPrim::drawText(r, "Right-click on owned region to build", tipFs, x + w / 2, y, "topcenter", Theme::dark_grey);
        y += std::round(u * 1.8f);
    }


    y = UIPrim::drawSectionHeader(r, x, y, w, secH, "Buildings");
    y += H * 0.005f;
    const BuildingType bts[] = {BuildingType::CivilianFactory, BuildingType::MilitaryFactory,
        BuildingType::Dockyard, BuildingType::Mine, BuildingType::OilWell,
        BuildingType::Refinery, BuildingType::Infrastructure, BuildingType::Port, BuildingType::Fortress};
    for (int bi = 0; bi < 9; bi++) {
        int count = cc->buildingManager.countAll(bts[bi]);
        if (count > 0) {
            UIPrim::drawText(r, std::string(buildingTypeName(bts[bi])) + ": " + std::to_string(count), fs, x, y, "midleft", Theme::cream);
            y += H * 0.025f;
        }
    }
    y += H * 0.008f;


    y = UIPrim::drawSectionHeader(r, x - pad, y, w + pad * 2, secH, "Construction Queue");
    y += H * 0.005f;
    auto& cQueue = cc->buildingManager.constructionQueue;
    int queueSize = static_cast<int>(cQueue.size());
    if (queueSize == 0) {
        UIPrim::drawText(r, "Queue empty", fs, x, y + H * 0.005f, "midleft", Theme::dark_grey);
        y += row_h;
    } else {
        int showMax = std::min(queueSize, 8);
        for (int qi = 0; qi < showMax; qi++) {
            auto& entry = cQueue[qi];
            std::string itemName = buildingTypeName(entry.type);
            char qbuf[128];
            int estDays = std::max(0, static_cast<int>(std::ceil(entry.daysRemaining / std::max(0.01f, constructSpeed))));
            snprintf(qbuf, sizeof(qbuf), "%s (%dd left)", itemName.c_str(), estDays);
            UIPrim::drawText(r, qbuf, fs, x, y + H * 0.005f, "midleft", Theme::cream);
            y += row_h;
        }
        if (queueSize > showMax) {
            UIPrim::drawText(r, "+" + std::to_string(queueSize - showMax) + " more...", fsSm,
                             x, y + H * 0.005f, "midleft", Theme::dark_grey);
            y += row_h;
        }
    }

    return y;
}

float Sidebar::renderSelfInfo(SDL_Renderer* r, App& app, float x, float y, float w) {
    return renderPoliticalTab(r, app, x, y, w);
}

float Sidebar::renderCountryInfo(SDL_Renderer* r, App& app, float x, float y, float w) {
    return y;
}

float Sidebar::renderForeignTab(SDL_Renderer* r, App& app, float x, float y, float w) {
    auto& gs = app.gameState();
    Country* fc = gs.getCountry(selectedCountry);
    if (!fc) return y;

    int H = Engine::instance().HEIGHT;
    float u = H / 100.0f * Engine::instance().uiScaleFactor();
    int fs = std::max(14, (int)(u * 1.2f));
    int fsLabel = std::max(12, (int)(u * 1.0f));
    float row_h = std::round(u * 2.6f);
    float pad = std::round(u * 0.8f);
    float secH = std::round(u * 3.2f);
    int mx, my; SDL_GetMouseState(&mx, &my);


    std::string displayName = replaceAll(selectedCountry, "_", " ");
    int nameFs = std::max(16, (int)(u * 1.6f));
    UIPrim::drawText(r, displayName, nameFs, x + w / 2, y, "topcenter", Theme::gold_bright, true);
    y += std::round(u * 2.5f);


    std::string lowerName = selectedCountry;
    std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
    SDL_Texture* flagTex = Engine::instance().loadTexture(Engine::instance().assetsPath + "flags/" + lowerName + "_flag.png");
    if (flagTex) {
        int fw, fh; SDL_QueryTexture(flagTex, nullptr, nullptr, &fw, &fh);
        float dispW = std::min(w * 0.60f, std::round(u * 14.0f));
        float dispH = dispW * fh / fw;
        SDL_Rect fDst = {(int)(x + (w - dispW) / 2), (int)y, (int)dispW, (int)dispH};
        UIPrim::drawRectFilled(r, {6, 8, 10}, fDst.x - 2, fDst.y - 2, fDst.w + 4, fDst.h + 4);
        SDL_RenderCopy(r, flagTex, nullptr, &fDst);
        SDL_SetRenderDrawColor(r, Theme::gold_dim.r, Theme::gold_dim.g, Theme::gold_dim.b, 100);
        SDL_RenderDrawRect(r, &fDst);
        y += dispH + std::round(u * 1.5f);
    }

    Country* player = gs.getCountry(gs.controlledCountry);
    std::vector<std::tuple<std::string, std::string, Color>> infoRows;
    infoRows.push_back({"Faction", fc->faction.empty() ? "None" : replaceAll(fc->faction, "_", " "), Theme::cream});
    if (player) {
        bool hasAccess = player->hasLandAccessTo(selectedCountry, gs);
        infoRows.push_back({"Military Access", hasAccess ? "Yes" : "No",
                            hasAccess ? Theme::green_light : Theme::red_light});
    }
    infoRows.push_back({"Ideology", replaceAll(fc->ideologyName, "_", " "), Theme::cream});
    if (!fc->capital.empty()) {
        infoRows.push_back({"Capital", replaceAll(fc->capital, "_", " "), Theme::cream});
    }
    infoRows.push_back({"Population", Helpers::prefixNumber(fc->population), Theme::cream});
    infoRows.push_back({"Regions", std::to_string(fc->regions.size()), Theme::cream});
    int foreignDivisions = 0;
    for (auto& div : fc->divisions) {
        if (div) foreignDivisions += div->divisionStack;
    }
    infoRows.push_back({"Divisions", std::to_string(foreignDivisions), Theme::cream});

    float infoH = row_h * static_cast<float>(infoRows.size()) + pad * 2;
    drawSidebarCard(r, x, y, w, infoH);

    float iy = y + pad;
    float iw = w - pad * 2;
    for (const auto& [label, value, color] : infoRows) {
        UIPrim::drawInfoRow(r, x + pad, iy, iw, row_h, label, value, {0,0,0,0}, color);
        iy += row_h;
    }
    y += infoH + std::round(u * 1.0f);

    if (player) {
        y = UIPrim::drawSectionHeader(r, x, y, w, secH, "Diplomacy");
        y += std::round(u * 0.5f);
        float btnH = std::round(u * 4.0f);
        bool mouseDown = app.input().mouseLeftDown;


        bool atWar = std::find(player->atWarWith.begin(), player->atWarWith.end(), selectedCountry) != player->atWarWith.end();
        if (!atWar) {
            bool hov = mx >= x && mx <= x + w && my >= y && my <= y + btnH;
            UIPrim::drawActionRow(r, x, y, w, btnH, "Declare War", mx, my, hov && mouseDown, "25 PP", "", player->politicalPower >= 25);
            if (hov && mouseDown && player->politicalPower >= 25) {
                player->politicalPower -= 25;
                player->declareWar(selectedCountry, gs);
                Audio::instance().playSound("clickedSound");
            }
            y += btnH + std::round(u * 0.4f);
        }


        bool hasAccess = player->hasLandAccessTo(selectedCountry, gs);
        if (!atWar && !hasAccess) {
            bool hov = mx >= x && mx <= x + w && my >= y && my <= y + btnH;
            UIPrim::drawActionRow(r, x, y, w, btnH, "Get Military Access", mx, my, hov && mouseDown, "50 PP", "", player->politicalPower >= 50);
            if (hov && mouseDown && player->politicalPower >= 50) {
                player->politicalPower -= 50;
                player->militaryAccess.push_back(selectedCountry);
                Audio::instance().playSound("clickedSound");
            }
            y += btnH + std::round(u * 0.4f);
        }


        if (!player->faction.empty() && player->factionLeader && fc->faction.empty() && !atWar) {
            bool hov = mx >= x && mx <= x + w && my >= y && my <= y + btnH;
            UIPrim::drawActionRow(r, x, y, w, btnH, "Invite to Faction", mx, my, hov && mouseDown, "25 PP", "", player->politicalPower >= 25);
            if (hov && mouseDown && player->politicalPower >= 25) {
                player->politicalPower -= 25;

                auto* fac = gs.getFaction(player->faction);
                if (fac) fac->addCountry(selectedCountry, gs);
                Audio::instance().playSound("clickedSound");
            }
            y += btnH + std::round(u * 0.4f);
        }

        y = UIPrim::drawSectionHeader(r, x, y, w, secH, "Trade");
        y += std::round(u * 0.5f);
        static const char* tradeLabels[] = {"Oil", "Steel", "Aluminum", "Tungsten", "Chromium", "Rubber"};
        bool showedTrade = false;
        for (int ri = 0; ri < RESOURCE_COUNT; ++ri) {
            Resource res = static_cast<Resource>(ri);
            float sellerStock = fc->resourceManager.stockpile[ri];
            float playerStock = player->resourceManager.stockpile[ri];
            TradeContract* activeContract = findImportContract(gs, player->name, res);
            bool activeHere = activeContract && activeContract->exporter == selectedCountry;
            float requestedAmount = activeContract && activeHere
                ? activeContract->amount
                : std::clamp(sellerStock * 0.08f, 8.0f, 24.0f);
            TradeApproval approval = evaluateTradeOffer(fc, player, res, requestedAmount, gs);
            bool offerVisible = activeHere || !atWar;
            if (!offerVisible) continue;

            showedTrade = true;
            float amount = activeContract && activeHere ? activeContract->amount : requestedAmount;
            float price = activeContract && activeHere ? activeContract->price : 120.0f;
            float dailyCost = tradeDailyCost(amount, price);
            bool hov = mx >= x && mx <= x + w && my >= y && my <= y + btnH;
            std::string label = activeHere
                ? ("Cancel " + std::string(tradeLabels[ri]) + " Import")
                : ("Import " + std::string(tradeLabels[ri]));
            std::string sublabel = "Seller " + Helpers::resourceValueText(sellerStock) +
                                   " | You " + Helpers::resourceValueText(playerStock);
            if (!activeHere) {
                sublabel += " | Max " + Helpers::resourceValueText(approval.maxAmount);
            }
            std::string costText = activeHere
                ? "Active"
                : (atWar ? "War"
                         : (approval.approved ? ("$" + Helpers::prefixNumber(dailyCost) + "/d") : "Ask"));
            bool enabled = activeHere || !atWar;
            UIPrim::drawActionRow(r, x, y, w, btnH, label, mx, my, hov && mouseDown, costText, sublabel, enabled);
            if (hov && mouseDown && enabled) {
                if (activeHere) {
                    activeContract->cancelWithCleanup(gs);
                    toasts().show("Trade cancelled", 2000);
                } else {
                    if (player->money < dailyCost) {
                        toasts().show("Not enough money for that trade", 2500);
                    } else if (!approval.approved) {
                        toasts().show(replaceAll(selectedCountry, "_", " ") + " declined the trade", 2500);
                    } else {
                        if (activeContract) {
                            activeContract->cancelWithCleanup(gs);
                        }
                        TradeContract contract;
                        contract.exporter = selectedCountry;
                        contract.importer = player->name;
                        contract.resource = res;
                        contract.amount = amount;
                        contract.price = price;
                        gs.tradeContracts.push_back(contract);
                        toasts().show("Trade agreed with " + replaceAll(selectedCountry, "_", " "), 2000);
                    }
                    if (activeContract && approval.approved) {
                        activeContract->cancelWithCleanup(gs);
                    }
                }
                Audio::instance().playSound("clickedSound");
                break;
            }
            y += btnH + std::round(u * 0.4f);
        }

        if (!showedTrade) {
            UIPrim::drawText(r, "No export offers available right now", fsLabel, x + pad, y, "midleft", Theme::grey);
            y += row_h;
        }
    } else {
        auto* gameScreen = dynamic_cast<GameScreen*>(app.screen(ScreenType::GAME));
        bool previewMode = gs.mapViewOnly && gs.controlledCountry.empty();
        float btnH = std::round(u * 4.0f);
        bool mouseDown = app.input().mouseLeftDown;

        y = UIPrim::drawSectionHeader(r, x, y, w, secH, previewMode ? "Start Here" : "Observer Actions");
        y += std::round(u * 0.5f);

        bool hov = mx >= x && mx <= x + w && my >= y && my <= y + btnH;
        std::string displayCountry = replaceAll(selectedCountry, "_", " ");
        std::string actionLabel = previewMode ? "Play As Country" : "Take Control";
        std::string actionValue = previewMode ? displayCountry : "Live switch";
        UIPrim::drawActionRow(r, x, y, w, btnH, actionLabel, mx, my, hov && mouseDown, actionValue);
        if (hov && mouseDown && gameScreen) {
            if (previewMode) {
                gameScreen->startCampaignAsCountry(app, selectedCountry);
            } else {
                gameScreen->takeControlOfCountry(app, selectedCountry);
            }
            return y + btnH;
        }
        y += btnH + std::round(u * 0.4f);

        std::string hint = previewMode
            ? "Starts a full campaign from this country's starting position."
            : "Take control of this country without restarting the world.";
        UIPrim::drawText(r, hint, std::max(11, fsLabel - 1), x + pad, y, "midleft", Theme::grey);
        y += row_h * 1.5f;
    }

    return y;
}

float Sidebar::renderOverlayInfo(SDL_Renderer* r, App& app, float x, float y, float w) {
    auto& gs = app.gameState();
    int H = Engine::instance().HEIGHT;
    float u = H / 100.0f * Engine::instance().uiScaleFactor();
    float pad = std::round(u * 0.8f);
    float rowH = std::round(u * 2.6f);
    float secH = std::round(u * 3.2f);
    int titleFs = std::max(16, (int)(u * 1.55f));
    int bodyFs = std::max(13, (int)(u * 1.0f));

    UIPrim::drawText(r, overlayTitle, titleFs, x + w / 2, y, "topcenter", overlayAccent, true);
    y += std::round(u * 2.5f);

    float cardH = std::max(rowH * std::max<size_t>(3, overlayLines.size()) + pad * 2.0f, std::round(u * 10.0f));
    drawSidebarCard(r, x, y, w, cardH);

    float lineY = y + pad;
    for (const auto& line : overlayLines) {
        UIPrim::drawText(r, line, bodyFs, x + pad, lineY, "midleft", Theme::cream);
        lineY += rowH;
    }

    y += cardH + std::round(u * 1.0f);
    y = UIPrim::drawSectionHeader(r, x, y, w, secH, "Map Overlay");
    y += std::round(u * 0.6f);
    std::string overlayHint;
    if (gs.mapViewOnly && gs.controlledCountry.empty()) {
        overlayHint = "Overlay clicks show province data for this map mode. Switch to Political to choose a country.";
    } else if (gs.controlledCountry.empty()) {
        overlayHint = "The world keeps running while you inspect overlays. Click another province or change map modes.";
    } else {
        overlayHint = "Switch back to the political map to inspect countries directly.";
    }
    UIPrim::drawText(r,
                     overlayHint,
                     std::max(11, bodyFs - 1), x + pad, y, "midleft", Theme::grey);
    return y + rowH;
}
