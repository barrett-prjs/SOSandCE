#include "screens/game_screen.h"
#include "screens/save_load.h"
#include "screens/settings_screen.h"
#include "core/app.h"
#include "core/engine.h"
#include "core/audio.h"
#include "core/input.h"
#include <sstream>
#include "ui/primitives.h"
#include "ui/theme.h"
#include "ui/ui_assets.h"
#include "ui/ui_manager.h"
#include "ui/popup.h"
#include "ui/toast.h"
#include "sidebar/sidebar.h"
#include "save/save_system.h"
#include "map/map_functions.h"
#include "game/game_state.h"
#include "game/country.h"
#include "game/division.h"
#include "game/faction.h"
#include "game/buildings.h"
#include "game/fog_of_war.h"
#include "game/event_manager.h"
#include "game/peace_conference.h"
#include "data/data_loader.h"
#include "data/region_data.h"
#include "data/country_data.h"
#include "game/helpers.h"


GameScreen::GameScreen()
    : uiManager_(std::make_unique<UIManager>()),
      sidebar_(std::make_unique<Sidebar>()) {
}

GameScreen::~GameScreen() = default;

bool GameScreen::divisionHitAtScreen(const Division* div, const GameState& gs, int screenX, int screenY, int screenW, int screenH) const {
    if (!div) return false;

    float sy = camera_.worldToScreenY(div->yBlit, screenH);
    float mapW = camera_.mapWidth;
    float drawXs[] = {
        camera_.worldToScreenX(div->xBlit, screenW),
        camera_.worldToScreenX(div->xBlit + mapW, screenW),
        camera_.worldToScreenX(div->xBlit - mapW, screenW)
    };

    float w = static_cast<float>(std::max(1, div->spriteW));
    float h = static_cast<float>(std::max(1, div->spriteH));
    int divisionsInRegion = 0;
    auto regionIt = gs.divisionsByRegion.find(div->region);
    if (regionIt != gs.divisionsByRegion.end()) {
        for (auto* regionDiv : regionIt->second) {
            if (regionDiv) divisionsInRegion++;
        }
    }
    divisionsInRegion = std::max(divisionsInRegion, 1);
    float top = sy - h * divisionsInRegion * 0.5f;
    float bottom = sy + h * divisionsInRegion * 0.5f;
    for (float drawX : drawXs) {
        if (screenX >= drawX - w * 0.5f && screenX <= drawX + w * 0.5f &&
            screenY >= top && screenY <= bottom) {
            return true;
        }
    }
    return false;
}

bool GameScreen::divisionInsideRect(const Division* div, const GameState& gs, int minX, int minY, int maxX, int maxY, int screenW, int screenH) const {
    if (!div) return false;

    float sy = camera_.worldToScreenY(div->yBlit, screenH);
    float mapW = camera_.mapWidth;
    float drawXs[] = {
        camera_.worldToScreenX(div->xBlit, screenW),
        camera_.worldToScreenX(div->xBlit + mapW, screenW),
        camera_.worldToScreenX(div->xBlit - mapW, screenW)
    };

    float w = static_cast<float>(std::max(1, div->spriteW));
    float h = static_cast<float>(std::max(1, div->spriteH));
    int divisionsInRegion = 0;
    auto regionIt = gs.divisionsByRegion.find(div->region);
    if (regionIt != gs.divisionsByRegion.end()) {
        for (auto* regionDiv : regionIt->second) {
            if (regionDiv) divisionsInRegion++;
        }
    }
    divisionsInRegion = std::max(divisionsInRegion, 1);
    float top = sy - h * divisionsInRegion * 0.5f;
    float bottom = sy + h * divisionsInRegion * 0.5f;
    for (float drawX : drawXs) {
        float left = drawX - w * 0.5f;
        float right = left + w;
        if (right >= minX && left <= maxX && bottom >= minY && top <= maxY) {
            return true;
        }
    }
    return false;
}


void GameScreen::enter(App& app) {
    auto& gs = app.gameState();
    if (!gs.inGame) {

        if (!gs.mapName.empty()) {
            setupGame(app, gs.mapName);
        } else {
            return;
        }
    }

    auto& eng = Engine::instance();
    camera_.mapWidth = static_cast<float>(mapManager_.mapWidth());
    camera_.mapHeight = static_cast<float>(mapManager_.mapHeight());
    uiManager_->topBar.rebuild(app, gs, eng.WIDTH);
}

void GameScreen::exit(App& app) {

    app.gameState().speed = 0;
}


void GameScreen::handleInput(App& app, const InputState& input) {
    auto& eng = Engine::instance();
    auto& gs = app.gameState();
    int W = eng.WIDTH;
    int H = eng.HEIGHT;
    if (sidebar_) sidebar_->leftSide = app.settings().sidebarLeft;

    const bool popupWasOpen = !uiManager_->popupList.empty();


    handleKeyboard(app, input);

    if (input.mouseLeftDown) {
        SDL_Point pt = {input.mouseX, input.mouseY};
        if (SDL_PointInRect(&pt, &spectatorPlayRect_)) {
            pressed_ = false;
            selectedRegions_.clear();
            return;
        }
    }


    uiManager_->handleInput(input);


    if (popupWasOpen || !uiManager_->popupList.empty()) {
        pressed_ = false;
        selectedRegions_.clear();
        return;
    }


    handleCameraInput(input, app);


    if (input.mouseLeftDown && sidebar_ && sidebar_->isOpen() && sidebar_->showsPlayerTabs(gs)) {
        float sideW = sidebar_->getWidth(W);
        float sideX = sidebar_->getX(W);
        float tabW = sideW / 3.0f;
        float topBarH = std::max(50.0f, H * 0.06f);
        float tabH = std::max(40.0f, H * 0.06f);

        if (input.mouseX >= sideX && input.mouseY >= topBarH && input.mouseY <= topBarH + tabH) {
            int tabIdx = static_cast<int>((input.mouseX - sideX) / tabW);
            const char* tabKeys[] = {"political", "military", "industry"};
            if (tabIdx >= 0 && tabIdx < 3) {
                openedTab_ = tabKeys[tabIdx];
                sidebar_->activeTab = tabKeys[tabIdx];
            }
        }
    }


    bool overSidebar = false;
    if (sidebar_ && sidebar_->isOpen()) {
        float sideX = sidebar_->getX(W);
        float sideW = sidebar_->getWidth(W);
        overSidebar = sidebar_->leftSide
            ? (input.mouseX <= sideX + sideW)
            : (input.mouseX >= sideX);
    }
    float topBarH = std::max(56.0f, H * 0.055f);
    bool overTopBar = input.mouseY < topBarH;
    bool overMapButtons = input.mouseY > H - H * 0.04f;
    bool overUI = uiManager_->hitUI(input.mouseX, input.mouseY) || overSidebar || overTopBar || overMapButtons;


    bool hasPopups = !uiManager_->popupList.empty();


    if (input.mouseLeftDown && !overUI && !hasPopups) {
        xPressed_ = input.mouseX;
        yPressed_ = input.mouseY;
        timePressed_ = SDL_GetTicks();


        if (!selectedDivisions_.empty()) {
            float u2 = H / 100.0f * eng.uiScaleFactor();
            float abW = std::round(u2 * 4.0f), abH = std::round(u2 * 4.0f);
            float abGap = std::round(u2 * 0.5f);
            float abX = W / 2.0f - abW - abGap / 2;
            float abY2 = H - std::round(u2 * 9.0f);

            if (input.mouseX >= abX && input.mouseX <= abX + abW && input.mouseY >= abY2 && input.mouseY <= abY2 + abH) {
                Country* p = gs.getCountry(gs.controlledCountry);
                if (p) { p->mergeDivisions(gs, selectedDivisions_); selectedDivisions_.clear(); Audio::instance().playSound("clickedSound"); }
                return;
            }

            float abX2 = abX + abW + abGap;
            if (input.mouseX >= abX2 && input.mouseX <= abX2 + abW && input.mouseY >= abY2 && input.mouseY <= abY2 + abH) {
                Country* p = gs.getCountry(gs.controlledCountry);
                if (p) {
                    for (auto* div : selectedDivisions_) { if (div) p->divideDivision(gs, div); }
                    selectedDivisions_.clear(); Audio::instance().playSound("clickedSound");
                }
                return;
            }
        }


        bool clickedDiv = false;
        Country* player = gs.getCountry(gs.controlledCountry);
        if (player) {
            for (auto& div : player->divisions) {
                if (!div) continue;
                if (divisionHitAtScreen(div.get(), gs, input.mouseX, input.mouseY, W, H)) {
                    if (std::find(selectedDivisions_.begin(), selectedDivisions_.end(), div.get()) == selectedDivisions_.end()) {
                        selectedDivisions_.push_back(div.get());
                    }
                    clickedDiv = true;
                    Audio::instance().playSound("selectDivSound");
                    break;
                }
            }
        }


        if (!clickedDiv) {
            pressed_ = true;
        }
    }


    if (input.mouseLeftUp && !hasPopups) {
        if (pressed_ && !overUI) {
            float dragDist = std::sqrt(std::pow(input.mouseX - xPressed_, 2) + std::pow(input.mouseY - yPressed_, 2));

            if (dragDist > 20) {

                Country* player = gs.getCountry(gs.controlledCountry);
                if (player) {
                    int x1 = std::min(xPressed_, input.mouseX), x2 = std::max(xPressed_, input.mouseX);
                    int y1 = std::min(yPressed_, input.mouseY), y2 = std::max(yPressed_, input.mouseY);

                    if (!input.isKeyHeld(SDL_SCANCODE_LSHIFT) && !input.isKeyHeld(SDL_SCANCODE_RSHIFT)) {
                        selectedDivisions_.clear();
                    }

                    for (auto& div : player->divisions) {
                        if (!div) continue;
                        if (divisionInsideRect(div.get(), gs, x1, y1, x2, y2, W, H)) {
                            if (std::find(selectedDivisions_.begin(), selectedDivisions_.end(), div.get()) == selectedDivisions_.end()) {
                                selectedDivisions_.push_back(div.get());
                            }
                        }
                    }
                    if (!selectedDivisions_.empty()) Audio::instance().playSound("selectDivSound");
                }
            } else {


                if (!selectedDivisions_.empty()) {

                    bool hitDiv = false;
                    Country* player = gs.getCountry(gs.controlledCountry);
                    if (player) {
                        for (auto& div : player->divisions) {
                            if (!div) continue;
                            if (divisionHitAtScreen(div.get(), gs, input.mouseX, input.mouseY, W, H)) {
                                hitDiv = true;
                                break;
                            }
                        }
                    }
                    if (!hitDiv) {

                        selectedDivisions_.clear();
                    }
                } else {
                    handleMapClick(app, input);
                }
            }
        }
        pressed_ = false;
    }


    if (input.mouseRight && !overUI && !selectedDivisions_.empty()) {
        int regionId = mapRenderer_.regionAtScreen(
            input.mouseX, input.mouseY, camera_, W, H,
            mapManager_.getRegionsMap());
        if (regionId >= 0 && std::find(selectedRegions_.begin(), selectedRegions_.end(), regionId) == selectedRegions_.end()
            && (int)selectedRegions_.size() < (int)selectedDivisions_.size()) {
            selectedRegions_.push_back(regionId);
        }
    }

    if (input.mouseRightDown && !overUI && selectedDivisions_.empty()) {
        handleMapRightClick(app, input);
    }


    if (input.mouseRightUp && !selectedDivisions_.empty()) {
        if (!selectedRegions_.empty()) {
            int idx = 0;
            for (auto* div : selectedDivisions_) {
                if (div && idx < (int)selectedRegions_.size()) {
                    div->command(selectedRegions_[idx], gs, false, false, 300);
                    idx++;
                    if (idx >= (int)selectedRegions_.size()) idx = 0;
                }
            }
            Audio::instance().playSound("moveDivSound");
        }
        selectedRegions_.clear();
    }
}


