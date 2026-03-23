#include "screens/peace_screen.h"
#include "screens/game_screen.h"
#include "core/app.h"
#include "core/engine.h"
#include "core/input.h"
#include "core/audio.h"
#include "ui/primitives.h"
#include "ui/theme.h"
#include "ui/ui_assets.h"
#include "ui/toast.h"
#include "game/game_state.h"
#include "game/peace_conference.h"
#include "game/puppet.h"
#include "game/country.h"
#include "game/faction.h"
#include "game/division.h"
#include "game/helpers.h"
#include "data/region_data.h"
#include "map/map_functions.h"

namespace {
float peaceSidebarWidth(int screenW) {
    return std::clamp(screenW * 0.29f, 340.0f, 560.0f);
}

std::string peaceRegionLabel(int regionId) {
    std::string cityName = RegionData::instance().getCity(regionId);
    if (!cityName.empty()) return replaceAll(cityName, "_", " ");
    return "Region " + std::to_string(regionId);
}

bool removeCountryName(std::vector<std::string>& names, const std::string& value) {
    auto it = std::find(names.begin(), names.end(), value);
    if (it == names.end()) return false;
    names.erase(it);
    return true;
}

void grantMutualAccess(Country* a, Country* b) {
    if (!a || !b) return;
    if (std::find(a->militaryAccess.begin(), a->militaryAccess.end(), b->name) == a->militaryAccess.end()) {
        a->militaryAccess.push_back(b->name);
    }
    if (std::find(b->militaryAccess.begin(), b->militaryAccess.end(), a->name) == b->militaryAccess.end()) {
        b->militaryAccess.push_back(a->name);
    }
}
}

PeaceScreen::~PeaceScreen() {
    if (treatyMapSurface_) {
        SDL_FreeSurface(treatyMapSurface_);
        treatyMapSurface_ = nullptr;
    }
}

void PeaceScreen::enter(App& app) {
    conference = app.gameState().pendingPeaceConference.get();
    hoveredProvince_ = -1;
    scrollOffset_ = 0;
    sideScroll_ = 0;
    currentEnemy.clear();
    resetCurrentDealState();

    if (conference && !conference->losers.empty()) {
        currentEnemy = conference->losers[0];
    }

    auto& gs = app.gameState();
    if (auto* gameScreen = dynamic_cast<GameScreen*>(app.screen(ScreenType::GAME))) {
        camera_ = gameScreen->camera();
    }
    if (gs.politicalMapSurf) {
        camera_.mapWidth = static_cast<float>(gs.politicalMapSurf->w);
        camera_.mapHeight = static_cast<float>(gs.politicalMapSurf->h);
    }
    clampCameraToMapBounds(Engine::instance().WIDTH, Engine::instance().HEIGHT);
    rebuildTreatyMap(app);
}

void PeaceScreen::exit(App& app) {
    (void)app;
    conference = nullptr;
    currentEnemy.clear();
    resetCurrentDealState();
    if (treatyMapSurface_) {
        SDL_FreeSurface(treatyMapSurface_);
        treatyMapSurface_ = nullptr;
    }
    mapRenderer_.invalidateTexture();
}

void PeaceScreen::resetCurrentDealState() {
    hoveredProvince_ = -1;
    selectedProvinces.clear();
    puppetRequested_ = false;
    installGovernmentRequested_ = false;
    demilitarizeRequested_ = false;
    reparationsRequested_ = false;
}

void PeaceScreen::advanceToNextEnemy() {
    if (!conference || conference->losers.empty()) {
        currentEnemy.clear();
        resetCurrentDealState();
        return;
    }

    if (!currentEnemy.empty() &&
        std::find(conference->losers.begin(), conference->losers.end(), currentEnemy) != conference->losers.end()) {
        return;
    }

    currentEnemy = conference->losers[0];
    resetCurrentDealState();
    scrollOffset_ = 0;
}

void PeaceScreen::clampCameraToMapBounds(int screenW, int screenH) {
    if (camera_.mapHeight <= 0.0f || camera_.mapWidth <= 0.0f) return;

    float minZoomY = static_cast<float>(screenH) / camera_.mapHeight;
    camera_.zoom = std::max(camera_.zoom, minZoomY);
    camera_.zoom = std::min(camera_.zoom, 50.0f);

    float maxY = -static_cast<float>(screenH) / (2.0f * camera_.zoom);
    float minY = static_cast<float>(screenH) / (2.0f * camera_.zoom) - camera_.mapHeight;
    if (minY > maxY) {
        float centeredY = -camera_.mapHeight * 0.5f;
        minY = centeredY;
        maxY = centeredY;
    }

    camera_.y = std::clamp(camera_.y, minY, maxY);
    camera_.x = camera_.normalizeX(camera_.x);
}

