#include "sidebar/sidebar.h"
#include "core/app.h"
#include "core/engine.h"
#include "core/input.h"
#include "ui/primitives.h"
#include "ui/theme.h"
#include "ui/anim.h"
#include "ui/ui_assets.h"
#include "ui/hoi4.h"
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
    if (fill.r == 24 && fill.g == 25 && fill.b == 28) {
        HOI4::inset(r, x, y, w, h);
        return;
    }
    UIPrim::drawRectFilled(r, fill, x, y, w, h);
    HOI4::frame(r, x, y, w, h, false);
}

void drawSidebarCardTitle(SDL_Renderer* r, float x, float y, const std::string& title, int fontSize) {
    HOI4::sectionLabel(r, x, y, title, fontSize);
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

float Sidebar::getWidth(int screenW) const {
    float w = screenW * sideBarSize_;
    float minW = std::min(320.0f, screenW * 0.45f);
    return std::clamp(w, minW, 640.0f);
}
float Sidebar::getX(int screenW) const {
    float sideW = getWidth(screenW);
    float inv = 1.0f - animation;
    float eased = 1.0f - inv * inv * inv;
    return leftSide ? (-sideW * (1.0f - eased)) : (screenW - sideW * eased);
}

void Sidebar::scroll(float delta) {
    scrollY = std::clamp(scrollY + delta, 0.0f, maxScroll);
}

bool Sidebar::clickAllowed(int mouseY) const {
    return interactive && mouseY >= clipTop_ && mouseY <= clipBottom_;
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

    float topBarH = std::round(std::max(56.0f, u * 5.5f));
    float sideW = getWidth(W);
    float sideX = getX(W);
    float sideY = topBarH;
    float sideH = H - sideY;
    auto& assets = UIAssets::instance();
    float panelX = sideX;
    float panelY = sideY;
    float panelW = sideW;
    float panelH = sideH;

    HOI4::panel(r, panelX, panelY - 2, panelW + 4, panelH + 4);

    int mx, my; SDL_GetMouseState(&mx, &my);
    bool showTabs = showsPlayerTabs(gs);
    float tabH = 0.0f;
    float contentTop = panelY;
    float contentInset = std::round(u * 1.25f);

    if (showTabs) {
        float tabsX = panelX + 2;
        float tabsY = panelY + 2;
        float tabsW = panelW - 2;
        tabH = std::max(40.0f, H * 0.06f);
        float tabW = tabsW / 3.0f;

        const char* tabs[] = {"Political", "Military", "Industry"};
        const char* tabKeys[] = {"political", "military", "industry"};
        for (int i = 0; i < 3; i++) {
            float tx = tabsX + i * tabW;
            float ty = tabsY;
            bool active = (activeTab == tabKeys[i]);
            bool hovered = (mx >= tx && mx <= tx + tabW && my >= ty && my <= ty + tabH);

            HOI4::tab(r, tx, ty, tabW + (i < 2 ? 1.0f : 0.0f), tabH, active, hovered);

            int tabFs = std::max(13, static_cast<int>(tabH * 0.33f));
            Color tabTextColor = active ? Theme::gold_bright : (hovered ? Theme::cream : Theme::grey);
            UIPrim::drawText(r, HOI4::caps(tabs[i]), tabFs, tx + tabW * 0.5f, ty + tabH * 0.52f,
                             "center", tabTextColor, active);
        }
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
        HOI4::sectionHeader(r, headerX, headerY, headerW, headerH, headerTitle);
        if (!overlayTitle.empty()) {
            UIPrim::drawHLine(r, overlayAccent, headerX + u * 0.8f, headerX + headerW - u * 0.8f,
                              headerY + headerH - 2, 2);
        }
        contentTop = headerY + headerH;
    }

    float pad = std::round(H * 0.014f);
    float contentY = contentTop + pad;
    float w = panelW - (contentInset + pad) * 2.0f;
    float x = panelX + contentInset + pad;

    Country* cc = gs.getCountry(gs.controlledCountry);
    float btn_h = std::max(34.0f, H * 0.042f);

    clipTop_ = contentTop;
    clipBottom_ = panelY + panelH;
    SDL_Rect clipRect = {(int)panelX, (int)contentTop,
                         (int)std::ceil(panelW), (int)(panelY + panelH - contentTop)};
    SDL_RenderSetClipRect(r, &clipRect);

    float y = contentY - scrollY;
    float startY = y;

    if (showTabs && cc) {
        bool dtHovered = (mx >= x && mx <= x + w && my >= y && my <= y + btn_h);
        bool dtClicked = dtHovered && clickAllowed(my) && app.input().mouseLeftDown;
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
    } else if (cc) {
        if (activeTab == "political") y = renderPoliticalTab(r, app, x, y, w);
        else if (activeTab == "military") y = renderMilitaryTab(r, app, x, y, w);
        else if (activeTab == "industry") y = renderIndustryTab(r, app, x, y, w);
    }

    SDL_RenderSetClipRect(r, nullptr);

    float visibleH = panelY + panelH - contentTop;
    float contentH = (y - startY) + pad * 2.0f;
    maxScroll = std::max(0.0f, contentH - visibleH);
    scrollY = std::clamp(scrollY, 0.0f, maxScroll);

    if (maxScroll > 0.0f) {
        float trackX = panelX + panelW - 5.0f;
        float trackY = contentTop + 2.0f;
        float trackH = visibleH - 4.0f;
        float thumbH = std::max(24.0f, trackH * (visibleH / (visibleH + maxScroll)));
        float thumbY = trackY + (trackH - thumbH) * (scrollY / maxScroll);
        UIPrim::drawRectFilled(r, {0, 0, 0, 90}, trackX, trackY, 3.0f, trackH);
        UIPrim::drawRectFilled(r, {Theme::gold_dim.r, Theme::gold_dim.g, Theme::gold_dim.b, 170},
                               trackX, thumbY, 3.0f, thumbH);
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
    float titleH = (float)UIPrim::textHeight(fsLabel);
    float valueH = (float)UIPrim::textHeight(fsValue);
    float smallH = (float)UIPrim::textHeight(fsSmall);
    float cardHeaderH = cardPad + titleH + std::round(gap * 0.5f);
    float infoH = cardHeaderH + valueH + cardPad;
    float infoRowH = std::max(std::round(u * 2.35f), valueH + 8.0f);
    float colW = (w - gap) * 0.5f;
    int mx, my; SDL_GetMouseState(&mx, &my);

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

    auto animatedCount = [](const std::string& key, float target) {
        return std::to_string((long long)std::llround(UIAnim::countTo(key, target)));
    };

    auto drawInfoCard = [&](float cx, float cy, float cw, float ch,
                            const std::string& title, const std::string& value, Color valueColor) {
        float innerW = cw - cardPad * 2.0f;
        drawSidebarCard(r, cx, cy, cw, ch);
        drawSidebarCardTitle(r, cx + cardPad, cy + cardPad, UIPrim::fitText(title, fsLabel, innerW), fsLabel);
        UIPrim::drawText(r, UIPrim::fitText(value, fsValue, innerW), fsValue,
                         cx + cardPad, cy + cardHeaderH, "topleft", valueColor, false);
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

    float lineGap = std::round(u * 0.3f);
    float textStackH = valueH + smallH * 3.0f + lineGap * 3.0f;
    float previewSize = std::max(std::round(u * 5.5f), textStackH);
    float profileH = cardHeaderH + previewSize + cardPad;
    drawSidebarCard(r, x, y, w, profileH);
    drawSidebarCardTitle(r, x + cardPad, y + cardPad, "National Profile", fsLabel);

    float previewX = x + cardPad;
    float previewY = y + cardHeaderH;
    float textX = previewX + previewSize + gap;
    float textMax = x + w - cardPad - textX;
    std::string lowerName = gs.controlledCountry;
    std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
    SDL_Texture* flagTex = Engine::instance().loadTexture(Engine::instance().assetsPath + "flags/" + lowerName + "_flag.png");
    drawSidebarCard(r, previewX, previewY, previewSize, previewSize, {12, 14, 18, 255}, {78, 70, 44, 160}, 6);
    if (flagTex) {
        int fw, fh;
        SDL_QueryTexture(flagTex, nullptr, nullptr, &fw, &fh);
        float availSz = previewSize - 8.0f;
        float fscale = std::min(availSz / fw, availSz / fh);
        float dw = fw * fscale, dh = fh * fscale;
        SDL_Rect dst = {(int)(previewX + (previewSize - dw) * 0.5f),
                        (int)(previewY + (previewSize - dh) * 0.5f), (int)dw, (int)dh};
        SDL_RenderCopy(r, flagTex, nullptr, &dst);
    } else {
        float half = previewSize * 0.5f;
        UIPrim::drawRectFilled(r, {160, 78, 78}, previewX + 4, previewY + 4, half - 4, half - 4);
        UIPrim::drawRectFilled(r, {65, 118, 185}, previewX + half, previewY + 4, half - 4, half - 4);
        UIPrim::drawRectFilled(r, {82, 160, 82}, previewX + 4, previewY + half, half - 4, half - 4);
        UIPrim::drawRectFilled(r, {136, 108, 178}, previewX + half, previewY + half, half - 4, half - 4);
    }

    auto [profileLine1, profileLine2] = countryProfileLines(*cc);
    float lineY = previewY;
    UIPrim::drawText(r, UIPrim::fitText(ideology, fsValue, textMax), fsValue, textX, lineY, "topleft", Theme::cream, true);
    lineY += valueH + lineGap;
    UIPrim::drawText(r, UIPrim::fitText(profileLine1, fsSmall, textMax), fsSmall, textX, lineY, "topleft", Theme::grey);
    lineY += smallH + lineGap;
    UIPrim::drawText(r, UIPrim::fitText(profileLine2, fsSmall, textMax), fsSmall, textX, lineY, "topleft", Theme::grey);
    lineY += smallH + lineGap;

    int puppetCount = 0;
    for (auto& ps : gs.puppetStates) {
        if (ps.active && ps.overlord == gs.controlledCountry) puppetCount++;
    }
    std::string profileMeta = cc->puppetTo.empty()
        ? ("Focus: " + focusStr)
        : ("Subject of " + replaceAll(cc->puppetTo, "_", " "));
    UIPrim::drawText(r, UIPrim::fitText(profileMeta, fsSmall, textMax), fsSmall, textX, lineY, "topleft",
                     cc->puppetTo.empty() ? Theme::gold_dim : Theme::red_light);
    y += profileH + gap;

    float statsH = cardHeaderH + infoRowH * 6.0f + cardPad;
    drawSidebarCard(r, x, y, w, statsH);
    drawSidebarCardTitle(r, x + cardPad, y + cardPad, "Statistics", fsLabel);
    float statsTop = y + cardHeaderH;
    float statsW = w - cardPad * 2.0f;
    UIPrim::drawInfoRow(r, x + cardPad, statsTop, statsW, infoRowH, "Population",
                        Helpers::prefixNumber(UIAnim::countTo("sb:pop", cc->population))); statsTop += infoRowH;
    UIPrim::drawInfoRow(r, x + cardPad, statsTop, statsW, infoRowH, "Stability", stabilityLabel(cc->stability),
                        {0,0,0,0}, cc->stability >= 55.0f ? Theme::green_light : Theme::red_light); statsTop += infoRowH;
    UIPrim::drawInfoRow(r, x + cardPad, statsTop, statsW, infoRowH, "Political Power",
                        animatedCount("sb:pp", cc->politicalPower),
                        {0,0,0,0}, Theme::gold_bright); statsTop += infoRowH;
    UIPrim::drawInfoRow(r, x + cardPad, statsTop, statsW, infoRowH, "Regions",
                        animatedCount("sb:regions", (float)cc->regions.size())); statsTop += infoRowH;
    UIPrim::drawInfoRow(r, x + cardPad, statsTop, statsW, infoRowH, "At War With", std::to_string(cc->atWarWith.size()),
                        {0,0,0,0}, cc->atWarWith.empty() ? Theme::grey : Theme::red_light); statsTop += infoRowH;
    UIPrim::drawInfoRow(r, x + cardPad, statsTop, statsW, infoRowH, "Puppets", std::to_string(puppetCount),
                        {0,0,0,0}, puppetCount > 0 ? Theme::green_light : Theme::grey);
    y += statsH + gap;

    float atkVal = std::round(cc->combatStats.attack * cc->attackMultiplier * 10.0f) / 10.0f;
    float defVal = std::round(cc->combatStats.defense * cc->defenseMultiplier * 10.0f) / 10.0f;
    float combatH = cardHeaderH + infoRowH * 5.0f + cardPad;
    drawSidebarCard(r, x, y, w, combatH);
    drawSidebarCardTitle(r, x + cardPad, y + cardPad, "Combat Stats", fsLabel);
    float combatTop = y + cardHeaderH;
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
    float catH = std::max(std::round(u * 2.8f), titleH + 12.0f);
    bool mouseDown = clickAllowed(my) && app.input().mouseLeftDown;
    std::map<std::string, std::vector<PoliticalOption*>> grouped;
    for (auto& opt : options) {
        grouped[opt.category].push_back(&opt);
    }

    static std::string openCategory;
    float actionsH = cardHeaderH + cardPad;
    for (auto& [category, catOpts] : grouped) {
        actionsH += catH + gap * 0.45f;
        if (openCategory == category) {
            actionsH += catOpts.size() * (actH + gap * 0.35f);
        }
    }

    drawSidebarCard(r, x, y, w, actionsH);
    drawSidebarCardTitle(r, x + cardPad, y + cardPad, "Political Actions", fsLabel);
    float actY = y + cardHeaderH;

    for (auto& [category, catOpts] : grouped) {
        std::string catTitle = category.empty() ? "General" : category;
        if (!catTitle.empty()) catTitle[0] = std::toupper(catTitle[0]);
        bool catHover = mx >= x + cardPad && mx <= x + w - cardPad && my >= actY && my <= actY + catH;
        HOI4::button(r, x + cardPad, actY, w - cardPad * 2.0f, catH, catHover, false, true);
        UIPrim::drawText(r, UIPrim::fitText(catTitle, fsLabel, w - cardPad * 4.5f), fsLabel,
                         x + cardPad * 1.9f, actY + catH * 0.5f, "midleft",
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

    int fsLabel = std::max(11, (int)(u * 0.86f));
    int fsValue = std::max(14, (int)(u * 1.1f));
    float gap = std::round(u * 0.8f);
    float cardPad = std::round(u * 0.9f);
    float titleH = (float)UIPrim::textHeight(fsLabel);
    float valueH = (float)UIPrim::textHeight(fsValue);
    float cardHeaderH = cardPad + titleH + std::round(gap * 0.5f);
    float infoRowH = std::max(std::round(u * 2.35f), valueH + 8.0f);
    float actH = std::round(u * 4.2f);
    float actGap = std::round(gap * 0.35f);
    int mx, my; SDL_GetMouseState(&mx, &my);
    bool mouseDown = clickAllowed(my) && app.input().mouseLeftDown;

    float totalStacks = 0;
    for (auto& d : cc->divisions) totalStacks += d->divisionStack;
    float dailyUp = totalStacks * gs.DIVISION_UPKEEP_PER_DAY;
    int armsC = cc->buildingManager.countAll(BuildingType::MilitaryFactory);
    int tDays = std::max(3, (int)(14 * std::max(0.25f, 1.0f - armsC * 0.15f)));
    float trainScale = 1.0f + static_cast<float>(cc->divisions.size()) * 0.05f;
    float trainDiscount = std::max(0.5f, 1.0f - armsC * 0.05f);
    float trainOneMoney = gs.TRAINING_COST_PER_DIV * trainScale * trainDiscount;
    auto trainOneBundle = Country::trainingCostBundle(1);
    std::string deployCity = cc->deployRegion.empty() ? "None" : replaceAll(cc->deployRegion, "_", " ");
    char buf[128];

    float armyH = cardHeaderH + infoRowH * 5.0f + cardPad;
    drawSidebarCard(r, x, y, w, armyH);
    drawSidebarCardTitle(r, x + cardPad, y + cardPad, "Army", fsLabel);
    float iy = y + cardHeaderH;
    float iw = w - cardPad * 2.0f;
    UIPrim::drawInfoRow(r, x + cardPad, iy, iw, infoRowH, "Service", Helpers::getMilitarySizeName(cc->militarySize)); iy += infoRowH;
    UIPrim::drawInfoRow(r, x + cardPad, iy, iw, infoRowH, "Deployment", deployCity); iy += infoRowH;
    snprintf(buf, sizeof(buf), "$%s/day", Helpers::prefixNumber(UIAnim::countTo("sb:upkeep", dailyUp)).c_str());
    UIPrim::drawInfoRow(r, x + cardPad, iy, iw, infoRowH, "Upkeep", buf, {0,0,0,0}, Theme::orange); iy += infoRowH;
    snprintf(buf, sizeof(buf), "$%s | %dd", Helpers::prefixNumber(trainOneMoney).c_str(), tDays);
    UIPrim::drawInfoRow(r, x + cardPad, iy, iw, infoRowH, "Train Cost", buf); iy += infoRowH;
    UIPrim::drawInfoRow(r, x + cardPad, iy, iw, infoRowH, "Train Res.", formatResourceBundle(trainOneBundle));
    y += armyH + gap;

    float recH = cardHeaderH + actH * 3.0f + actGap * 2.0f + cardPad;
    drawSidebarCard(r, x, y, w, recH);
    drawSidebarCardTitle(r, x + cardPad, y + cardPad, "Recruitment", fsLabel);
    float ry = y + cardHeaderH;
    float rowX = x + cardPad;
    float rowW = w - cardPad * 2.0f;

    bool cdHov = mx >= rowX && mx <= rowX + rowW && my >= ry && my <= ry + actH;
    UIPrim::drawActionRow(r, rowX, ry, rowW, actH, "Change Deployment", mx, my, cdHov && mouseDown,
                          deployCity, "Cycle deployment city");
    if (cdHov && mouseDown) {
        cc->changeDeployment(gs);
        Audio::instance().playSound("clickedSound");
    }
    ry += actH + actGap;

    bool canTrain1 = (cc->money >= trainOneMoney && cc->manPower >= 10000 &&
                      canAffordResourceBundle(cc, trainOneBundle));
    bool t1Hov = mx >= rowX && mx <= rowX + rowW && my >= ry && my <= ry + actH;
    UIPrim::drawActionRow(r, rowX, ry, rowW, actH, "Train 1 Division", mx, my, t1Hov && mouseDown,
                          "$" + Helpers::prefixNumber(trainOneMoney),
                          "10k MP | " + std::to_string(tDays) + "d | " + formatResourceBundle(trainOneBundle), canTrain1);
    if (t1Hov && mouseDown) {
        if (canTrain1) {
            cc->trainDivision(gs, 1);
            Audio::instance().playSound("clickedSound");
        } else {
            Audio::instance().playSound("failedClickSound");
        }
    }
    ry += actH + actGap;

    int allDiv = (int)(cc->manPower / 10000);
    if (trainOneMoney > 0.0f) {
        allDiv = std::min(allDiv, (int)(cc->money / trainOneMoney));
    }
    auto oneBundle = Country::trainingCostBundle(1);
    for (int ri = 0; ri < RESOURCE_COUNT; ++ri) {
        if (oneBundle[ri] > 0.001f) {
            allDiv = std::min(allDiv, (int)(cc->resourceManager.stockpile[ri] / oneBundle[ri]));
        }
    }
    allDiv = std::max(0, allDiv);
    float trainAllMoney = trainOneMoney * allDiv;
    auto trainAllBundle = Country::trainingCostBundle(std::max(0, allDiv));
    bool canTrainAll = (allDiv > 0 && cc->money >= trainAllMoney &&
                        canAffordResourceBundle(cc, trainAllBundle));
    snprintf(buf, sizeof(buf), "Train All (%d)", allDiv);
    bool tAllHov = mx >= rowX && mx <= rowX + rowW && my >= ry && my <= ry + actH;
    UIPrim::drawActionRow(r, rowX, ry, rowW, actH, buf, mx, my, tAllHov && mouseDown,
                          "$" + Helpers::prefixNumber(trainAllMoney),
                          Helpers::prefixNumber(allDiv * 10000.0f) + " MP | " + formatResourceBundle(trainAllBundle), canTrainAll);
    if (tAllHov && mouseDown) {
        if (canTrainAll) {
            cc->trainDivision(gs, allDiv);
            Audio::instance().playSound("clickedSound");
        } else {
            Audio::instance().playSound("failedClickSound");
        }
    }
    y += recH + gap;

    int trainShown = std::min((int)cc->training.size(), 6);
    int trainExtra = ((int)cc->training.size() > 6) ? 1 : 0;
    float trainBodyH = cc->training.empty()
        ? infoRowH
        : trainShown * (actH + actGap) - actGap + trainExtra * infoRowH;
    float trainingH = cardHeaderH + trainBodyH + cardPad;
    drawSidebarCard(r, x, y, w, trainingH);
    drawSidebarCardTitle(r, x + cardPad, y + cardPad, "Training", fsLabel);
    float ty = y + cardHeaderH;
    if (cc->training.empty()) {
        UIPrim::drawText(r, "No units in training", fsValue, x + cardPad, ty + infoRowH * 0.5f, "midleft", Theme::dark_grey);
    } else {
        for (int i = 0; i < trainShown; i++) {
            bool ready = (cc->training[i][1] <= 0);
            auto deployBundle = Country::deploymentCostBundle(cc->training[i][0]);
            bool canDeploy = ready && canAffordResourceBundle(cc, deployBundle);
            std::string label = ready
                ? "Deploy " + std::to_string(cc->training[i][0]) + " Division" + (cc->training[i][0] > 1 ? "s" : "")
                : "Training " + std::to_string(cc->training[i][0]) + " Division" + (cc->training[i][0] > 1 ? "s" : "");
            bool trHov = canDeploy && mx >= rowX && mx <= rowX + rowW && my >= ty && my <= ty + actH;
            UIPrim::drawActionRow(r, rowX, ty, rowW, actH, label, mx, my, trHov && mouseDown,
                                  ready ? (canDeploy ? "Ready" : "Need stock") : (std::to_string(cc->training[i][1]) + "d"),
                                  "Deploy: " + formatResourceBundle(deployBundle), canDeploy);
            if (trHov && mouseDown) {
                if (cc->deployTrainingDivision(gs, i)) {
                    Audio::instance().playSound("clickedSound");
                } else {
                    Audio::instance().playSound("failedClickSound");
                }
                break;
            }
            ty += actH + actGap;
        }
        if (trainExtra) {
            UIPrim::drawText(r, "+" + std::to_string(cc->training.size() - 6) + " more in queue", fsLabel,
                             x + cardPad, ty + infoRowH * 0.5f, "midleft", Theme::grey);
        }
    }
    y += trainingH + gap;

    float totalTroops = 0.0f;
    for (auto& d : cc->divisions) {
        if (d) totalTroops += d->units;
    }
    float divH = cardHeaderH + infoRowH * 3.0f + cardPad;
    drawSidebarCard(r, x, y, w, divH);
    drawSidebarCardTitle(r, x + cardPad, y + cardPad, "Divisions", fsLabel);
    iy = y + cardHeaderH;
    UIPrim::drawInfoRow(r, x + cardPad, iy, iw, infoRowH, "Divisions",
                        std::to_string(std::max(0, (int)std::round(UIAnim::countTo("sb:divs", totalStacks))))); iy += infoRowH;
    UIPrim::drawInfoRow(r, x + cardPad, iy, iw, infoRowH, "Armies", std::to_string(cc->divisions.size())); iy += infoRowH;
    UIPrim::drawInfoRow(r, x + cardPad, iy, iw, infoRowH, "Troops",
                        Helpers::prefixNumber(UIAnim::countTo("sb:troops", totalTroops)));
    y += divH;

    return y;
}

float Sidebar::renderIndustryTab(SDL_Renderer* r, App& app, float x, float y, float w) {
    auto& gs = app.gameState();
    Country* cc = gs.getCountry(gs.controlledCountry);
    if (!cc) return y;
    int H = Engine::instance().HEIGHT;
    float u = H / 100.0f * Engine::instance().uiScaleFactor();

    int fsLabel = std::max(11, (int)(u * 0.86f));
    int fsValue = std::max(14, (int)(u * 1.1f));
    int fsSmall = std::max(10, (int)(u * 0.78f));
    float gap = std::round(u * 0.8f);
    float cardPad = std::round(u * 0.9f);
    float titleH = (float)UIPrim::textHeight(fsLabel);
    float valueH = (float)UIPrim::textHeight(fsValue);
    float smallH = (float)UIPrim::textHeight(fsSmall);
    float cardHeaderH = cardPad + titleH + std::round(gap * 0.5f);
    float infoRowH = std::max(std::round(u * 2.35f), valueH + 8.0f);
    float constructSpeed = cc->buildingManager.getConstructionSpeed(cc) * cc->resourceManager.getProductionPenalty();
    int mx, my; SDL_GetMouseState(&mx, &my);
    bool mouseDown = clickAllowed(my) && app.input().mouseLeftDown;

    int civC = cc->buildingManager.countAll(BuildingType::CivilianFactory);
    float dailyIncome = 5000.0f * (civC + 1) * cc->moneyMultiplier;
    float totalStacks = 0;
    for (auto& d : cc->divisions) totalStacks += d->divisionStack;
    float dailyUpkeep = totalStacks * gs.DIVISION_UPKEEP_PER_DAY;
    float net = dailyIncome - dailyUpkeep;
    Color nc = net >= 0 ? Theme::green_light : Theme::red_light;
    auto& rm = cc->resourceManager;

    float econH = cardHeaderH + infoRowH * 3.0f + cardPad;
    drawSidebarCard(r, x, y, w, econH);
    drawSidebarCardTitle(r, x + cardPad, y + cardPad, "Economy", fsLabel);
    float iy = y + cardHeaderH;
    float iw = w - cardPad * 2.0f;
    UIPrim::drawInfoRow(r, x + cardPad, iy, iw, infoRowH, "Income",
                        "$" + Helpers::prefixNumber(UIAnim::countTo("sb:income", dailyIncome)) + "/d", {0,0,0,0}, Theme::green_light); iy += infoRowH;
    UIPrim::drawInfoRow(r, x + cardPad, iy, iw, infoRowH, "Upkeep",
                        "-$" + Helpers::prefixNumber(UIAnim::countTo("sb:iupkeep", dailyUpkeep)) + "/d", {0,0,0,0}, Theme::orange); iy += infoRowH;
    UIPrim::drawInfoRow(r, x + cardPad, iy, iw, infoRowH, "Net",
                        "$" + Helpers::prefixNumber(UIAnim::countTo("sb:net", net)) + "/d", {0,0,0,0}, nc);
    y += econH + gap;

    float prodPen = rm.getProductionPenalty();
    float combatPen = rm.getCombatPenalty();
    int penaltyRows = (prodPen < 1.0f ? 1 : 0) + (combatPen < 1.0f ? 1 : 0);
    const char* resNames[] = {"Oil", "Steel", "Aluminum", "Tungsten", "Chromium", "Rubber"};
    float resH = cardHeaderH + infoRowH * (6.0f + penaltyRows) + cardPad;
    drawSidebarCard(r, x, y, w, resH);
    drawSidebarCardTitle(r, x + cardPad, y + cardPad, "Resources", fsLabel);
    iy = y + cardHeaderH;
    for (int ri = 0; ri < RESOURCE_COUNT && ri < 6; ri++) {
        float prod = rm.production[ri], cons = rm.consumption[ri];
        float imp = rm.tradeImports[ri], exp = rm.tradeExports[ri];
        float stk = UIAnim::countTo("sb:res" + std::to_string(ri), rm.stockpile[ri]);
        float netR = prod + imp - cons - exp;
        Color rc = netR >= 0 ? Theme::green_light : Theme::red_light;
        char vbuf[64];
        snprintf(vbuf, sizeof(vbuf), "%s  (%+.1f/d)", Helpers::resourceValueText(stk).c_str(), netR);
        UIPrim::drawInfoRow(r, x + cardPad, iy, iw, infoRowH, resNames[ri], vbuf, {0,0,0,0}, rc);
        iy += infoRowH;
    }
    if (prodPen < 1.0f) {
        char penBuf[32];
        snprintf(penBuf, sizeof(penBuf), "-%.0f%%", (1.0f - prodPen) * 100.0f);
        UIPrim::drawInfoRow(r, x + cardPad, iy, iw, infoRowH, "Production Penalty", penBuf, {0,0,0,0}, Theme::orange);
        iy += infoRowH;
    }
    if (combatPen < 1.0f) {
        char penBuf[32];
        snprintf(penBuf, sizeof(penBuf), "-%.0f%%", (1.0f - combatPen) * 100.0f);
        UIPrim::drawInfoRow(r, x + cardPad, iy, iw, infoRowH, "Combat Penalty", penBuf, {0,0,0,0}, Theme::red_light);
        iy += infoRowH;
    }
    y += resH + gap;

    struct CBtn { const char* label; const char* key; };
    CBtn cbtns[] = {
        {"Factory", "civilian_factory"}, {"Arms", "arms_factory"}, {"Dock", "dockyard"},
        {"Mine", "mine"}, {"Oil", "oil_well"}, {"Refine", "refinery"},
        {"Infra", "infrastructure"}, {"Port", "port"}, {"Destroy", "destroy"}
    };
    float cbH = std::round(u * 2.8f);
    float cbGap = std::round(u * 0.3f);
    float lineStep = smallH + std::round(u * 0.25f);
    BuildingType selBt = buildingTypeFromString(currentlyBuilding);
    const char* bDesc = buildingDescription(selBt);
    int infoLines = (currentlyBuilding == "destroy" ? 1 : 2) + (bDesc[0] != '\0' ? 1 : 0) + 1;
    float gridH = 3.0f * cbH + 2.0f * cbGap;
    float conH = cardHeaderH + gridH + std::round(u * 0.5f) + infoLines * lineStep + cardPad;
    drawSidebarCard(r, x, y, w, conH);
    drawSidebarCardTitle(r, x + cardPad, y + cardPad, "Construction", fsLabel);
    float gy = y + cardHeaderH;
    float gridW = w - cardPad * 2.0f;
    float cbW = (gridW - 2.0f * cbGap) / 3.0f;
    for (int ci = 0; ci < 9; ci++) {
        int row = ci / 3;
        int col = ci % 3;
        float cbX = x + cardPad + col * (cbW + cbGap);
        float cbY = gy + row * (cbH + cbGap);
        bool isActive = (currentlyBuilding == cbtns[ci].key);
        bool cbHov = mx >= cbX && mx <= cbX + cbW && my >= cbY && my <= cbY + cbH;
        HOI4::button(r, cbX, cbY, cbW, cbH, cbHov, false, true);
        if (isActive) {
            SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(r, Theme::gold.r, Theme::gold.g, Theme::gold.b, 230);
            SDL_Rect activeRim = {(int)cbX, (int)cbY, (int)cbW, (int)cbH};
            SDL_RenderDrawRect(r, &activeRim);
        }
        int cbFs = std::max(10, (int)(cbH * 0.34f));
        Color cbTc = isActive ? Theme::gold_bright : (cbHov ? Theme::cream : Theme::grey);
        UIPrim::drawText(r, UIPrim::fitText(cbtns[ci].label, cbFs, cbW - 8.0f), cbFs,
                         cbX + cbW / 2, cbY + cbH / 2, "center", cbTc, isActive);
        if (cbHov && mouseDown) currentlyBuilding = cbtns[ci].key;
    }
    float infoY = gy + gridH + std::round(u * 0.5f);
    float infoMaxW = w - cardPad * 2.0f;
    float dynamicCost = (float)buildingBaseCost(selBt) * (1.0f + cc->regions.size() / 150.0f);
    int bDays = std::max(1, static_cast<int>(std::ceil(buildingDays(selBt) / std::max(0.01f, constructSpeed))));
    char costBuf[128];
    if (currentlyBuilding == "destroy") {
        snprintf(costBuf, sizeof(costBuf), "Selected: Destroy");
    } else {
        snprintf(costBuf, sizeof(costBuf), "Selected: %s  ($%s | %dd)", buildingTypeName(selBt),
                 Helpers::prefixNumber(dynamicCost).c_str(), bDays);
    }
    UIPrim::drawText(r, UIPrim::fitText(costBuf, fsSmall, infoMaxW), fsSmall, x + cardPad, infoY, "topleft", Theme::cream);
    infoY += lineStep;
    if (currentlyBuilding != "destroy") {
        UIPrim::drawText(r, UIPrim::fitText("Cost: " + formatResourceBundle(buildingResourceCost(selBt)), fsSmall, infoMaxW),
                         fsSmall, x + cardPad, infoY, "topleft", Theme::grey);
        infoY += lineStep;
    }
    if (bDesc[0] != '\0') {
        UIPrim::drawText(r, UIPrim::fitText(bDesc, fsSmall, infoMaxW), fsSmall, x + cardPad, infoY, "topleft", Theme::grey);
        infoY += lineStep;
    }
    UIPrim::drawText(r, "Right-click an owned region to build", fsSmall, x + cardPad, infoY, "topleft", Theme::dark_grey);
    y += conH + gap;

    const BuildingType bts[] = {BuildingType::CivilianFactory, BuildingType::MilitaryFactory,
        BuildingType::Dockyard, BuildingType::Mine, BuildingType::OilWell,
        BuildingType::Refinery, BuildingType::Infrastructure, BuildingType::Port, BuildingType::Fortress};
    int builtTypes = 0;
    for (int bi = 0; bi < 9; bi++) {
        if (cc->buildingManager.countAll(bts[bi]) > 0) builtTypes++;
    }
    float bldH = cardHeaderH + infoRowH * std::max(1, builtTypes) + cardPad;
    drawSidebarCard(r, x, y, w, bldH);
    drawSidebarCardTitle(r, x + cardPad, y + cardPad, "Buildings", fsLabel);
    iy = y + cardHeaderH;
    if (builtTypes == 0) {
        UIPrim::drawText(r, "Nothing built yet", fsValue, x + cardPad, iy + infoRowH * 0.5f, "midleft", Theme::dark_grey);
    } else {
        for (int bi = 0; bi < 9; bi++) {
            int count = cc->buildingManager.countAll(bts[bi]);
            if (count <= 0) continue;
            UIPrim::drawInfoRow(r, x + cardPad, iy, iw, infoRowH, buildingTypeName(bts[bi]), std::to_string(count));
            iy += infoRowH;
        }
    }
    y += bldH + gap;

    auto& cQueue = cc->buildingManager.constructionQueue;
    int queueSize = static_cast<int>(cQueue.size());
    int showMax = std::min(queueSize, 8);
    int queueRows = std::max(1, showMax + (queueSize > showMax ? 1 : 0));
    float queueH = cardHeaderH + infoRowH * queueRows + cardPad;
    drawSidebarCard(r, x, y, w, queueH);
    drawSidebarCardTitle(r, x + cardPad, y + cardPad, "Construction Queue", fsLabel);
    iy = y + cardHeaderH;
    if (queueSize == 0) {
        UIPrim::drawText(r, "Queue empty", fsValue, x + cardPad, iy + infoRowH * 0.5f, "midleft", Theme::dark_grey);
    } else {
        for (int qi = 0; qi < showMax; qi++) {
            auto& entry = cQueue[qi];
            int estDays = std::max(0, static_cast<int>(std::ceil(entry.daysRemaining / std::max(0.01f, constructSpeed))));
            UIPrim::drawInfoRow(r, x + cardPad, iy, iw, infoRowH, buildingTypeName(entry.type),
                                std::to_string(estDays) + "d left");
            iy += infoRowH;
        }
        if (queueSize > showMax) {
            UIPrim::drawText(r, "+" + std::to_string(queueSize - showMax) + " more queued", fsLabel,
                             x + cardPad, iy + infoRowH * 0.5f, "midleft", Theme::grey);
        }
    }
    y += queueH;

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
        float btnH = std::round(u * 4.0f);
        bool mouseDown = clickAllowed(my) && app.input().mouseLeftDown;
        bool atWar = std::find(player->atWarWith.begin(), player->atWarWith.end(), selectedCountry) != player->atWarWith.end();
        bool hasAccess = player->hasLandAccessTo(selectedCountry, gs);
        bool canInvite = !player->faction.empty() && player->factionLeader && fc->faction.empty() && !atWar;

        bool anyFriendly = (!atWar && !hasAccess) || canInvite;
        if (anyFriendly) {
            y = UIPrim::drawSectionHeader(r, x, y, w, secH, "Friendly Options");
            y += std::round(u * 0.5f);
        }

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

        if (canInvite) {
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

        if (anyFriendly) {
            y += std::round(u * 0.4f);
        }

        if (!atWar) {
            y = UIPrim::drawSectionHeader(r, x, y, w, secH, "Aggressive Options");
            y += std::round(u * 0.5f);
            bool hov = mx >= x && mx <= x + w && my >= y && my <= y + btnH;
            UIPrim::drawActionRow(r, x, y, w, btnH, "Declare War", mx, my, hov && mouseDown, "25 PP", "", player->politicalPower >= 25);
            if (hov && mouseDown && player->politicalPower >= 25) {
                player->politicalPower -= 25;
                player->declareWar(selectedCountry, gs);
                Audio::instance().playSound("clickedSound");
            }
            y += btnH + std::round(u * 0.8f);
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
        bool mouseDown = clickAllowed(my) && app.input().mouseLeftDown;

        y = UIPrim::drawSectionHeader(r, x, y, w, secH, previewMode ? "Start Here" : "Observer Actions");
        y += std::round(u * 0.5f);

        bool hov = mx >= x && mx <= x + w && my >= y && my <= y + btnH;
        std::string displayCountry = replaceAll(selectedCountry, "_", " ");
        std::string actionLabel = previewMode ? "Play As Country" : "Take Control";
        std::string actionValue = previewMode ? displayCountry : "Live switch";
        UIPrim::drawActionRow(r, x, y, w, btnH, actionLabel, mx, my, hov && mouseDown, actionValue);
        if (hov && mouseDown && gameScreen) {
            gameScreen->requestTakeControl(selectedCountry, previewMode);
            return y + btnH;
        }
        y += btnH + std::round(u * 0.4f);

        std::string hint = previewMode
            ? "Starts a full campaign from this country's starting position."
            : "Take control of this country without restarting the world.";
        y = UIPrim::drawTextWrapped(r, hint, std::max(11, fsLabel - 1),
                                    x + pad, y, w - pad * 2.0f, Theme::grey);
        y += row_h * 0.5f;
    }

    return y;
}

float Sidebar::renderOverlayInfo(SDL_Renderer* r, App& app, float x, float y, float w) {
    auto& gs = app.gameState();
    int H = Engine::instance().HEIGHT;
    float u = H / 100.0f * Engine::instance().uiScaleFactor();
    float pad = std::round(u * 0.9f);
    int titleFs = Theme::fsTitle(u);
    int bodyFs = Theme::fsBody(u);
    int capFs = Theme::fsCaption(u);
    float titleH = (float)UIPrim::textHeight(titleFs);
    float lineH = (float)UIPrim::textHeight(bodyFs);
    float lineGap = std::round(u * 0.55f);

    UIPrim::drawText(r, UIPrim::fitText(overlayTitle, titleFs, w), titleFs,
                     x + w / 2, y, "topcenter", overlayAccent, true);
    y += titleH + std::round(u * 0.8f);

    size_t lineCount = std::max<size_t>(1, overlayLines.size());
    float cardH = pad * 2.0f + lineCount * lineH + (lineCount - 1) * lineGap;
    drawSidebarCard(r, x, y, w, cardH);

    float lineY = y + pad;
    for (const auto& line : overlayLines) {
        UIPrim::drawText(r, UIPrim::fitText(line, bodyFs, w - pad * 2.0f), bodyFs,
                         x + pad, lineY, "topleft", Theme::cream);
        lineY += lineH + lineGap;
    }

    y += cardH + std::round(u * 1.0f);
    std::string overlayHint;
    if (gs.mapViewOnly && gs.controlledCountry.empty()) {
        overlayHint = "Overlay clicks show province data for this map mode. Switch to Political to choose a country.";
    } else if (gs.controlledCountry.empty()) {
        overlayHint = "The world keeps running while you inspect overlays. Click another province or change map modes.";
    } else {
        overlayHint = "Switch back to the political map to inspect countries directly.";
    }
    y = UIPrim::drawTextWrapped(r, overlayHint, capFs, x + pad, y, w - pad * 2.0f, Theme::grey);
    return y + std::round(u * 0.8f);
}