void GameScreen::handleCameraInput(const InputState& input, App& app) {
    auto& eng = Engine::instance();
    int W = eng.WIDTH;
    int H = eng.HEIGHT;

    float panSpeed = 400.0f / camera_.zoom;

    if (input.isKeyHeld(SDL_SCANCODE_LSHIFT) || input.isKeyHeld(SDL_SCANCODE_RSHIFT)) {
        panSpeed *= 3.0f;
    }


    if (input.isKeyHeld(SDL_SCANCODE_W) || input.isKeyHeld(SDL_SCANCODE_UP))
        camera_.pan(0, panSpeed * 0.016f);
    if (input.isKeyHeld(SDL_SCANCODE_S) || input.isKeyHeld(SDL_SCANCODE_DOWN))
        camera_.pan(0, -panSpeed * 0.016f);
    if (input.isKeyHeld(SDL_SCANCODE_A) || input.isKeyHeld(SDL_SCANCODE_LEFT))
        camera_.pan(panSpeed * 0.016f, 0);
    if (input.isKeyHeld(SDL_SCANCODE_D) || input.isKeyHeld(SDL_SCANCODE_RIGHT))
        camera_.pan(-panSpeed * 0.016f, 0);


    if (input.mouseMiddle) {
        camera_.pan(static_cast<float>(input.mouseRelX),
                    static_cast<float>(input.mouseRelY));
    }


    if (input.scrollY != 0) {
        float factor = (input.scrollY > 0) ? 1.15f : 1.0f / 1.15f;
        camera_.zoomBy(factor, static_cast<float>(input.mouseX),
                       static_cast<float>(input.mouseY), W, H);
    }


    if (input.isKeyHeld(SDL_SCANCODE_RIGHTBRACKET)) {
        camera_.zoomBy(1.02f, W * 0.5f, H * 0.5f, W, H);
    }
    if (input.isKeyHeld(SDL_SCANCODE_LEFTBRACKET)) {
        camera_.zoomBy(1.0f / 1.02f, W * 0.5f, H * 0.5f, W, H);
    }

    clampCameraToMapBounds(W, H);
}