void PeaceScreen::rebuildTreatyMap(App& app) {
    if (treatyMapSurface_) {
        SDL_FreeSurface(treatyMapSurface_);
        treatyMapSurface_ = nullptr;
    }

    auto& gs = app.gameState();
    if (!gs.politicalMapSurf) {
        mapRenderer_.invalidateTexture();
        return;
    }

    treatyMapSurface_ = SDL_ConvertSurface(gs.politicalMapSurf, gs.politicalMapSurf->format, 0);
    if (!treatyMapSurface_) {
        mapRenderer_.invalidateTexture();
        return;
    }

    Country* player = gs.getCountry(gs.controlledCountry);
    Color playerColor = player ? player->color : Color{0, 200, 0};

    if (gs.regionsMapSurf && !currentEnemy.empty()) {
        Country* enemy = gs.getCountry(currentEnemy);
        if (enemy) {
            for (int regionId : enemy->regions) {
                Vec2 loc = RegionData::instance().getLocation(regionId);
                bool selected = std::find(selectedProvinces.begin(), selectedProvinces.end(), regionId) != selectedProvinces.end();
                MapFunc::fillRegionMask(treatyMapSurface_, gs.regionsMapSurf, regionId, loc.x, loc.y,
                                        selected ? playerColor : enemy->color);
            }
        }
    }

    mapRenderer_.invalidateTexture();
}

void PeaceScreen::handleInput(App& app, const InputState& input) {
    if (!conference) {
        conference = app.gameState().pendingPeaceConference.get();
    }

    if (input.isKeyDown(SDL_SCANCODE_ESCAPE)) {
        if (conference) conference->finish(app.gameState());
        nextScreen = ScreenType::GAME;
        return;
    }

    auto& gs = app.gameState();
    auto& eng = Engine::instance();
    int W = eng.WIDTH;
    int H = eng.HEIGHT;
    float sideW = peaceSidebarWidth(W);
    bool overSidebar = input.mouseX <= sideW;

    if (!overSidebar && input.mouseMiddle) {
        camera_.pan(static_cast<float>(input.mouseRelX), static_cast<float>(input.mouseRelY));
    }

    if (!overSidebar && input.scrollY != 0) {
        float factor = (input.scrollY > 0) ? 1.15f : 1.0f / 1.15f;
        camera_.zoomBy(factor, static_cast<float>(input.mouseX), static_cast<float>(input.mouseY), W, H);
    }
    clampCameraToMapBounds(W, H);

    if (!conference || currentEnemy.empty() || overSidebar || !input.mouseLeftDown || !gs.regionsMapSurf) {
        return;
    }

    Country* enemy = gs.getCountry(currentEnemy);
    if (!enemy) return;

    int regionId = mapRenderer_.regionAtScreen(input.mouseX, input.mouseY, camera_, W, H, gs.regionsMapSurf);
    if (regionId < 0) return;

    if (std::find(enemy->regions.begin(), enemy->regions.end(), regionId) == enemy->regions.end()) {
        return;
    }

    auto selectedIt = std::find(selectedProvinces.begin(), selectedProvinces.end(), regionId);
    if (selectedIt != selectedProvinces.end()) {
        selectedProvinces.erase(selectedIt);
    } else {
        selectedProvinces.push_back(regionId);
    }

    hoveredProvince_ = regionId;
    Audio::instance().playSound("clickedSound");
    rebuildTreatyMap(app);
}

void PeaceScreen::update(App& app, float dt) {
    (void)dt;

    if (!conference) {
        conference = app.gameState().pendingPeaceConference.get();
    }
    if (!conference) return;

    if (conference->finished || !conference->active) {
        nextScreen = ScreenType::GAME;
        return;
    }

    advanceToNextEnemy();
    if (!treatyMapSurface_) {
        rebuildTreatyMap(app);
    }
}

void PeaceScreen::render(App& app) {
    auto& eng = Engine::instance();
    auto& gs = app.gameState();
    int W = eng.WIDTH;
    int H = eng.HEIGHT;
    float u = H / 100.0f * eng.uiScaleFactor();
    auto* r = eng.renderer;
    int mx, my;
    SDL_GetMouseState(&mx, &my);
    bool pressed = SDL_GetMouseState(nullptr, nullptr) & SDL_BUTTON_LMASK;

    eng.clear(Theme::bg_dark);

    if (treatyMapSurface_) {
        mapRenderer_.renderMap(r, treatyMapSurface_, camera_, W, H);
    } else if (gs.politicalMapSurf) {
        mapRenderer_.renderMap(r, gs.politicalMapSurf, camera_, W, H);
    }

    UIPrim::drawMapDim(r, W, H, 20);

    float sideW = peaceSidebarWidth(W);
    float sideX = 0.0f;
    float sideY = 0.0f;
    float sideH = static_cast<float>(H);
    float pad = std::round(u * 1.2f);
    float titleH = std::round(u * 5.0f);
    float contentX = sideX + pad;
    float contentW = sideW - pad * 2.0f;

    UIPrim::drawRectFilled(r, {16, 18, 24}, sideX, sideY, sideW, sideH);
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, Theme::gold_dim.r, Theme::gold_dim.g, Theme::gold_dim.b, 110);
    SDL_RenderDrawLine(r, static_cast<int>(sideW), 0, static_cast<int>(sideW), H);
    UIPrim::drawRectFilled(r, {22, 24, 32}, sideX, sideY, sideW, titleH);
    UIPrim::drawHLine(r, Theme::gold, sideX, sideW, titleH - 1.0f, 2);
    UIPrim::drawText(r, "PEACE CONFERENCE", std::max(18, static_cast<int>(u * 1.35f)),
                     sideW * 0.5f, titleH * 0.52f, "center", Theme::gold_bright, true);

    float y = titleH + pad;
    int bodyFs = std::max(12, static_cast<int>(u * 1.0f));
    int sectionFs = std::max(13, static_cast<int>(u * 1.1f));
    float rowH = std::round(u * 2.7f);
    float btnH = std::round(u * 3.3f);
    float gap = std::round(u * 0.5f);

    if (!conference) {
        UIPrim::drawText(r, "Peace conference data is not available.", bodyFs,
                         contentX, y, "midleft", Theme::grey);
        return;
    }

    UIPrim::drawSectionHeader(r, contentX, y, contentW, std::round(u * 2.8f), "VICTORS");
    y += std::round(u * 3.4f);
    for (const auto& winner : conference->winners) {
        UIPrim::drawText(r, replaceAll(winner, "_", " "), bodyFs, contentX + pad, y, "midleft", Theme::green_light);
        y += rowH;
    }

    y += gap;
    UIPrim::drawSectionHeader(r, contentX, y, contentW, std::round(u * 2.8f), "DEFEATED");
    y += std::round(u * 3.4f);
    for (const auto& loser : conference->losers) {
        bool hovered = mx >= contentX && mx <= contentX + contentW && my >= y && my <= y + rowH;
        bool active = (loser == currentEnemy);
        if (active || hovered) {
            UIPrim::drawRectFilled(r, active ? Color{42, 34, 20} : Color{24, 28, 34},
                                   contentX, y, contentW, rowH);
        }
        if (hovered && app.input().mouseLeftDown) {
            currentEnemy = loser;
            resetCurrentDealState();
            Audio::instance().playSound("clickedSound");
            rebuildTreatyMap(app);
        }
        UIPrim::drawText(r, replaceAll(loser, "_", " "), bodyFs, contentX + pad, y + rowH * 0.42f,
                         "midleft", active ? Theme::gold_bright : Theme::red_light);
        Country* enemy = gs.getCountry(loser);
        if (enemy) {
            UIPrim::drawText(r, std::to_string(enemy->regions.size()) + " rgn", bodyFs - 1,
                             contentX + contentW - pad, y + rowH * 0.42f, "midright", Theme::grey);
        }
        y += rowH;
    }

    y += gap;
    std::string currentLabel = currentEnemy.empty() ? "No active enemy" : replaceAll(currentEnemy, "_", " ");
    UIPrim::drawSectionHeader(r, contentX, y, contentW, std::round(u * 2.8f), "CURRENT DEAL");
    y += std::round(u * 3.2f);
    UIPrim::drawText(r, currentLabel, sectionFs, contentX + pad, y, "midleft", Theme::cream, true);
    y += rowH;

    Country* player = gs.getCountry(gs.controlledCountry);
    Country* enemy = currentEnemy.empty() ? nullptr : gs.getCountry(currentEnemy);
    int selectedCount = static_cast<int>(selectedProvinces.size());
    int totalEnemyRegions = enemy ? static_cast<int>(enemy->regions.size()) : 0;
    UIPrim::drawText(r, "Selected provinces: " + std::to_string(selectedCount) + " / " + std::to_string(totalEnemyRegions),
                     bodyFs, contentX + pad, y, "midleft", Theme::gold);
    y += rowH;
    UIPrim::drawText(r, "Select provinces to annex, toggle any extra terms, then finalize the deal.", bodyFs - 1,
                     contentX + pad, y, "midleft", Theme::grey);
    y += std::round(u * 2.2f);

    std::vector<std::string> pendingTerms;
    if (!selectedProvinces.empty()) {
        pendingTerms.push_back("Annex " + std::to_string(selectedProvinces.size()) + " province" +
                               (selectedProvinces.size() == 1 ? "" : "s"));
    }
    if (puppetRequested_) pendingTerms.push_back("Puppet");
    if (installGovernmentRequested_) pendingTerms.push_back("Install Government");
    if (demilitarizeRequested_) pendingTerms.push_back("Demilitarize");
    if (reparationsRequested_) pendingTerms.push_back("War Reparations");

    std::string pendingSummary = "Pending terms: ";
    if (pendingTerms.empty()) {
        pendingSummary += "None selected";
    } else {
        for (size_t i = 0; i < pendingTerms.size(); ++i) {
            if (i > 0) pendingSummary += ", ";
            pendingSummary += pendingTerms[i];
        }
    }
    UIPrim::drawText(r, pendingSummary, bodyFs - 1, contentX + pad, y, "midleft", Theme::cream);
    y += rowH;

    auto finalizeCurrentDeal = [&]() {
        if (!conference || !player || !enemy) return;

        std::string resolvedEnemy = currentEnemy;
        std::vector<int> toAnnex;
        for (int rid : selectedProvinces) {
            if (std::find(enemy->regions.begin(), enemy->regions.end(), rid) != enemy->regions.end()) {
                toAnnex.push_back(rid);
            }
        }

        if (puppetRequested_ && enemy && !enemy->regions.empty() &&
            static_cast<int>(toAnnex.size()) >= static_cast<int>(enemy->regions.size())) {
            toasts().show("A fully annexed country cannot also be puppeted");
            Audio::instance().playSound("failedClickSound");
            return;
        }

        if (!toAnnex.empty()) {
            player->addRegions(toAnnex, gs);
        }

        if (enemy && puppetRequested_) {
            bool foundPuppetState = false;
            for (auto& puppetState : gs.puppetStates) {
                if (puppetState.puppet == resolvedEnemy) {
                    puppetState.overlord = gs.controlledCountry;
                    puppetState.active = true;
                    foundPuppetState = true;
                    break;
                }
            }
            if (!foundPuppetState) {
                PuppetState ps;
                ps.overlord = gs.controlledCountry;
                ps.puppet = resolvedEnemy;
                ps.autonomy = 50.0f;
                ps.active = true;
                gs.puppetStates.push_back(ps);
            }
            enemy->puppetTo = player->name;
            enemy->ideology = player->ideology;
            enemy->ideologyName = player->ideologyName;
            grantMutualAccess(player, enemy);
            if (!player->faction.empty()) {
                if (auto* fac = gs.getFaction(player->faction)) {
                    fac->addCountry(resolvedEnemy, gs, false, false);
                }
            }
        }

        if (enemy && installGovernmentRequested_) {
            enemy->ideology = player->ideology;
            enemy->ideologyName = player->ideologyName;
        }

        if (enemy && demilitarizeRequested_) {
            while (!enemy->divisions.empty()) {
                enemy->divisions.back()->kill(gs);
            }
            enemy->militarySize = 0;
        }

        if (enemy && reparationsRequested_) {
            float reparations = enemy->money * 0.5f;
            enemy->money -= reparations;
            player->money += reparations;
        }

        if (enemy && enemy->regions.empty()) {
            gs.removeCountry(resolvedEnemy);
            enemy = nullptr;
        }

        removeCountryName(conference->losers, resolvedEnemy);
        resetCurrentDealState();
        advanceToNextEnemy();
        rebuildTreatyMap(app);
        Audio::instance().playSound("clickedSound");

        if (conference->losers.empty()) {
            conference->finish(gs);
            nextScreen = ScreenType::GAME;
        }
    };

    auto runReleaseAction = [&](const std::string& releaseName, const std::vector<int>& releaseRegions) {
        if (!conference || !player || !enemy || releaseName.empty() || releaseRegions.empty()) return;

        gs.spawnCountry(releaseName, releaseRegions);
        toasts().show("Released: " + replaceAll(releaseName, "_", " "));
        Audio::instance().playSound("clickedSound");

        if (enemy->regions.empty()) {
            std::string resolvedEnemy = currentEnemy;
            gs.removeCountry(resolvedEnemy);
            removeCountryName(conference->losers, resolvedEnemy);
            resetCurrentDealState();
            advanceToNextEnemy();
        }

        rebuildTreatyMap(app);
        if (conference->losers.empty()) {
            conference->finish(gs);
            nextScreen = ScreenType::GAME;
        }
    };

    struct ActionButton { std::string label; std::string id; };
    const std::vector<ActionButton> actionButtons = {
        {"Select All Provinces", "select_all"},
        {"Clear Selection", "clear_selection"},
        {std::string("Puppet: ") + (puppetRequested_ ? "On" : "Off"), "toggle_puppet"},
        {std::string("Install Government: ") + (installGovernmentRequested_ ? "On" : "Off"), "toggle_install_govt"},
        {std::string("Demilitarize: ") + (demilitarizeRequested_ ? "On" : "Off"), "toggle_demilitarize"},
        {std::string("War Reparations: ") + (reparationsRequested_ ? "On" : "Off"), "toggle_reparations"},
        {"Finalize Current Deal", "finalize"},
    };

    auto runDealAction = [&](const std::string& actionId) {
        if (!enemy || !player) return;

        if (actionId == "select_all") {
            selectedProvinces = enemy->regions;
        } else if (actionId == "clear_selection") {
            selectedProvinces.clear();
        } else if (actionId == "toggle_puppet") {
            puppetRequested_ = !puppetRequested_;
        } else if (actionId == "toggle_install_govt") {
            installGovernmentRequested_ = !installGovernmentRequested_;
        } else if (actionId == "toggle_demilitarize") {
            demilitarizeRequested_ = !demilitarizeRequested_;
        } else if (actionId == "toggle_reparations") {
            reparationsRequested_ = !reparationsRequested_;
        } else if (actionId == "finalize") {
            finalizeCurrentDeal();
            return;
        }

        Audio::instance().playSound("clickedSound");
        rebuildTreatyMap(app);
    };

    for (const auto& button : actionButtons) {
        bool hovered = mx >= contentX && mx <= contentX + contentW && my >= y && my <= y + btnH;
        UIPrim::drawMenuButton(r, contentX, y, contentW, btnH, button.label.c_str(), mx, my, hovered && pressed);
        if (hovered && app.input().mouseLeftDown && enemy && player) {
            runDealAction(button.id);
        }
        y += btnH + gap;
    }

    if (enemy) {
        std::vector<std::pair<std::string, std::vector<int>>> releasable;
        for (auto& [culture, cultureRegs] : enemy->cultures) {
            if (cultureRegs.empty()) continue;
            std::string releaseCountry;
            for (auto& [countryName, cObj] : gs.countries) {
                if (cObj && cObj->culture == culture && countryName != currentEnemy) {
                    releaseCountry = countryName;
                    break;
                }
            }
            if (!releaseCountry.empty()) {
                releasable.push_back({releaseCountry, cultureRegs});
            }
        }

        if (!releasable.empty()) {
            y += gap;
            UIPrim::drawSectionHeader(r, contentX, y, contentW, std::round(u * 2.8f), "RELEASE NATIONS");
            y += std::round(u * 3.4f);

            for (const auto& [releaseName, regions] : releasable) {
                if (y + btnH > H - std::round(u * 7.0f)) break;
                std::string label = "Release " + replaceAll(releaseName, "_", " ");
                bool hovered = mx >= contentX && mx <= contentX + contentW && my >= y && my <= y + btnH;
                UIPrim::drawMenuButton(r, contentX, y, contentW, btnH, label.c_str(), mx, my, hovered && pressed);
                if (hovered && app.input().mouseLeftDown) {
                    runReleaseAction(releaseName, regions);
                }
                y += btnH + gap;
            }
        }
    }

    float whitePeaceY = H - std::round(u * 5.5f);
    bool whitePeaceHovered = mx >= contentX && mx <= contentX + contentW &&
                             my >= whitePeaceY && my <= whitePeaceY + btnH;
    UIPrim::drawMenuButton(r, contentX, whitePeaceY, contentW, btnH, "White Peace", mx, my, whitePeaceHovered && pressed);
    if (whitePeaceHovered && app.input().mouseLeftDown) {
        conference->finish(gs);
        nextScreen = ScreenType::GAME;
        Audio::instance().playSound("clickedSound");
    }

    UIPrim::drawText(r, "Click provinces on the map. Selected land previews in your color.", bodyFs - 1,
                     contentX + contentW * 0.5f, H - std::round(u * 1.6f), "center", Theme::grey);

    toasts().render(r, W, H, eng.uiScale);
}