void GameScreen::clampCameraToMapBounds(int screenW, int screenH) {
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

bool GameScreen::isMapViewMode(const GameState& gs) const {
    return gs.mapViewOnly && gs.controlledCountry.empty();
}

bool GameScreen::isSpectatorMode(const GameState& gs) const {
    return !gs.mapViewOnly && gs.controlledCountry.empty();
}

void GameScreen::startCampaignAsCountry(App& app, const std::string& countryName) {
    auto& gs = app.gameState();
    if (countryName.empty()) return;

    const bool fromMapPreview = isMapViewMode(gs);

    gs.controlledCountry = countryName;
    gs.mapViewOnly = false;

    if (fromMapPreview || !gs.inGame) {
        gs.inGame = false;
        setupGame(app, gs.mapName);
        return;
    }

    if (gs.fogOfWar) {
        gs.fogOfWar->recalculate(gs.controlledCountry, gs);
    }

    selectedDivisions_.clear();
    selectedRegions_.clear();
    clicked_ = gs.controlledCountry;
    openedTab_ = "political";

    if (sidebar_) {
        sidebar_->activeTab = "political";
        sidebar_->selectedCountry.clear();
        sidebar_->clearOverlayInfo();
        sidebar_->animation = 1.0f;
        sidebar_->open();
        sideBarAnimation_ = 1.0f;
    }

    focusCameraOnCountry(countryName, app);
    uiManager_->topBar.rebuild(app, gs, Engine::instance().WIDTH);
    toasts().show("Now playing as " + replaceAll(countryName, "_", " "));
    Audio::instance().playSound("startGameSound");
}

void GameScreen::startSpectatorMode(App& app) {
    auto& gs = app.gameState();
    const bool fromMapPreview = isMapViewMode(gs);

    gs.controlledCountry.clear();
    gs.mapViewOnly = false;

    if (fromMapPreview || !gs.inGame) {
        gs.inGame = false;
        setupGame(app, gs.mapName);
        gs.speed = std::max(gs.speed, 1);
        return;
    }

    selectedDivisions_.clear();
    selectedRegions_.clear();
    clicked_.clear();
    openedTab_ = "political";

    if (sidebar_) {
        sidebar_->activeTab = "political";
        sidebar_->selectedCountry.clear();
        sidebar_->clearOverlayInfo();
        sidebar_->open();
        sidebar_->animation = 1.0f;
        sideBarAnimation_ = 1.0f;
    }

    uiManager_->topBar.rebuild(app, gs, Engine::instance().WIDTH);
    gs.speed = std::max(gs.speed, 1);
    toasts().show("Spectator mode enabled");
    Audio::instance().playSound("startGameSound");
}

void GameScreen::takeControlOfCountry(App& app, const std::string& countryName) {
    if (countryName.empty()) return;

    auto& gs = app.gameState();
    if (isMapViewMode(gs)) {
        startCampaignAsCountry(app, countryName);
        return;
    }

    gs.controlledCountry = countryName;
    gs.mapViewOnly = false;
    startCampaignAsCountry(app, countryName);
}

void GameScreen::openCountrySidebar(App& app, const std::string& countryName) {
    auto& gs = app.gameState();
    if (!sidebar_ || countryName.empty()) return;

    clicked_ = countryName;
    openedTab_ = "political";
    sidebar_->clearOverlayInfo();
    sidebar_->selectedCountry =
        (!isMapViewMode(gs) && countryName == gs.controlledCountry) ? "" : countryName;
    sidebar_->activeTab = "political";
    sidebar_->animation = 1.0f;
    sidebar_->open();
    sideBarAnimation_ = 1.0f;
    Audio::instance().playSound("select");
}

void GameScreen::openOverlaySidebar(App& app, int regionId, const std::string& owner, MapMode mode) {
    auto& gs = app.gameState();
    if (!sidebar_) return;

    auto& rd = RegionData::instance();
    auto regionName = rd.getCity(regionId);
    if (regionName.empty()) regionName = "Region " + std::to_string(regionId);
    else regionName = replaceAll(regionName, "_", " ");

    std::string title = regionName;
    std::vector<std::string> lines;
    Color accent = Theme::gold_bright;

    auto ownerDisplay = owner.empty() ? std::string("Unowned") : replaceAll(owner, "_", " ");

    if (mode == MapMode::FACTION) {
        auto* country = gs.getCountry(owner);
        if (!country) return;

        if (country->faction.empty()) {
            title = "Unaffiliated";
            accent = Theme::grey;
            lines = {
                "Country: " + ownerDisplay,
                "Faction: None",
                "Ideology: " + replaceAll(country->ideologyName, "_", " "),
                "Regions: " + std::to_string(country->regions.size())
            };
        } else {
            auto* fac = gs.getFaction(country->faction);
            title = replaceAll(country->faction, "_", " ");
            accent = fac ? fac->color : country->factionColor;
            lines.push_back("Viewed country: " + ownerDisplay);
            if (fac) {
                lines.push_back("Leader: " + replaceAll(fac->factionLeader, "_", " "));
                lines.push_back("Members: " + std::to_string(fac->members.size()));
                std::string memberLine = "States: ";
                const size_t previewCount = std::min<size_t>(4, fac->members.size());
                for (size_t i = 0; i < previewCount; ++i) {
                    if (i > 0) memberLine += ", ";
                    memberLine += replaceAll(fac->members[i], "_", " ");
                }
                if (fac->members.size() > previewCount) memberLine += ", ...";
                lines.push_back(memberLine);
            } else {
                lines.push_back("Members: unavailable");
            }
        }
    } else if (mode == MapMode::IDEOLOGY) {
        auto* country = gs.getCountry(owner);
        if (!country) return;
        title = titleCase(replaceAll(country->ideologyName, "_", " "));
        accent = Helpers::getIdeologyColor(country->ideology[0], country->ideology[1]);
        lines = {
            "Country: " + ownerDisplay,
            "Economic axis: " + std::to_string((int)std::lround(country->ideology[0] * 100.0f)),
            "Social axis: " + std::to_string((int)std::lround(country->ideology[1] * 100.0f))
        };
        if (!country->faction.empty()) {
            lines.push_back("Faction: " + replaceAll(country->faction, "_", " "));
        }
    } else if (mode == MapMode::BIOME) {
        SDL_Surface* biomeMap = mapManager_.getBiomeMap();
        if (!biomeMap) return;

        Vec2 loc = rd.getLocation(regionId);
        int px = std::clamp((int)std::round(loc.x), 0, biomeMap->w - 1);
        int py = std::clamp((int)std::round(loc.y), 0, biomeMap->h - 1);
        Color biomeColor = getPixel(biomeMap, px, py);
        std::string biomeName = rd.getBiomeName(biomeColor);
        auto biomeInfo = rd.getBiomeInfo(biomeColor);

        title = biomeName.empty() ? "Biome" : biomeName;
        accent = Theme::green_light;
        lines = {
            "Region: " + regionName,
            "Owner: " + ownerDisplay,
            "Attack modifier: " + std::to_string(biomeInfo[1]),
            "Defense modifier: " + std::to_string(biomeInfo[2]),
            "Movement modifier: " + std::to_string(biomeInfo[3])
        };
    } else if (mode == MapMode::INDUSTRY) {
        auto* country = gs.getCountry(owner);
        std::vector<Building> regionBuildings;
        int queued = 0;
        if (country) {
            regionBuildings = country->buildingManager.getInRegion(regionId);
            for (const auto& entry : country->buildingManager.constructionQueue) {
                if (entry.regionId == regionId) queued++;
            }
        }

        accent = Theme::blue;
        lines.push_back("Owner: " + ownerDisplay);
        lines.push_back("Buildings: " + std::to_string(regionBuildings.size()));
        if (!regionBuildings.empty()) {
            std::vector<std::pair<std::string, int>> counts;
            for (const auto& b : regionBuildings) {
                auto it = std::find_if(counts.begin(), counts.end(),
                    [&](const auto& entry) { return entry.first == buildingTypeName(b.type); });
                if (it == counts.end()) counts.push_back({buildingTypeName(b.type), 1});
                else it->second++;
            }
            std::string summary = "Sites: ";
            for (size_t i = 0; i < counts.size(); ++i) {
                if (i > 0) summary += ", ";
                summary += counts[i].first + " x" + std::to_string(counts[i].second);
                if (i >= 2 && counts.size() > 3) {
                    summary += ", ...";
                    break;
                }
            }
            lines.push_back(summary);
            accent = buildingMapColor(regionBuildings.front().type);
        } else {
            lines.push_back("Sites: none built here");
        }
        if (queued > 0) {
            lines.push_back("Construction queued: " + std::to_string(queued));
        }
    } else if (mode == MapMode::RESOURCE) {
        const auto& resources = rd.getResources(regionId);
        std::vector<std::pair<std::string, float>> sortedResources(resources.begin(), resources.end());
        std::sort(sortedResources.begin(), sortedResources.end(),
                  [](const auto& a, const auto& b) { return a.second > b.second; });

        auto resourceAccent = [](const std::string& key) -> Color {
            if (key == "oil") return {50, 50, 60};
            if (key == "steel") return {100, 140, 220};
            if (key == "aluminum") return {170, 220, 255};
            if (key == "tungsten") return {220, 150, 40};
            if (key == "chromium") return {200, 60, 200};
            if (key == "rubber") return {40, 190, 40};
            return Theme::gold_bright;
        };

        lines.push_back("Owner: " + ownerDisplay);
        if (sortedResources.empty()) {
            lines.push_back("No mapped land resources");
            accent = Theme::grey;
        } else {
            accent = resourceAccent(sortedResources.front().first);
            const size_t previewCount = std::min<size_t>(3, sortedResources.size());
            for (size_t i = 0; i < previewCount; ++i) {
                const auto& [name, amount] = sortedResources[i];
                lines.push_back(titleCase(replaceAll(name, "_", " ")) + ": " + std::to_string((int)std::lround(amount)));
            }
        }
    }

    clicked_.clear();
    sidebar_->animation = 1.0f;
    sidebar_->open();
    sideBarAnimation_ = 1.0f;
    sidebar_->setOverlayInfo(title, lines, accent);
    Audio::instance().playSound("select");
}


void GameScreen::handleMapClick(App& app, const InputState& input) {
    auto& eng = Engine::instance();
    auto& gs = app.gameState();
    int W = eng.WIDTH;
    int H = eng.HEIGHT;

    int regionId = mapRenderer_.regionAtScreen(
        input.mouseX, input.mouseY, camera_, W, H,
        mapManager_.getRegionsMap());

    if (regionId < 0) return;

    auto& rd = RegionData::instance();
    std::string owner = rd.getOwner(regionId);

    MapMode currentMode = mapManager_.getMode();

    if (currentMode != MapMode::POLITICAL) {
        openOverlaySidebar(app, regionId, owner, currentMode);
        return;
    }

    if (owner.empty()) return;
    openCountrySidebar(app, owner);
}

void GameScreen::focusCameraOnCountry(const std::string& countryName, App& app) {
    auto& eng = Engine::instance();
    auto& gs = app.gameState();
    auto& regData = RegionData::instance();
    Country* player = gs.getCountry(countryName);
    if (!player || player->regions.empty()) return;

    float sumX = 0.0f;
    float sumY = 0.0f;
    float minX = 1e9f, maxX = -1e9f, minY = 1e9f, maxY = -1e9f;
    for (int rid : player->regions) {
        Vec2 loc = regData.getLocation(rid);
        sumX += loc.x;
        sumY += loc.y;
        minX = std::min(minX, loc.x);
        maxX = std::max(maxX, loc.x);
        minY = std::min(minY, loc.y);
        maxY = std::max(maxY, loc.y);
    }

    float count = static_cast<float>(player->regions.size());
    camera_.x = -(sumX / count);
    camera_.y = -(sumY / count);

    float extent = std::max(maxX - minX, maxY - minY);
    if (extent > 0.0f) {
        float W = static_cast<float>(eng.WIDTH);
        camera_.zoom = std::clamp((W / extent + 16.0f) / 5.0f, 0.7f, 46.0f);
    }
    clampCameraToMapBounds(eng.WIDTH, eng.HEIGHT);
}


void GameScreen::handleMapRightClick(App& app, const InputState& input) {
    auto& eng = Engine::instance();
    auto& gs = app.gameState();
    if (isMapViewMode(gs)) return;
    int W = eng.WIDTH;
    int H = eng.HEIGHT;

    int regionId = mapRenderer_.regionAtScreen(
        input.mouseX, input.mouseY, camera_, W, H,
        mapManager_.getRegionsMap());

    if (regionId < 0) return;


    selectedDivisions_.clear();
    selectedRegions_.clear();

    auto it = gs.divisionsByRegion.find(regionId);
    if (it != gs.divisionsByRegion.end()) {
        for (auto* div : it->second) {
            if (div && div->country == gs.controlledCountry) {
                selectedDivisions_.push_back(div);
            }
        }
    }

    if (!selectedDivisions_.empty()) {
        selectedRegions_.push_back(regionId);
        Audio::instance().playSound("selectDivSound");
    }


    if (sidebar_ && sidebar_->activeTab == "industry" &&
        (sidebar_->selectedCountry.empty() || sidebar_->selectedCountry == gs.controlledCountry)) {
        Country* player = gs.getCountry(gs.controlledCountry);
        if (player) {
            std::string owner = RegionData::instance().getOwner(regionId);
            if (owner != gs.controlledCountry) {
                toasts().show("You don't own this region");
            } else {
                std::string buildMode = sidebar_ ? sidebar_->currentlyBuilding : currentlyBuilding_;

                if (buildMode == "destroy") {
                    player->destroy(gs, regionId);
                    toasts().show("Destroying building...");
                    Audio::instance().playSound("clickedSound");
                } else {

                    BuildingType bt = buildingTypeFromString(buildMode);
                    float baseCost = static_cast<float>(buildingBaseCost(bt));
                    float dynamicCost = baseCost * (1.0f + player->regions.size() / 150.0f);
                    auto resCost = buildingResourceCost(bt);
                    auto lacksResources = [&]() {
                        for (int i = 0; i < RESOURCE_COUNT; ++i) {
                            if (player->resourceManager.stockpile[i] + 0.001f < resCost[i]) {
                                return true;
                            }
                        }
                        return false;
                    };
                    auto resourceBundleText = [&]() {
                        static const char* shortNames[] = {"Oil", "Stl", "Alu", "Tun", "Chr", "Rub"};
                        std::ostringstream oss;
                        bool first = true;
                        for (int i = 0; i < RESOURCE_COUNT; ++i) {
                            if (resCost[i] <= 0.001f) continue;
                            if (!first) oss << " | ";
                            first = false;
                            oss << shortNames[i] << " " << Helpers::resourceValueText(resCost[i]);
                        }
                        return oss.str();
                    };

                    if (!player->buildingManager.canBuild(regionId, bt)) {
                        toasts().show("Cannot build here (at max)");
                        Audio::instance().playSound("failedClickSound");
                    } else if (player->money < dynamicCost) {
                        char buf[64]; snprintf(buf, sizeof(buf), "Not enough money (need $%s)", Helpers::prefixNumber(dynamicCost).c_str());
                        toasts().show(buf);
                        Audio::instance().playSound("failedClickSound");
                    } else if (lacksResources()) {
                        toasts().show("Need " + resourceBundleText());
                        Audio::instance().playSound("failedClickSound");
                    } else {
                        player->money -= dynamicCost;
                        for (int i = 0; i < RESOURCE_COUNT; ++i) {
                            player->resourceManager.stockpile[i] =
                                std::max(0.0f, player->resourceManager.stockpile[i] - resCost[i]);
                        }
                        player->build(gs, buildMode, buildingDays(bt), regionId);
                        std::string label = replaceAll(buildMode, "_", " ");
                        label[0] = std::toupper(label[0]);
                        toasts().show("Building " + label + "...");
                        Audio::instance().playSound("buildSound");
                    }
                }
            }
        }
    }
}


void GameScreen::handleKeyboard(App& app, const InputState& input) {
    auto& gs = app.gameState();


    if (input.isKeyDown(SDL_SCANCODE_ESCAPE)) {
        if (!uiManager_->popupList.empty()) {
            printf("[GameScreen] ESC closed popup stack (%zu popups)\n", uiManager_->popupList.size());
            fflush(stdout);
            uiManager_->popupList.clear();
            gs.speed = 1;
            Audio::instance().playSound("closeMenuSound");
        } else {

            App* appPtr = &app;
            float s = 2.2f;
            float y0 = 3.0f;
            auto popup = std::make_unique<Popup>(
                "Menu",
                std::vector<std::string>{"Game paused."},
                std::vector<PopupButton>{
                    PopupButton{"Resume",       "", [appPtr]() { appPtr->gameState().speed = 1; },                  0, y0,         4.5f},
                    PopupButton{"Save Game",    "", [appPtr]() {
                        auto* saveLoad = dynamic_cast<SaveLoadScreen*>(appPtr->screen(ScreenType::SAVE_LOAD));
                        if (saveLoad) {
                            saveLoad->mode = SaveLoadScreen::Mode::SAVE;
                            saveLoad->returnScreen = ScreenType::GAME;
                        }
                        appPtr->switchScreen(ScreenType::SAVE_LOAD);
                    }, 0, y0 + s, 4.5f},
                    PopupButton{"Settings",     "", [appPtr]() {
                        if (auto* settings = dynamic_cast<SettingsScreen*>(appPtr->screen(ScreenType::SETTINGS))) {
                            settings->returnScreen = ScreenType::GAME;
                        }
                        appPtr->switchScreen(ScreenType::SETTINGS);
                    },     0, y0 + s * 2, 4.5f},
                    PopupButton{"Quit to Menu", "", [appPtr]() {
                        appPtr->requestGameResetOnTransition();
                        appPtr->switchScreen(ScreenType::MAIN_MENU);
                    }, 0, y0 + s * 3, 4.5f}
                },
                14.0f, y0 + s * 3 + 2.5f
            );
            gs.speed = 0;
            uiManager_->popupList.push_back(std::move(popup));
            printf("[GameScreen] ESC opened pause popup\n");
            fflush(stdout);
            Audio::instance().playSound("openMenuSound");
        }
    }


    if (input.isKeyDown(SDL_SCANCODE_TAB)) {
        if (sidebar_) {
            sidebar_->toggle();
            sideBarAnimation_ = sidebar_->isOpen() ? 0.0f : 1.0f;
            Audio::instance().playSound(sidebar_->isOpen() ? "openMenuSound" : "closeMenuSound");
        }
    }


    if (input.isKeyDown(SDL_SCANCODE_F2)) {
        showUI_ = !showUI_;
        toasts().show(showUI_ ? "UI enabled" : "UI disabled");
    }


    if (input.isKeyDown(SDL_SCANCODE_F3)) {
        oldDivisions_ = !oldDivisions_;
        toasts().show(oldDivisions_ ? "Timelapse divisions ON" : "Timelapse divisions OFF");
    }


    if (input.isKeyDown(SDL_SCANCODE_F1)) {
        auto& eng = Engine::instance();
        SDL_Surface* sshot = SDL_CreateRGBSurface(0, eng.WIDTH, eng.HEIGHT, 32,
                                                   0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000);
        if (sshot) {
            SDL_RenderReadPixels(eng.renderer, nullptr, sshot->format->format,
                                 sshot->pixels, sshot->pitch);
            std::string filename = "screenshot_" + std::to_string(SDL_GetTicks()) + ".bmp";
            SDL_SaveBMP(sshot, filename.c_str());
            SDL_FreeSurface(sshot);
            toasts().show("Screenshot saved: " + filename);
        }
    }


    if (input.isKeyDown(SDL_SCANCODE_SLASH) && !selectedDivisions_.empty()) {
        Country* player = gs.getCountry(gs.controlledCountry);
        if (player) {
            for (auto* div : selectedDivisions_) {
                if (div) player->divideDivision(gs, div);
            }
            selectedDivisions_.clear();
            Audio::instance().playSound("clickedSound");
        }
    }


    if (input.isKeyDown(SDL_SCANCODE_M) && !selectedDivisions_.empty()) {
        Country* player = gs.getCountry(gs.controlledCountry);
        if (player) {
            player->mergeDivisions(gs, selectedDivisions_);
            selectedDivisions_.clear();
            Audio::instance().playSound("clickedSound");
        }
    }


    if (input.isKeyDown(SDL_SCANCODE_F5)) {
        std::vector<std::pair<std::string, float>> sorted;
        for (auto& [n, c] : gs.countries) { if (c) sorted.push_back({n, (float)c->population}); }
        std::sort(sorted.begin(), sorted.end(), [](auto& a, auto& b) { return a.second > b.second; });
        std::string msg = "Population: ";
        for (int i = 0; i < std::min(5, (int)sorted.size()); i++)
            msg += replaceAll(sorted[i].first, "_", " ") + " (" + Helpers::prefixNumber(sorted[i].second) + ") ";
        toasts().show(msg, 4000);
    }
    if (input.isKeyDown(SDL_SCANCODE_F6)) {
        std::vector<std::pair<std::string, int>> sorted;
        for (auto& [n, c] : gs.countries) { if (c) { int d=0; for(auto& div:c->divisions) d+=div->divisionStack; sorted.push_back({n,d}); }}
        std::sort(sorted.begin(), sorted.end(), [](auto& a, auto& b) { return a.second > b.second; });
        std::string msg = "Military: ";
        for (int i = 0; i < std::min(5, (int)sorted.size()); i++)
            msg += replaceAll(sorted[i].first, "_", " ") + " (" + std::to_string(sorted[i].second) + ") ";
        toasts().show(msg, 4000);
    }


    if (input.isKeyDown(SDL_SCANCODE_F7)) {
        std::vector<std::pair<std::string, int>> sorted;
        for (auto& [n, c] : gs.countries) { if (c) sorted.push_back({n, (int)c->regions.size()}); }
        std::sort(sorted.begin(), sorted.end(), [](auto& a, auto& b) { return a.second > b.second; });
        std::string msg = "Regions: ";
        for (int i = 0; i < std::min(5, (int)sorted.size()); i++)
            msg += replaceAll(sorted[i].first, "_", " ") + " (" + std::to_string(sorted[i].second) + ") ";
        toasts().show(msg, 4000);
    }

    if (input.isKeyDown(SDL_SCANCODE_F8)) {
        std::vector<std::pair<std::string, int>> sorted;
        for (auto& [n, c] : gs.countries) {
            if (c) {
                int fac = c->buildingManager.countAll(BuildingType::CivilianFactory)
                        + c->buildingManager.countAll(BuildingType::MilitaryFactory);
                sorted.push_back({n, fac});
            }
        }
        std::sort(sorted.begin(), sorted.end(), [](auto& a, auto& b) { return a.second > b.second; });
        std::string msg = "Factories: ";
        for (int i = 0; i < std::min(5, (int)sorted.size()); i++)
            msg += replaceAll(sorted[i].first, "_", " ") + " (" + std::to_string(sorted[i].second) + ") ";
        toasts().show(msg, 4000);
    }


    if (input.isKeyDown(SDL_SCANCODE_F4)) {
        showTutorialPopup(gs);
    }


    if (input.isKeyDown(SDL_SCANCODE_F9)) {
        auto* saveLoad = dynamic_cast<SaveLoadScreen*>(app.screen(ScreenType::SAVE_LOAD));
        if (saveLoad) {
            saveLoad->mode = SaveLoadScreen::Mode::SAVE;
            saveLoad->returnScreen = ScreenType::GAME;
        }
        nextScreen = ScreenType::SAVE_LOAD;
    }


    auto resetSidebarForMapMode = [&]() {
        if (!sidebar_) return;
        sidebar_->clearOverlayInfo();
        if (gs.controlledCountry.empty()) {
            sidebar_->selectedCountry.clear();
            sidebar_->close();
        } else if (!sidebar_->selectedCountry.empty() && sidebar_->selectedCountry != gs.controlledCountry) {
            sidebar_->selectedCountry.clear();
            sidebar_->activeTab = openedTab_.empty() ? "political" : openedTab_;
        }
    };

    auto setMapMode = [&](MapMode mode, const std::string& label) {
        mapManager_.setMode(mode);
        resetSidebarForMapMode();
        toasts().show(label);
    };
    if (input.isKeyDown(SDL_SCANCODE_1)) { setMapMode(MapMode::POLITICAL, "Political Map"); }
    if (input.isKeyDown(SDL_SCANCODE_2)) { setMapMode(MapMode::FACTION, "Faction Map"); }
    if (input.isKeyDown(SDL_SCANCODE_3)) { setMapMode(MapMode::IDEOLOGY, "Ideology Map"); }
    if (input.isKeyDown(SDL_SCANCODE_4)) { setMapMode(MapMode::INDUSTRY, "Industry Map"); }
    if (input.isKeyDown(SDL_SCANCODE_5)) { setMapMode(MapMode::BIOME, "Biome Map"); }
    if (input.isKeyDown(SDL_SCANCODE_6)) { setMapMode(MapMode::RESOURCE, "Resource Map"); }

    if (gs.mapViewOnly) {
        gs.speed = 0;
    }


    if (!gs.mapViewOnly && (input.isKeyDown(SDL_SCANCODE_KP_PLUS) || input.isKeyDown(SDL_SCANCODE_EQUALS))) {
        gs.speed = std::min(gs.speed + 1, 10);
        Audio::instance().playSound("clickedSound");
    }
    if (!gs.mapViewOnly && (input.isKeyDown(SDL_SCANCODE_KP_MINUS) || input.isKeyDown(SDL_SCANCODE_MINUS))) {
        gs.speed = std::max(gs.speed - 1, 0);
        Audio::instance().playSound("clickedSound");
    }
    if (!gs.mapViewOnly && input.isKeyDown(SDL_SCANCODE_SPACE)) {
        gs.speed = gs.speed > 0 ? 0 : 1;
        Audio::instance().playSound("clickedSound");
    }

    if (input.mouseLeftDown && !gs.mapViewOnly) {
        SDL_Point pt = {input.mouseX, input.mouseY};
        if (SDL_PointInRect(&pt, &speedMinusRect_)) {
            gs.speed = std::max(gs.speed - 1, 0);
            Audio::instance().playSound("clickedSound");
        }
        if (SDL_PointInRect(&pt, &speedPlusRect_)) {
            gs.speed = std::min(gs.speed + 1, 10);
            Audio::instance().playSound("clickedSound");
        }
    }

    if (input.mouseLeftDown) {
        if (isMapViewMode(gs)) {
            auto& engPreview = Engine::instance();
            int W = engPreview.WIDTH;
            int H = engPreview.HEIGHT;
            SDL_Point pt = {input.mouseX, input.mouseY};
            float u2 = H / 100.0f * Engine::instance().uiScaleFactor();
            float specW = std::round(u2 * 11.5f);
            float specH = std::round(u2 * 3.3f);
            float specX = std::round((W - specW) / 2.0f);
            float specY = H - std::round(u2 * 8.0f);
            SDL_Rect spectatorRect = {
                static_cast<int>(specX), static_cast<int>(specY),
                static_cast<int>(specW), static_cast<int>(specH)
            };
            if (SDL_PointInRect(&pt, &spectatorRect)) {
                startSpectatorMode(app);
                return;
            }
        }


        auto& eng2 = Engine::instance();
        int W = eng2.WIDTH, H = eng2.HEIGHT;
        float u2 = H / 100.0f * Engine::instance().uiScaleFactor();
        float mmBtnW = std::round(u2 * 7.0f);
        float mmBtnH = std::round(u2 * 3.0f);
        float mmGap = std::round(u2 * 0.3f);
        float totalMmW2 = 6 * mmBtnW + 5 * mmGap;
        float mmX = std::round((W - totalMmW2) / 2.0f);
        float mmY = H - std::round(u2 * 4.0f);
        MapMode mmModes[] = {MapMode::POLITICAL, MapMode::FACTION, MapMode::IDEOLOGY,
                              MapMode::INDUSTRY, MapMode::BIOME, MapMode::RESOURCE};
        const char* mmToasts[] = {"Political Map", "Faction Map", "Ideology Map", "Industry Map", "Biome Map", "Resource Map"};
        for (int i = 0; i < 6; i++) {
            float bx = mmX + i * (mmBtnW + mmGap);
            if (input.mouseX >= bx && input.mouseX <= bx + mmBtnW &&
                input.mouseY >= mmY && input.mouseY <= mmY + mmBtnH) {
                mapManager_.setMode(mmModes[i]);
                resetSidebarForMapMode();
                toasts().show(mmToasts[i]);
                Audio::instance().playSound("clickedSound");
            }
        }
    }
}


void GameScreen::update(App& app, float dt) {
    auto& gs = app.gameState();
    auto& eng = Engine::instance();

    uiManager_->update(dt);


    if (sidebar_) {
        sidebar_->update(app, dt);
        sideBarAnimation_ = sidebar_->animation;
    }

    if (gs.mapViewOnly) {
        gs.speed = 0;
    }

    clampCameraToMapBounds(eng.WIDTH, eng.HEIGHT);

    if (gs.pendingPeaceConference) {
        if (gs.pendingPeaceConference->finished || !gs.pendingPeaceConference->active) {
            gs.pendingPeaceConference.reset();
        } else if (nextScreen == ScreenType::NONE) {
            nextScreen = ScreenType::PEACE_CONFERENCE;
        }
    }


    if (gs.speed > 0) {

        float tickRate = gs.speed * 4.0f;
        gs.tick += dt * tickRate;

        while (gs.tick >= 1.0f) {
            gs.tick -= 1.0f;

            gs.tickAll();
        }


        uiManager_->topBar.rebuild(app, gs, eng.WIDTH);

    }


    if (gs.mapDirty) {
        mapManager_.reloadIndustryMap(gs);
        mapManager_.rebuildTextures(eng.renderer);
        mapRenderer_.invalidateTexture();
        gs.mapDirty = false;
    }


    if (nextScreen != ScreenType::NONE) {
        app.switchScreen(nextScreen);
        nextScreen = ScreenType::NONE;
    }
}


void GameScreen::render(App& app) {
    auto& eng = Engine::instance();
    auto& gs = app.gameState();
    int W = eng.WIDTH;
    int H = eng.HEIGHT;
    if (sidebar_) sidebar_->leftSide = app.settings().sidebarLeft;


    auto getSkyColor = [](int hour) -> Color {
        struct KeyTime { int h; uint8_t r, g, b; };
        KeyTime keys[] = {{4,0,0,0}, {6,218,204,165}, {7,169,182,190}, {9,134,170,196},
                          {16,134,170,196}, {17,91,103,177}, {18,119,100,132}, {19,131,69,92}, {21,0,0,0}};
        int n = 9;
        for (int i = 0; i < n - 1; i++) {
            if (hour >= keys[i].h && hour < keys[i+1].h) {
                float t = (float)(hour - keys[i].h) / (keys[i+1].h - keys[i].h);
                return {(uint8_t)(keys[i].r + (keys[i+1].r - keys[i].r) * t),
                        (uint8_t)(keys[i].g + (keys[i+1].g - keys[i].g) * t),
                        (uint8_t)(keys[i].b + (keys[i+1].b - keys[i].b) * t)};
            }
        }
        return {0, 0, 0};
    };
    Color sky = getSkyColor(gs.time.hour);
    eng.clear(sky);


    renderMap(app);


    if (pressed_) {
        int cx, cy; SDL_GetMouseState(&cx, &cy);
        float dist = std::sqrt(std::pow(cx - xPressed_, 2) + std::pow(cy - yPressed_, 2));
        if (dist > 10) {
            int x1 = std::min(xPressed_, cx), y1 = std::min(yPressed_, cy);
            int x2 = std::max(xPressed_, cx), y2 = std::max(yPressed_, cy);
            SDL_SetRenderDrawBlendMode(eng.renderer, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(eng.renderer, 100, 200, 100, 40);
            SDL_Rect selBox = {x1, y1, x2 - x1, y2 - y1};
            SDL_RenderFillRect(eng.renderer, &selBox);
            SDL_SetRenderDrawColor(eng.renderer, 100, 255, 100, 180);
            SDL_RenderDrawRect(eng.renderer, &selBox);
        }
    }


    if (!selectedDivisions_.empty()) {
        float u = H / 100.0f * eng.uiScaleFactor();
        float abW = std::round(u * 4.0f), abH = std::round(u * 4.0f);
        float abGap = std::round(u * 0.5f);
        float abX = W / 2.0f - abW - abGap / 2;
        float abY = H - std::round(u * 9.0f);
        int abMx, abMy; SDL_GetMouseState(&abMx, &abMy);


        bool mergeHov = abMx >= abX && abMx <= abX + abW && abMy >= abY && abMy <= abY + abH;
        SDL_Texture* panelTex = UIAssets::instance().panelBodyHeadless();
        if (panelTex) {
            uint8_t mod = mergeHov ? 255 : 236;
            SDL_SetTextureColorMod(panelTex, mod, mod, mod);
            SDL_SetTextureAlphaMod(panelTex, 248);
            UIAssets::draw9Slice(eng.renderer, panelTex, abX, abY, abW, abH, 18);
            SDL_SetTextureColorMod(panelTex, 255, 255, 255);
            SDL_SetTextureAlphaMod(panelTex, 255);
        } else {
            UIPrim::drawRoundedRect(eng.renderer, mergeHov ? Color{40, 45, 55} : Color{20, 22, 28, 200},
                                    abX, abY, abW, abH, 4, mergeHov ? Theme::gold_dim : Theme::border);
        }
        UIPrim::drawHLine(eng.renderer, mergeHov ? Theme::gold_dim : Color{Theme::border.r, Theme::border.g, Theme::border.b, 90},
                          abX + 4, abX + abW - 4, abY + abH - 2, 1);
        int abFs = std::max(10, (int)(abH * 0.22f));
        UIPrim::drawText(eng.renderer, "M", (int)(abH * 0.5f), abX + abW / 2, abY + abH * 0.35f, "center", Theme::cream);
        UIPrim::drawText(eng.renderer, "Merge", abFs, abX + abW / 2, abY + abH * 0.75f, "center", Theme::grey);


        float abX2 = abX + abW + abGap;
        bool splitHov = abMx >= abX2 && abMx <= abX2 + abW && abMy >= abY && abMy <= abY + abH;
        if (panelTex) {
            uint8_t mod = splitHov ? 255 : 236;
            SDL_SetTextureColorMod(panelTex, mod, mod, mod);
            SDL_SetTextureAlphaMod(panelTex, 248);
            UIAssets::draw9Slice(eng.renderer, panelTex, abX2, abY, abW, abH, 18);
            SDL_SetTextureColorMod(panelTex, 255, 255, 255);
            SDL_SetTextureAlphaMod(panelTex, 255);
        } else {
            UIPrim::drawRoundedRect(eng.renderer, splitHov ? Color{40, 45, 55} : Color{20, 22, 28, 200},
                                    abX2, abY, abW, abH, 4, splitHov ? Theme::gold_dim : Theme::border);
        }
        UIPrim::drawHLine(eng.renderer, splitHov ? Theme::gold_dim : Color{Theme::border.r, Theme::border.g, Theme::border.b, 90},
                          abX2 + 4, abX2 + abW - 4, abY + abH - 2, 1);
        UIPrim::drawText(eng.renderer, "/", (int)(abH * 0.5f), abX2 + abW / 2, abY + abH * 0.35f, "center", Theme::cream);
        UIPrim::drawText(eng.renderer, "Split", abFs, abX2 + abW / 2, abY + abH * 0.75f, "center", Theme::grey);
    }


    if (showUI_ && mapManager_.getMode() == MapMode::RESOURCE) {
        float u = H / 100.0f * eng.uiScaleFactor();
        float keyX = u * 1.5f;
        float keyY = u * 8.0f;
        float keyW = u * 14.0f;
        float rowH = u * 2.0f;
        int legendFs = std::max(11, (int)(u * 1.0f));

        struct ResLegend { const char* name; Color color; };
        ResLegend legends[] = {
            {"Oil",      {50, 50, 60}},
            {"Steel",    {100, 140, 220}},
            {"Aluminum", {170, 220, 255}},
            {"Tungsten", {220, 150, 40}},
            {"Chromium", {200, 60, 200}},
            {"Rubber",   {40, 190, 40}},
        };
        int count = 6;
        float keyH = rowH * (count + 1) + u;

        SDL_SetRenderDrawBlendMode(eng.renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(eng.renderer, 0, 0, 0, 180);
        SDL_Rect kb = {(int)keyX, (int)keyY, (int)keyW, (int)keyH};
        SDL_RenderFillRect(eng.renderer, &kb);
        SDL_SetRenderDrawColor(eng.renderer, Theme::gold_dim.r, Theme::gold_dim.g, Theme::gold_dim.b, 80);
        SDL_RenderDrawRect(eng.renderer, &kb);

        UIPrim::drawText(eng.renderer, "Resources", legendFs + 1, keyX + keyW / 2, keyY + u * 0.6f, "topcenter", Theme::cream, true);
        float ky = keyY + rowH + u * 0.4f;
        for (int li = 0; li < count; li++) {
            float swatchW = u * 1.5f, swatchH = u * 1.2f;
            SDL_SetRenderDrawColor(eng.renderer, legends[li].color.r, legends[li].color.g, legends[li].color.b, 255);
            SDL_Rect sw = {(int)(keyX + u * 0.8f), (int)(ky + (rowH - swatchH) / 2), (int)swatchW, (int)swatchH};
            SDL_RenderFillRect(eng.renderer, &sw);
            SDL_SetRenderDrawColor(eng.renderer, Theme::grey.r, Theme::grey.g, Theme::grey.b, 255);
            SDL_RenderDrawRect(eng.renderer, &sw);
            UIPrim::drawText(eng.renderer, legends[li].name, legendFs, keyX + u * 3.0f, ky + rowH * 0.5f, "midleft", Theme::cream);
            ky += rowH;
        }
    }


    if (showUI_ && sidebar_ && sidebar_->isOpen()) {
        UIPrim::drawMapDim(eng.renderer, W, H, 25);
    }


    if (showUI_) {
        renderUI(app);
        renderPopups(app);
    }


    uiManager_->topBar.tooltip_.hide();

    if (showUI_ && !uiManager_->popupList.empty()) {

    } else {
        int hmx, hmy; SDL_GetMouseState(&hmx, &hmy);
        float topH2 = std::max(56.0f, H * 0.055f);
        bool overUI2 = hmy < topH2;
        if (sidebar_ && sidebar_->isOpen()) {
            float sideX2 = sidebar_->getX(W);
            float sideW2 = sidebar_->getWidth(W);
            if (sidebar_->leftSide) {
                if (hmx <= sideX2 + sideW2) overUI2 = true;
            } else if (hmx >= sideX2) {
                overUI2 = true;
            }
        }
        if (hmy > H - H * 0.04f) overUI2 = true;
        if (isMapViewMode(gs)) {
            SDL_Point hoverPt = {hmx, hmy};
            if (SDL_PointInRect(&hoverPt, &spectatorPlayRect_)) {
                overUI2 = true;
            }
        }

        if (!overUI2 && !pressed_ && selectedDivisions_.empty()) {
            int hovRegion = mapRenderer_.regionAtScreen(hmx, hmy, camera_, W, H, mapManager_.getRegionsMap());
            if (hovRegion > 0) {
                auto& rd = RegionData::instance();
                std::string owner = rd.getOwner(hovRegion);
                std::string city = rd.getCity(hovRegion);
                std::string regionName = city.empty() ? ("Region " + std::to_string(hovRegion)) : city;

                std::vector<std::pair<std::string, Color>> lines;
                if (!owner.empty()) lines.push_back({"Owner: " + replaceAll(owner, "_", " "), Theme::gold});


                auto& resources = rd.getResources(hovRegion);
                if (!resources.empty()) {
                    for (auto& [rname, ramount] : resources) {
                        char rbuf[64]; snprintf(rbuf, sizeof(rbuf), "%s: %.0f", rname.c_str(), ramount);
                        lines.push_back({rbuf, Theme::green_light});
                    }
                } else {
                    lines.push_back({"No resources", Theme::dark_grey});
                }


                std::vector<std::string> tipLines;
                tipLines.push_back(regionName);
                for (auto& [text, color] : lines) tipLines.push_back(text);
                uiManager_->topBar.tooltip_.show(tipLines, (float)hmx, (float)hmy);
            }
        }
    }
    uiManager_->topBar.tooltip_.render(eng.renderer, W, H);


    toasts().render(eng.renderer, W, H, eng.uiScale);
}


void GameScreen::renderMap(App& app) {
    auto& eng = Engine::instance();
    auto& gs = app.gameState();
    int W = eng.WIDTH;
    int H = eng.HEIGHT;


    SDL_Surface* activeSurface = nullptr;


    if (showResources_ && mapManager_.getResourceMap()) {
        activeSurface = mapManager_.getResourceMap();
    }

    else if (sidebar_ && sidebar_->activeTab == "industry" &&
        (sidebar_->selectedCountry.empty() || sidebar_->selectedCountry == gs.controlledCountry) &&
        sidebar_->isOpen()) {

        std::string buildSel = sidebar_->currentlyBuilding;
        bool isResourceBuilding = (buildSel == "mine" || buildSel == "oil_well" || buildSel == "refinery");
        if (isResourceBuilding && mapManager_.getResourceMap()) {
            activeSurface = mapManager_.getResourceMap();
        } else {
            activeSurface = mapManager_.getModifiedIndustryMap();
            if (!activeSurface) activeSurface = mapManager_.getIndustryMap();
        }
    }
    if (!activeSurface) activeSurface = mapManager_.getActiveSurface();

    if (activeSurface) {
        mapRenderer_.renderMap(eng.renderer, activeSurface, camera_, W, H);
    }


    if (!selectedRegions_.empty()) {
        mapRenderer_.renderCommands(eng.renderer, gs, camera_, W, H,
                                    selectedRegions_, selectedDivisions_);
    }


    {
        mapRenderer_.renderCities(eng.renderer, gs, camera_, W, H);
    }


    {
        mapRenderer_.renderDivisions(eng.renderer, gs, camera_, W, H, &selectedDivisions_);
    }


    mapRenderer_.renderBattles(eng.renderer, gs, camera_, W, H);
}


void GameScreen::renderUI(App& app) {
    auto& eng = Engine::instance();
    auto& gs = app.gameState();
    int W = eng.WIDTH, H = eng.HEIGHT;
    float ui = eng.uiScale;


    float u = H / 100.0f * eng.uiScaleFactor();
    float topH = std::round(std::max(56.0f, u * 5.5f));
    auto* rr = eng.renderer;
    auto& assets = UIAssets::instance();


    UIPrim::drawRectFilled(rr, {14, 16, 20}, 0, 0, (float)W, topH);
    SDL_SetRenderDrawBlendMode(rr, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(rr, 255, 255, 255, 5);
    SDL_Rect topHl = {0, 0, W, (int)(topH * 0.35f)};
    SDL_RenderFillRect(rr, &topHl);


    UIPrim::drawHLine(rr, Theme::gold_dim, 0, (float)W, topH - 2, 1);
    UIPrim::drawHLine(rr, {6, 8, 10}, 0, (float)W, topH - 1, 1);

    int mx, my; SDL_GetMouseState(&mx, &my);

    Country* player = gs.getCountry(gs.controlledCountry);
    if (player) {
        float pad = std::round(u * 0.6f);
        float slotH = topH - pad * 2;
        int fsName = std::max(15, (int)(topH * 0.32f));
        int fsLabel = std::max(11, (int)(topH * 0.18f));
        int fsVal = std::max(13, (int)(topH * 0.26f));


        float flagW = std::round(u * 4.0f);
        float flagH = slotH;
        float flagX = pad + 2, flagY = pad;

        std::string lowerName = gs.controlledCountry;
        std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
        SDL_Texture* flagTex = eng.loadTexture(eng.assetsPath + "flags/" + lowerName + "_flag.png");
        if (flagTex) {
            int fw, fh; SDL_QueryTexture(flagTex, nullptr, nullptr, &fw, &fh);
            float dw = flagW, dh = flagW * fh / fw;
            if (dh > flagH) { dh = flagH; dw = flagH * fw / fh; }
            SDL_Rect fd = {(int)(flagX + (flagW - dw)/2), (int)(flagY + (flagH - dh)/2), (int)dw, (int)dh};
            SDL_RenderCopy(rr, flagTex, nullptr, &fd);
        }

        SDL_SetRenderDrawColor(rr, Theme::gold_dim.r, Theme::gold_dim.g, Theme::gold_dim.b, 150);
        SDL_Rect fb = {(int)flagX, (int)flagY, (int)flagW, (int)flagH};
        SDL_RenderDrawRect(rr, &fb);


        float tx = flagX + flagW + std::round(u * 0.8f);
        std::string cname = replaceAll(gs.controlledCountry, "_", " ");
        UIPrim::drawText(rr, cname, fsName, tx, topH * 0.5f, "midleft", Theme::gold_bright, true);
        tx += UIPrim::textWidth(cname, fsName) + std::round(u * 1.5f);


        SDL_SetRenderDrawColor(rr, Theme::border.r, Theme::border.g, Theme::border.b, 60);
        SDL_RenderDrawLine(rr, (int)tx, (int)(pad + 4), (int)tx, (int)(topH - pad - 4));
        tx += std::round(u * 1.0f);


        struct Slot { const char* label; std::string value; Color color; const char* icon; };
        std::vector<Slot> slots;
        slots.push_back({"PP", Helpers::prefixNumber(player->politicalPower), Theme::blue, "Star"});
        slots.push_back({"MP", Helpers::prefixNumber(std::max(0.0f, player->manPower)), Theme::cream, "Player"});
        slots.push_back({"$", Helpers::prefixNumber(std::max(0.0f, player->money)),
                          player->money < 0 ? Theme::red_light : Theme::gold, "Cart"});
        int totalFac = player->buildingManager.countAll(BuildingType::CivilianFactory)
                     + player->buildingManager.countAll(BuildingType::MilitaryFactory);
        slots.push_back({"Fac", std::to_string(totalFac), Theme::cream, "Gear"});
        slots.push_back({"Stab", std::to_string((int)player->stability) + "%",
                          player->stability < 30 ? Theme::red_light : (player->stability < 50 ? Theme::orange : Theme::green_light), "Levels"});


        auto& rm = player->resourceManager;
        const char* resLabels[] = {"Oil", "Stl", "Alu", "Tun", "Chr", "Rub"};
        for (int ri = 0; ri < RESOURCE_COUNT && ri < 6; ri++) {
            float stk = rm.stockpile[ri];
            float netR = rm.production[ri] + rm.tradeImports[ri] - rm.consumption[ri] - rm.tradeExports[ri];
            Color rc = (netR < 0) ? Theme::red_light : Theme::cream;
            slots.push_back({resLabels[ri], Helpers::resourceValueText(stk), rc, nullptr});
        }

        float slotGap = std::round(u * 0.3f);
        for (auto& s : slots) {
            std::string valText = s.value;
            float valW = (float)UIPrim::textWidth(valText, fsVal);
            float labelW = (float)UIPrim::textWidth(s.label, fsLabel);
            float iconSz2 = s.icon ? slotH * 0.35f : 0.0f;
            float minW = s.icon ? std::round(u * 6.0f) : std::round(u * 4.0f);
            float slotW = std::max(minW, iconSz2 + labelW + valW + std::round(u * 2.0f));


            UIPrim::drawBeveledRect(rr, Theme::slot, tx, pad, slotW, slotH);


            SDL_Texture* slotIcon = s.icon ? assets.icon(s.icon) : nullptr;
            if (slotIcon) {
                SDL_SetTextureColorMod(slotIcon, 120, 118, 115);
                SDL_SetTextureAlphaMod(slotIcon, 160);
                SDL_Rect iDst = {(int)(tx + 4), (int)(pad + 3), (int)iconSz2, (int)iconSz2};
                SDL_RenderCopy(rr, slotIcon, nullptr, &iDst);
                SDL_SetTextureColorMod(slotIcon, 255, 255, 255);
                SDL_SetTextureAlphaMod(slotIcon, 255);
            }


            UIPrim::drawText(rr, s.label, fsLabel, tx + iconSz2 + 6, pad + slotH * 0.22f, "midleft", Theme::dark_grey);
            UIPrim::drawText(rr, valText, fsVal, tx + slotW * 0.5f, pad + slotH * 0.65f, "center", s.color, true);

            tx += slotW + slotGap;
        }


        float rightEdge = (float)W - pad - 4;


        float tensionVal = gs.eventManager ? gs.eventManager->globalTension : 0.0f;
        int tensionPct = std::clamp((int)(tensionVal * 100), 0, 100);
        std::string tensionStr = "WT " + std::to_string(tensionPct) + "%";
        Color tensionColor = tensionPct > 70 ? Theme::red_light : (tensionPct > 40 ? Theme::orange : Theme::cream);
        float tensionW = std::round(u * 6.0f);
        float tensionX = rightEdge - tensionW;
        UIPrim::drawBeveledRect(rr, Theme::slot, tensionX, pad, tensionW, slotH);
        UIPrim::drawText(rr, tensionStr, fsVal, tensionX + tensionW / 2, topH * 0.5f, "center", tensionColor, true);


        float speedBtnSz = std::round(slotH * 0.9f);
        float speedSlotW = std::round(u * 5.0f);
        float speedTotalW = speedBtnSz + speedSlotW + speedBtnSz + slotGap * 2;
        float speedGroupX = tensionX - speedTotalW - slotGap;


        float minusBtnX = speedGroupX;
        bool minusHov = mx >= minusBtnX && mx <= minusBtnX + speedBtnSz && my >= pad && my <= pad + slotH;
        UIPrim::drawBeveledRect(rr, minusHov ? Theme::btn_hover : Theme::slot, minusBtnX, pad, speedBtnSz, slotH);
        SDL_Texture* minusIcon = assets.icon("ArrowLeft");
        if (minusIcon) {
            float iSz = speedBtnSz * 0.5f;
            SDL_SetTextureColorMod(minusIcon, minusHov ? 220 : 160, minusHov ? 210 : 155, minusHov ? 195 : 150);
            SDL_Rect iDst = {(int)(minusBtnX + (speedBtnSz - iSz)/2), (int)(pad + (slotH - iSz)/2), (int)iSz, (int)iSz};
            SDL_RenderCopy(rr, minusIcon, nullptr, &iDst);
            SDL_SetTextureColorMod(minusIcon, 255, 255, 255);
        }


        float speedX = minusBtnX + speedBtnSz + slotGap;
        UIPrim::drawBeveledRect(rr, Theme::slot, speedX, pad, speedSlotW, slotH);
        float pipW = std::round(u * 0.5f), pipGap2 = std::round(u * 0.2f);
        float pipsStartX = speedX + (speedSlotW - 5 * (pipW + pipGap2)) / 2;
        for (int i = 0; i < 5; i++) {
            Color pc = i < gs.speed ? Theme::gold : Color{30, 32, 38};
            UIPrim::drawRoundedRect(rr, pc, pipsStartX + i * (pipW + pipGap2), pad + slotH * 0.25f, pipW, slotH * 0.5f, 2);
        }
        if (gs.speed == 0) {
            SDL_Texture* pauseIcon = assets.icon("Pause");
            if (pauseIcon) {
                float piSz = slotH * 0.45f;
                SDL_SetTextureColorMod(pauseIcon, Theme::red.r, Theme::red.g, Theme::red.b);
                SDL_SetTextureAlphaMod(pauseIcon, 200);
                SDL_Rect piDst = {(int)(speedX + speedSlotW/2 - piSz/2), (int)(pad + slotH/2 - piSz/2), (int)piSz, (int)piSz};
                SDL_RenderCopy(rr, pauseIcon, nullptr, &piDst);
                SDL_SetTextureColorMod(pauseIcon, 255, 255, 255);
                SDL_SetTextureAlphaMod(pauseIcon, 255);
            }
        }


        float plusBtnX = speedX + speedSlotW + slotGap;
        bool plusHov = mx >= plusBtnX && mx <= plusBtnX + speedBtnSz && my >= pad && my <= pad + slotH;
        UIPrim::drawBeveledRect(rr, plusHov ? Theme::btn_hover : Theme::slot, plusBtnX, pad, speedBtnSz, slotH);
        SDL_Texture* plusIcon = assets.icon("ArrowRight");
        if (plusIcon) {
            float iSz = speedBtnSz * 0.5f;
            SDL_SetTextureColorMod(plusIcon, plusHov ? 220 : 160, plusHov ? 210 : 155, plusHov ? 195 : 150);
            SDL_Rect iDst = {(int)(plusBtnX + (speedBtnSz - iSz)/2), (int)(pad + (slotH - iSz)/2), (int)iSz, (int)iSz};
            SDL_RenderCopy(rr, plusIcon, nullptr, &iDst);
            SDL_SetTextureColorMod(plusIcon, 255, 255, 255);
        }


        speedMinusRect_ = {(int)minusBtnX, (int)pad, (int)speedBtnSz, (int)slotH};
        speedPlusRect_ = {(int)plusBtnX, (int)pad, (int)speedBtnSz, (int)slotH};


        char hourBuf[8];
        snprintf(hourBuf, sizeof(hourBuf), "%02d:00", gs.time.hour);
        std::string dateStr = std::to_string(gs.time.day) + " " +
            Helpers::getMonthName(gs.time.month) + ", " + std::to_string(gs.time.year) +
            "  " + hourBuf;
        float dateW = std::round(u * 14.0f);
        float dateX = speedGroupX - dateW - slotGap;
        UIPrim::drawBeveledRect(rr, Theme::slot, dateX, pad, dateW, slotH);
        UIPrim::drawText(rr, dateStr, fsVal, dateX + dateW / 2, topH * 0.5f, "center", Theme::cream, true);
    } else if (isSpectatorMode(gs)) {
        float pad = std::round(u * 0.6f);
        float slotH = topH - pad * 2;
        int fsName = std::max(15, (int)(topH * 0.32f));
        int fsLabel = std::max(11, (int)(topH * 0.18f));
        int fsVal = std::max(13, (int)(topH * 0.26f));
        float slotGap = std::round(u * 0.3f);

        float modeCardX = pad + 2;
        float modeCardW = std::round(u * 18.0f);
        UIPrim::drawBeveledRect(rr, Theme::slot, modeCardX, pad, modeCardW, slotH);
        UIPrim::drawText(rr, "Spectator Mode", fsName,
                         modeCardX + std::round(u * 0.9f), pad + slotH * 0.38f,
                         "midleft", Theme::gold_bright, true);
        UIPrim::drawText(rr, replaceAll(gs.mapName, "_", " "), fsLabel,
                         modeCardX + std::round(u * 0.9f), pad + slotH * 0.70f,
                         "midleft", Theme::cream);
        UIPrim::drawText(rr, "All countries are AI-controlled. Click a country to take control.",
                         std::max(10, fsLabel - 1), modeCardX + modeCardW - std::round(u * 0.9f),
                         pad + slotH * 0.70f, "midright", Theme::grey);

        float rightEdge = (float)W - pad - 4;
        float tensionVal = gs.eventManager ? gs.eventManager->globalTension : 0.0f;
        int tensionPct = std::clamp((int)(tensionVal * 100), 0, 100);
        std::string tensionStr = "WT " + std::to_string(tensionPct) + "%";
        Color tensionColor = tensionPct > 70 ? Theme::red_light : (tensionPct > 40 ? Theme::orange : Theme::cream);
        float tensionW = std::round(u * 6.0f);
        float tensionX = rightEdge - tensionW;
        UIPrim::drawBeveledRect(rr, Theme::slot, tensionX, pad, tensionW, slotH);
        UIPrim::drawText(rr, tensionStr, fsVal, tensionX + tensionW / 2, topH * 0.5f, "center", tensionColor, true);

        float speedBtnSz = std::round(slotH * 0.9f);
        float speedSlotW = std::round(u * 5.0f);
        float speedTotalW = speedBtnSz + speedSlotW + speedBtnSz + slotGap * 2;
        float speedGroupX = tensionX - speedTotalW - slotGap;

        float minusBtnX = speedGroupX;
        bool minusHov = mx >= minusBtnX && mx <= minusBtnX + speedBtnSz && my >= pad && my <= pad + slotH;
        UIPrim::drawBeveledRect(rr, minusHov ? Theme::btn_hover : Theme::slot, minusBtnX, pad, speedBtnSz, slotH);
        SDL_Texture* minusIcon = assets.icon("ArrowLeft");
        if (minusIcon) {
            float iSz = speedBtnSz * 0.5f;
            SDL_SetTextureColorMod(minusIcon, minusHov ? 220 : 160, minusHov ? 210 : 155, minusHov ? 195 : 150);
            SDL_Rect iDst = {(int)(minusBtnX + (speedBtnSz - iSz)/2), (int)(pad + (slotH - iSz)/2), (int)iSz, (int)iSz};
            SDL_RenderCopy(rr, minusIcon, nullptr, &iDst);
            SDL_SetTextureColorMod(minusIcon, 255, 255, 255);
        }

        float speedX = minusBtnX + speedBtnSz + slotGap;
        UIPrim::drawBeveledRect(rr, Theme::slot, speedX, pad, speedSlotW, slotH);
        float pipW = std::round(u * 0.5f), pipGap2 = std::round(u * 0.2f);
        float pipsStartX = speedX + (speedSlotW - 5 * (pipW + pipGap2)) / 2;
        for (int i = 0; i < 5; i++) {
            Color pc = i < gs.speed ? Theme::gold : Color{30, 32, 38};
            UIPrim::drawRoundedRect(rr, pc, pipsStartX + i * (pipW + pipGap2), pad + slotH * 0.25f, pipW, slotH * 0.5f, 2);
        }
        if (gs.speed == 0) {
            SDL_Texture* pauseIcon = assets.icon("Pause");
            if (pauseIcon) {
                float piSz = slotH * 0.45f;
                SDL_SetTextureColorMod(pauseIcon, Theme::red.r, Theme::red.g, Theme::red.b);
                SDL_SetTextureAlphaMod(pauseIcon, 200);
                SDL_Rect piDst = {(int)(speedX + speedSlotW/2 - piSz/2), (int)(pad + slotH/2 - piSz/2), (int)piSz, (int)piSz};
                SDL_RenderCopy(rr, pauseIcon, nullptr, &piDst);
                SDL_SetTextureColorMod(pauseIcon, 255, 255, 255);
                SDL_SetTextureAlphaMod(pauseIcon, 255);
            }
        }

        float plusBtnX = speedX + speedSlotW + slotGap;
        bool plusHov = mx >= plusBtnX && mx <= plusBtnX + speedBtnSz && my >= pad && my <= pad + slotH;
        UIPrim::drawBeveledRect(rr, plusHov ? Theme::btn_hover : Theme::slot, plusBtnX, pad, speedBtnSz, slotH);
        SDL_Texture* plusIcon = assets.icon("ArrowRight");
        if (plusIcon) {
            float iSz = speedBtnSz * 0.5f;
            SDL_SetTextureColorMod(plusIcon, plusHov ? 220 : 160, plusHov ? 210 : 155, plusHov ? 195 : 150);
            SDL_Rect iDst = {(int)(plusBtnX + (speedBtnSz - iSz)/2), (int)(pad + (slotH - iSz)/2), (int)iSz, (int)iSz};
            SDL_RenderCopy(rr, plusIcon, nullptr, &iDst);
            SDL_SetTextureColorMod(plusIcon, 255, 255, 255);
        }

        speedMinusRect_ = {(int)minusBtnX, (int)pad, (int)speedBtnSz, (int)slotH};
        speedPlusRect_ = {(int)plusBtnX, (int)pad, (int)speedBtnSz, (int)slotH};

        char hourBuf[8];
        snprintf(hourBuf, sizeof(hourBuf), "%02d:00", gs.time.hour);
        std::string dateStr = std::to_string(gs.time.day) + " " +
            Helpers::getMonthName(gs.time.month) + ", " + std::to_string(gs.time.year) +
            "  " + hourBuf;
        float dateW = std::round(u * 14.0f);
        float dateX = speedGroupX - dateW - slotGap;
        UIPrim::drawBeveledRect(rr, Theme::slot, dateX, pad, dateW, slotH);
        UIPrim::drawText(rr, dateStr, fsVal, dateX + dateW / 2, topH * 0.5f, "center", Theme::cream, true);
    } else {
        speedMinusRect_ = {0, 0, 0, 0};
        speedPlusRect_ = {0, 0, 0, 0};
        float promptW = std::round(u * 21.0f);
        float promptH = std::round(u * 5.5f);
        float promptX = std::round(u * 1.2f);
        float promptY = topH + std::round(u * 1.0f);
        UIPrim::drawRoundedRect(rr, {16, 18, 24, 220}, promptX, promptY, promptW, promptH, 5, Theme::gold_dim);
        UIPrim::drawText(rr, "Scenario Map", std::max(15, (int)(u * 1.1f)),
                         promptX + promptW * 0.5f, promptY + promptH * 0.32f, "center", Theme::gold_bright, true);
        UIPrim::drawText(rr, "Click a country to inspect it, then use Play in the sidebar.",
                         std::max(11, (int)(u * 0.86f)),
                         promptX + promptW * 0.5f, promptY + promptH * 0.66f, "center", Theme::cream);
    }


    if (sidebar_) {
        sidebar_->render(eng.renderer, app);
    }

    if (isMapViewMode(gs)) {
        float specW = std::round(u * 11.5f);
        float specH = std::round(u * 3.3f);
        float specX = std::round((W - specW) / 2.0f);
        float specY = H - std::round(u * 8.0f);
        spectatorPlayRect_ = {
            static_cast<int>(specX), static_cast<int>(specY),
            static_cast<int>(specW), static_cast<int>(specH)
        };
        bool specHov = mx >= specX && mx <= specX + specW && my >= specY && my <= specY + specH;
        UIPrim::drawMenuButton(rr, specX, specY, specW, specH, "Play As Spectator", mx, my, false);
        if (specHov) {
            UIPrim::drawText(rr, "Start the scenario with every country under AI control.",
                             std::max(10, (int)(u * 0.75f)),
                             specX + specW * 0.5f, specY - std::round(u * 0.45f),
                             "center", Theme::cream);
        }
    } else {
        spectatorPlayRect_ = {0, 0, 0, 0};
    }


    {
        float mmBtnW = std::round(u * 7.0f);
        float mmBtnH = std::round(u * 3.0f);
        float mmGap = std::round(u * 0.3f);
        float totalMmW = 6 * mmBtnW + 5 * mmGap;
        float mmX = std::round((W - totalMmW) / 2.0f);
        float mmY = H - std::round(u * 4.0f);

        const char* mmNames[] = {"Political", "Factions", "Ideology", "Industry", "Biome", "Resource"};
        const MapMode mmModes[] = {MapMode::POLITICAL, MapMode::FACTION, MapMode::IDEOLOGY,
                                    MapMode::INDUSTRY, MapMode::BIOME, MapMode::RESOURCE};
        MapMode currentMode = mapManager_.getMode();

        for (int i = 0; i < 6; i++) {
            float bx = mmX + i * (mmBtnW + mmGap);
            float by = mmY;
            bool isActive = (currentMode == mmModes[i]);
            bool isHov = mx >= bx && mx <= bx + mmBtnW && my >= by && my <= by + mmBtnH;


            Color bg = isActive ? Color{35, 45, 55} : (isHov ? Color{28, 32, 40} : Color{16, 18, 22, 200});
            UIPrim::drawRoundedRect(eng.renderer, bg, bx, by, mmBtnW, mmBtnH, 4,
                                    isActive ? Theme::gold_dim : Color{Theme::border.r, Theme::border.g, Theme::border.b, 80});

            if (isActive) {
                UIPrim::drawHLine(eng.renderer, Theme::gold, bx + 4, bx + mmBtnW - 4, by + 1, 2);
            }

            int mmFs = std::max(11, (int)(mmBtnH * 0.36f));
            Color tc = isActive ? Theme::gold_bright : (isHov ? Theme::cream : Theme::grey);
            std::string label = std::string("") + (char)('1' + i) + " " + mmNames[i];
            UIPrim::drawText(eng.renderer, label, mmFs, bx + mmBtnW / 2, by + mmBtnH / 2, "center", tc, isActive);
        }
    }


    {
        Country* playerFronts = gs.getCountry(gs.controlledCountry);
        if (playerFronts && !playerFronts->battleBorder.empty()) {
            auto& rd = RegionData::instance();
            std::vector<std::string> frontNames;
            SDL_Surface* wrMap = mapManager_.getWorldRegionsMap();
            for (int region : playerFronts->battleBorder) {
                if (!wrMap) break;
                Vec2 loc = rd.getLocation(region);
                int prx = std::clamp((int)std::round(loc.x), 0, wrMap->w - 1);
                int pry = std::clamp((int)std::round(loc.y), 0, wrMap->h - 1);
                Color wrColor = getPixel(wrMap, prx, pry);
                std::string wrName = rd.getWorldRegion(wrColor);
                if (!wrName.empty() && wrName != "Error") {
                    std::string frontLabel = wrName + " Front";
                    if (std::find(frontNames.begin(), frontNames.end(), frontLabel) == frontNames.end()) {
                        frontNames.push_back(frontLabel);
                    }
                }
            }
            std::sort(frontNames.begin(), frontNames.end());

            float frontW = std::round(u * 12.0f);
            float frontH = std::round(u * 2.5f);
            float frontX = W - frontW - std::round(u * 3.0f);
            float frontY = topH + std::round(u * 1.0f);

            for (auto& fn : frontNames) {
                UIPrim::drawRoundedRect(eng.renderer, {35, 18, 18, 200}, frontX, frontY, frontW, frontH, 4, Theme::red);
                int frontFs = std::max(10, (int)(frontH * 0.38f));
                UIPrim::drawText(eng.renderer, fn, frontFs, frontX + frontW / 2, frontY + frontH / 2, "center", Theme::red_light);
                frontY += frontH + std::round(u * 0.4f);
                if (frontY > H * 0.5f) break;
            }
        }
    }
}


void GameScreen::renderPopups(App& app) {
    auto& eng = Engine::instance();
    float uiSize = Theme::uiScale;

    for (auto& popup : uiManager_->popupList) {
        if (popup) {
            popup->draw(eng.renderer, uiSize);
        }
    }
}


void GameScreen::setupGame(App& app, const std::string& scenario, int eventFrequency) {
    auto& eng = Engine::instance();
    auto& gs = app.gameState();
    const std::string scenarioName = scenario;

    printf("[GameScreen] setupGame: scenario=%s\n", scenarioName.c_str()); fflush(stdout);

    resetSessionState();


    std::string savedControlled = gs.controlledCountry;
    bool savedMapViewOnly = gs.mapViewOnly;


    gs.clear();
    gs.mapName = scenarioName;
    gs.controlledCountry = savedControlled;
    gs.mapViewOnly = savedMapViewOnly;


    mapManager_.loadMaps(scenarioName);
    mapManager_.setMode(MapMode::POLITICAL);


    camera_.mapWidth = static_cast<float>(mapManager_.mapWidth());
    camera_.mapHeight = static_cast<float>(mapManager_.mapHeight());


    gs.politicalMapSurf = mapManager_.getPoliticalMap();
    gs.ideologyMapSurf = mapManager_.getIdeologyMap();
    gs.factionMapSurf = mapManager_.getFactionMap();
    gs.industryMapSurf = mapManager_.getIndustryMap();
    gs.cultureMapSurf = mapManager_.getCultureMap();
    gs.regionsMapSurf = mapManager_.getRegionsMap();
    camera_.x = 0;
    camera_.y = 0;
    camera_.zoom = 1.0f;


    gs.fogOfWar = std::make_unique<FogOfWar>();


    printf("[GameScreen] Setting up countries from map...\n"); fflush(stdout);

    auto& regData = RegionData::instance();
    auto& countryData = CountryData::instance();
    SDL_Surface* politMap = mapManager_.getPoliticalMap();

    if (!politMap) {
        printf("[GameScreen] ERROR: No political map loaded!\n"); fflush(stdout);
        return;
    }


    if (SDL_MUSTLOCK(politMap)) SDL_LockSurface(politMap);


    std::unordered_map<std::string, std::vector<int>> ownerRegions;
    int matched = 0;

    for (int id = 1; id <= regData.regionCount(); id++) {
        Vec2 loc = regData.getLocation(id);
        int px = std::clamp(static_cast<int>(std::round(loc.x)), 0, politMap->w - 1);
        int py = std::clamp(static_cast<int>(std::round(loc.y)), 0, politMap->h - 1);

        Color pixelColor = getPixel(politMap, px, py);
        std::string countryName = countryData.colorToCountry(pixelColor);

        if (!countryName.empty()) {
            ownerRegions[countryName].push_back(id);
            regData.updateOwner(id, countryName);
            matched++;
        }
    }

    if (SDL_MUSTLOCK(politMap)) SDL_UnlockSurface(politMap);

    printf("[GameScreen] Matched %d regions to %d countries\n", matched, (int)ownerRegions.size());
    fflush(stdout);


    for (auto& [countryName, regions] : ownerRegions) {
        if (countryName.empty()) continue;
        auto country = std::make_unique<Country>(countryName, regions, gs);
        gs.registerCountry(countryName, std::move(country));
    }


    if (!gs.mapViewOnly) {
        printf("[GameScreen] Spawning divisions...\n"); fflush(stdout);
        for (auto& [name, country] : gs.countries) {
            if (country) {
                country->spawnDivisions(gs);
            }
        }
    }


    Country* player = gs.getCountry(gs.controlledCountry);
    if (player && !player->regions.empty()) {

        float sumX = 0, sumY = 0;
        int count = 0;
        for (int rid : player->regions) {
            Vec2 loc = regData.getLocation(rid);
            sumX += loc.x;
            sumY += loc.y;
            count++;
        }
        if (count > 0) {
            camera_.x = -(sumX / count);
            camera_.y = -(sumY / count);
        }


        float minX = 1e9, maxX = -1e9, minY = 1e9, maxY = -1e9;
        for (int rid : player->regions) {
            Vec2 loc = regData.getLocation(rid);
            minX = std::min(minX, loc.x); maxX = std::max(maxX, loc.x);
            minY = std::min(minY, loc.y); maxY = std::max(maxY, loc.y);
        }
        float extent = std::max(maxX - minX, maxY - minY);
        if (extent > 0) {
            float W = static_cast<float>(Engine::instance().WIDTH);
            camera_.zoom = std::clamp((W / extent + 16.0f) / 5.0f, 0.7f, 46.0f);
        } else {
            camera_.zoom = 5.0f;
        }

        printf("[GameScreen] Camera centered on %s at (%.0f, %.0f) zoom=%.1f\n",
               gs.controlledCountry.c_str(), -camera_.x, -camera_.y, camera_.zoom);
        fflush(stdout);
    } else {
        camera_.x = 0.0f;
        camera_.y = -camera_.mapHeight * 0.5f;
        clampCameraToMapBounds(eng.WIDTH, eng.HEIGHT);
    }

    if (!gs.mapViewOnly) {
        printf("[GameScreen] Initializing factories and ports...\n"); fflush(stdout);
        for (auto& [name, country] : gs.countries) {
            if (!country) continue;
            int factoryCount = static_cast<int>(country->regions.size() * country->stability / 400.0f);
            for (int f = 0; f < factoryCount; f++) {
                country->addFactory(gs);
            }
        }


        SDL_Surface* indMap = mapManager_.getIndustryMap();
        if (indMap) {
            if (SDL_MUSTLOCK(indMap)) SDL_LockSurface(indMap);
            for (auto& [name, country] : gs.countries) {
                if (!country || country->regions.size() <= 1) continue;
                int beachTiles = 0;
                for (int r1 : country->regions) {
                    Vec2 loc1 = regData.getLocation(r1);
                    int px1 = std::clamp((int)std::round(loc1.x), 0, indMap->w - 1);
                    int py1 = std::clamp((int)std::round(loc1.y), 0, indMap->h - 1);
                    Color c1 = getPixel(indMap, px1, py1);
                    if (c1.r == 255 && c1.g == 255 && c1.b == 255) {
                        for (int r2 : regData.getConnections(r1)) {
                            Vec2 loc2 = regData.getLocation(r2);
                            int px2 = std::clamp((int)std::round(loc2.x), 0, indMap->w - 1);
                            int py2 = std::clamp((int)std::round(loc2.y), 0, indMap->h - 1);
                            Color c2 = getPixel(indMap, px2, py2);
                            if (c2.r == 126 && c2.g == 142 && c2.b == 158) {
                                beachTiles++;
                                break;
                            }
                        }
                    }
                }
                int portCount = (int)std::ceil(beachTiles / 10.0f);
                for (int p = 0; p < portCount; p++) {
                    country->addPort(gs);
                }
            }
            if (SDL_MUSTLOCK(indMap)) SDL_UnlockSurface(indMap);
        }


        for (auto& [cname, country] : gs.countries) {
            if (!country) continue;
            country->resourceManager.setStartingStockpile(static_cast<int>(country->regions.size()));
            country->resourceManager.tick(*country, 0);
            int civStart = country->buildingManager.countAll(BuildingType::CivilianFactory);
            country->money = 5000.0f * (civStart + 1) * 30.0f;
        }
    }


    mapManager_.reloadIndustryMap(gs);
    mapManager_.generateResourceMap(gs);
    mapRenderer_.invalidateTexture();
    gs.mapDirty = false;

    gs.time.hour = 1;
    gs.time.day = 1;
    gs.time.month = 1;
    gs.time.year = 2026;
    gs.time.startHour = 1;
    gs.time.startDay = 1;
    gs.time.startMonth = 1;
    gs.time.startYear = 2026;
    printf("[GameScreen] Start date: %d/%d/%d %d:00\n", gs.time.day, gs.time.month, gs.time.year, gs.time.hour);
    fflush(stdout);


    std::string factionsPath = eng.assetsPath + "starts/" + scenarioName + "/factions.txt";
    if (fs::exists(factionsPath)) {
        std::ifstream factionsFile(factionsPath);
        std::string line;
        while (std::getline(factionsFile, line)) {
            if (line.empty()) continue;

            auto colonPos = line.find(": ");
            if (colonPos == std::string::npos) continue;
            std::string facName = line.substr(0, colonPos);

            facName.erase(std::remove(facName.begin(), facName.end(), '\''), facName.end());

            std::string membersStr = line.substr(colonPos + 2);

            membersStr.erase(std::remove(membersStr.begin(), membersStr.end(), '['), membersStr.end());
            membersStr.erase(std::remove(membersStr.begin(), membersStr.end(), ']'), membersStr.end());
            membersStr.erase(std::remove(membersStr.begin(), membersStr.end(), '\''), membersStr.end());

            std::vector<std::string> members;
            std::istringstream iss(membersStr);
            std::string token;
            while (std::getline(iss, token, ',')) {

                token.erase(0, token.find_first_not_of(" \t"));
                token.erase(token.find_last_not_of(" \t") + 1);
                if (!token.empty() && gs.getCountry(token)) {
                    members.push_back(token);
                }
            }

            if (!members.empty()) {
                gs.spawnFaction(members, facName);
                printf("[GameScreen] Loaded faction: %s (%d members)\n", facName.c_str(), (int)members.size());
                fflush(stdout);
            }
        }
    }


    gs.canals = {1802, 1868, 2567, 1244, 1212, 1485, 1502, 418};


    {
        int totalDivs = 0;
        for (auto& [n, c] : gs.countries) {
            if (c) totalDivs += (int)c->divisions.size();
        }
        printf("[GameScreen] Total divisions across all countries: %d\n", totalDivs);
        auto* player = gs.getCountry(gs.controlledCountry);
        if (player) {
            printf("[GameScreen] %s: pop=%s, mp=%.0f, divs=%d, factories=%d, regions=%d\n",
                   gs.controlledCountry.c_str(), Helpers::prefixNumber(player->population).c_str(),
                   player->manPower, (int)player->divisions.size(), player->factories, (int)player->regions.size());
        }
        fflush(stdout);
    }


    mapManager_.generateResourceMap(gs);

    if (gs.mapViewOnly) {
        gs.speed = 0;
    } else if (isSpectatorMode(gs)) {
        gs.speed = std::max(gs.speed, 1);
    }
    gs.inGame = true;


    uiManager_->topBar.rebuild(app, gs, eng.WIDTH);


    openedTab_ = "political";
    clicked_ = gs.controlledCountry;
    sideBarAnimation_ = 0.0f;
    if (sidebar_) {
        sidebar_->activeTab = "political";
        sidebar_->selectedCountry.clear();
        sidebar_->clearOverlayInfo();
        sidebar_->animation = 0.0f;
        if (!gs.mapViewOnly && !gs.controlledCountry.empty()) {
            sidebar_->animation = 1.0f;
            sidebar_->open();
            sideBarAnimation_ = 1.0f;
        }
    }


    std::string musicPath = eng.assetsPath + "music/gameMusic.mp3";
    if (fs::exists(musicPath)) {
        Audio::instance().playMusic(musicPath, true, 2000);
    }

    printf("[GameScreen] Game started: %s (player: %s, %d countries)\n",
           scenarioName.c_str(), gs.controlledCountry.c_str(),
           (int)gs.countries.size());
    fflush(stdout);

    if (gs.mapViewOnly) {
        Audio::instance().playSound("clickedSound");
        toasts().show("Opened map: " + scenarioName);
    } else if (isSpectatorMode(gs)) {
        Audio::instance().playSound("startGameSound");
        toasts().show("Spectator mode: " + scenarioName);
    } else {
        Audio::instance().playSound("startGameSound");
        toasts().show("Game started: " + scenarioName);
    }


    if (!gs.mapViewOnly && !gs.controlledCountry.empty()) {
        showStartPopup(gs);
    }
}


bool GameScreen::loadGame(App& app, const std::string& saveName) {
    auto& eng = Engine::instance();
    auto& gs = app.gameState();

    resetSessionState();

    SaveSystem loader;
    if (!loader.loadGame(gs, mapManager_, saveName)) {
        toasts().show("Load failed: " + saveName);
        return false;
    }

    gs.mapViewOnly = false;

    camera_.mapWidth = static_cast<float>(mapManager_.mapWidth());
    camera_.mapHeight = static_cast<float>(mapManager_.mapHeight());
    camera_.x = 0.0f;
    camera_.y = 0.0f;
    camera_.zoom = 1.0f;

    auto& regData = RegionData::instance();
    Country* player = gs.getCountry(gs.controlledCountry);
    if (player && !player->regions.empty()) {
        float sumX = 0.0f;
        float sumY = 0.0f;
        for (int rid : player->regions) {
            Vec2 loc = regData.getLocation(rid);
            sumX += loc.x;
            sumY += loc.y;
        }
        const float count = static_cast<float>(player->regions.size());
        camera_.x = -(sumX / count);
        camera_.y = -(sumY / count);

        float minX = 1e9f, maxX = -1e9f, minY = 1e9f, maxY = -1e9f;
        for (int rid : player->regions) {
            Vec2 loc = regData.getLocation(rid);
            minX = std::min(minX, loc.x);
            maxX = std::max(maxX, loc.x);
            minY = std::min(minY, loc.y);
            maxY = std::max(maxY, loc.y);
        }
        float extent = std::max(maxX - minX, maxY - minY);
        if (extent > 0.0f) {
            float W = static_cast<float>(eng.WIDTH);
            camera_.zoom = std::clamp((W / extent + 16.0f) / 5.0f, 0.7f, 46.0f);
        }
    }
    clampCameraToMapBounds(eng.WIDTH, eng.HEIGHT);

    selectedDivisions_.clear();
    selectedRegions_.clear();
    pressed_ = false;
    clicked_ = gs.controlledCountry;
    openedTab_ = "political";
    sideBarAnimation_ = 1.0f;
    if (sidebar_) {
        sidebar_->animation = 1.0f;
        sidebar_->open();
        sidebar_->activeTab = "political";
    }
    if (uiManager_) {
        uiManager_->popupList.clear();
        uiManager_->topBar.rebuild(app, gs, eng.WIDTH);
    }

    mapRenderer_.invalidateTexture();
    mapManager_.markDirty();

    std::string musicPath = eng.assetsPath + "music/gameMusic.mp3";
    if (fs::exists(musicPath)) {
        Audio::instance().playMusic(musicPath, true, 2000);
    }

    toasts().show("Loaded: " + saveName);
    return true;
}


void GameScreen::showTutorialPopup(GameState& gs) {
    std::string country = replaceAll(gs.controlledCountry, "_", " ");

    std::vector<std::string> text = {
        "Our country, " + country + ", awaits your leadership.",
        "",
        "WASD: Camera | Tab: Sidebar | Esc: Menu",
        "1-6: Map Modes | F4: Tutorial | F5-F8: Stats",
        "Left Click: Select | Right Click: Move Troops",
        "Scroll/Brackets: Zoom | Shift: Fast Camera",
        "",
        "Open sidebar for politics, military, industry.",
        "Right-click regions to build (industry tab).",
        "Drag-select troops, right-click to move them.",
    };

    text.push_back("");
    text.push_back("[Press ESC or click this panel to dismiss]");

    float popupH = (float)(text.size()) * 0.85f + 3.0f;

    auto popup = std::make_unique<Popup>(
        "A New Dawn", text,
        std::vector<PopupButton>{},
        22.0f, popupH,
        -1.0f, -1.0f,
        "", "", true
    );
    uiManager_->popupList.push_back(std::move(popup));
}

void GameScreen::showStartPopup(GameState& gs) {
    showTutorialPopup(gs);
}

void GameScreen::resetSessionState() {
    clicked_.clear();
    openedTab_.clear();
    openedPoliticalTab_.clear();
    currentlyBuilding_ = "civilian_factory";
    showDivisions_ = true;
    showCities_ = true;
    showResources_ = false;
    showUI_ = true;
    oldDivisions_ = false;
    sideBarAnimation_ = 0.0f;
    sideBarSize_ = 0.2f;
    sideBarScroll_ = 0.0f;
    holdingSideBar_ = false;

    selectedRegions_.clear();
    selectedDivisions_.clear();
    xPressed_ = 0;
    yPressed_ = 0;
    timePressed_ = 0;
    pressed_ = false;

    speedMinusRect_ = {0, 0, 0, 0};
    speedPlusRect_ = {0, 0, 0, 0};

    camera_ = Camera{};
    clickRegistry_.clear();

    uiManager_ = std::make_unique<UIManager>();
    sidebar_ = std::make_unique<Sidebar>();
}
