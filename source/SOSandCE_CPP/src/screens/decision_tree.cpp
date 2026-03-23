#include "screens/decision_tree.h"
#include "core/app.h"
#include "core/engine.h"
#include "core/audio.h"
#include "core/input.h"
#include "ui/primitives.h"
#include "ui/theme.h"
#include "ui/ui_assets.h"
#include "ui/toast.h"
#include "game/game_state.h"
#include "game/country.h"
#include "game/focus_tree.h"
#include "game/helpers.h"

#include <limits>
#include <sstream>

namespace {
std::vector<std::string> wrapNodeLabel(const std::string& text, int fontSize, int maxWidth, int maxLines = 2) {
    std::vector<std::string> lines;
    auto* font = Engine::instance().getFont(fontSize);
    if (!font || maxWidth <= 0) {
        lines.push_back(text);
        return lines;
    }

    std::istringstream iss(text);
    std::string word;
    std::string currentLine;

    auto textWidth = [&](const std::string& value) -> int {
        int w = 0;
        TTF_SizeUTF8(font, value.c_str(), &w, nullptr);
        return w;
    };

    while (iss >> word) {
        std::string trial = currentLine.empty() ? word : currentLine + " " + word;
        if (currentLine.empty() || textWidth(trial) <= maxWidth) {
            currentLine = trial;
            continue;
        }

        lines.push_back(currentLine);
        currentLine = word;
        if (static_cast<int>(lines.size()) >= maxLines - 1) {
            break;
        }
    }

    if (!currentLine.empty()) {
        lines.push_back(currentLine);
    }

    if (static_cast<int>(lines.size()) > maxLines) {
        lines.resize(maxLines);
    }

    if (!lines.empty() && !currentLine.empty() && static_cast<int>(lines.size()) == maxLines) {
        int lastIndex = static_cast<int>(lines.size()) - 1;
        std::string ellipsized = lines[lastIndex];
        while (!ellipsized.empty() && textWidth(ellipsized + "...") > maxWidth) {
            ellipsized.pop_back();
        }
        lines[lastIndex] = ellipsized.empty() ? "..." : ellipsized + "...";
    }

    return lines;
}

struct DecisionTreeLayout {
    float u = 0.0f;
    float headerH = 0.0f;
    float footH = 0.0f;
    float contentX = 0.0f;
    float contentY = 0.0f;
    float contentW = 0.0f;
    float contentH = 0.0f;
    float padding = 0.0f;
    float nodeW = 0.0f;
    float nodeH = 0.0f;
    float scaleFactor = 0.0f;
    float ySpacing = 0.0f;
    float minWorldX = 0.0f;
    float minWorldY = 0.0f;
    float treeWidth = 0.0f;
    float treeHeight = 0.0f;
    float minPanX = 0.0f;
    float minPanY = 0.0f;
    std::unordered_map<int, float> laneX;
    std::unordered_map<int, float> laneY;
};

DecisionTreeLayout buildTreeLayout(const Country* player, int screenW, int screenH) {
    DecisionTreeLayout layout;
    layout.u = screenH / 100.0f * Engine::instance().uiScaleFactor();
    layout.headerH = std::round(layout.u * 5.0f);
    layout.footH = std::round(layout.u * 3.0f);
    layout.contentX = std::round(layout.u * 1.2f);
    layout.contentY = layout.headerH + std::round(layout.u * 1.2f);
    layout.contentW = screenW - layout.contentX * 2.0f;
    layout.contentH = screenH - layout.contentY - layout.footH - std::round(layout.u * 1.2f);
    layout.padding = std::round(layout.u * 1.4f);

    std::vector<int> xKeys;
    std::vector<int> yKeys;
    if (player) {
        xKeys.reserve(player->focusTreeEngine.nodes.size());
        yKeys.reserve(player->focusTreeEngine.nodes.size());
        for (const auto& node : player->focusTreeEngine.nodes) {
            xKeys.push_back((int)std::lround(node.position.x));
            yKeys.push_back((int)std::lround(node.position.y));
        }
    }
    std::sort(xKeys.begin(), xKeys.end());
    xKeys.erase(std::unique(xKeys.begin(), xKeys.end()), xKeys.end());
    std::sort(yKeys.begin(), yKeys.end());
    yKeys.erase(std::unique(yKeys.begin(), yKeys.end()), yKeys.end());

    const size_t laneCount = xKeys.empty() ? 1 : xKeys.size();
    layout.nodeW = std::round(layout.u * (laneCount > 26 ? 8.8f : (laneCount > 18 ? 9.4f : 10.2f)));
    layout.nodeH = std::round(layout.u * (laneCount > 22 ? 2.8f : 3.1f));
    layout.scaleFactor = layout.nodeW + std::round(layout.u * (laneCount > 22 ? 0.8f : 1.2f));
    layout.ySpacing = layout.nodeH + std::round(layout.u * 1.45f);

    auto buildLaneMap = [](const std::vector<int>& values, float stepBase, float spacing,
                           std::unordered_map<int, float>& out) {
        if (values.empty()) {
            out[0] = 0.0f;
            return;
        }
        float cursor = 0.0f;
        out[values.front()] = 0.0f;
        for (size_t i = 1; i < values.size(); ++i) {
            float rawGap = static_cast<float>(values[i] - values[i - 1]);
            float laneGap = std::max(1.0f, std::round(rawGap / std::max(1.0f, stepBase)));
            cursor += laneGap * spacing;
            out[values[i]] = cursor;
        }
    };

    buildLaneMap(xKeys, 200.0f, layout.scaleFactor, layout.laneX);
    buildLaneMap(yKeys, 1.0f, layout.ySpacing, layout.laneY);

    float minX = std::numeric_limits<float>::max();
    float minY = std::numeric_limits<float>::max();
    float maxX = std::numeric_limits<float>::lowest();
    float maxY = std::numeric_limits<float>::lowest();

    if (player) {
        for (const auto& node : player->focusTreeEngine.nodes) {
            int xKey = (int)std::lround(node.position.x);
            int yKey = (int)std::lround(node.position.y);
            float wx = layout.laneX.count(xKey) ? layout.laneX[xKey] : (node.position.x / 200.0f) * layout.scaleFactor;
            float wy = layout.laneY.count(yKey) ? layout.laneY[yKey] : node.position.y * layout.ySpacing;
            minX = std::min(minX, wx);
            minY = std::min(minY, wy);
            maxX = std::max(maxX, wx + layout.nodeW);
            maxY = std::max(maxY, wy + layout.nodeH);
        }
    }

    if (minX == std::numeric_limits<float>::max()) {
        minX = minY = 0.0f;
        maxX = layout.nodeW;
        maxY = layout.nodeH;
    }

    layout.minWorldX = minX;
    layout.minWorldY = minY;
    layout.treeWidth = std::max(layout.nodeW, maxX - minX);
    layout.treeHeight = std::max(layout.nodeH, maxY - minY);
    layout.minPanX = std::min(0.0f, layout.contentW - (layout.treeWidth + layout.padding * 2.0f));
    layout.minPanY = std::min(0.0f, layout.contentH - (layout.treeHeight + layout.padding * 2.0f));

    return layout;
}

void clampTreeCamera(Camera& cam, const DecisionTreeLayout& layout) {
    cam.zoom = 1.0f;
    cam.x = std::clamp(cam.x, layout.minPanX, 0.0f);
    cam.y = std::clamp(cam.y, layout.minPanY, 0.0f);
}

std::pair<float, float> focusToScreen(float fx, float fy,
                                      const DecisionTreeLayout& layout,
                                      const Camera& cam) {
    int xKey = (int)std::lround(fx);
    int yKey = (int)std::lround(fy);
    float wx = layout.laneX.count(xKey) ? layout.laneX.at(xKey) : (fx / 200.0f) * layout.scaleFactor;
    float wy = layout.laneY.count(yKey) ? layout.laneY.at(yKey) : fy * layout.ySpacing;
    float sx = layout.contentX + layout.padding + (wx - layout.minWorldX) + cam.x;
    float sy = layout.contentY + layout.padding + (wy - layout.minWorldY) + cam.y;
    return {sx, sy};
}
}


void DecisionTreeScreen::enter(App& app) {
    hoveredFocus_ = -1;
    selectedFocus_.clear();


    treeCam_.x = 0;
    treeCam_.y = 0;
    treeCam_.zoom = 1.0f;
    treeCam_.mapWidth = 0.0f;
    treeCam_.mapHeight = 0.0f;
}


void DecisionTreeScreen::handleInput(App& app, const InputState& input) {
    auto& eng = Engine::instance();
    auto& gs = app.gameState();
    int W = eng.WIDTH;
    int H = eng.HEIGHT;
    Country* player = gs.getCountry(gs.controlledCountry);
    float u = H / 100.0f * Engine::instance().uiScaleFactor();
    float headerH = std::round(u * 5.0f);
    float backW = std::round(u * 8.0f);
    float backH = std::round(headerH * 0.65f);
    float backX = std::round(u * 1.5f);
    float backY = (headerH - backH) / 2.0f;
    DecisionTreeLayout layout = buildTreeLayout(player, W, H);


    treeCam_.zoom = 1.0f;

    if (input.mouseLeftDown &&
        input.mouseX >= backX && input.mouseX <= backX + backW &&
        input.mouseY >= backY && input.mouseY <= backY + backH) {
        Audio::instance().playSound("clickedSound");
        nextScreen = ScreenType::GAME;
        return;
    }


    float panSpeed = 300.0f;
    if (input.isKeyHeld(SDL_SCANCODE_W) || input.isKeyHeld(SDL_SCANCODE_UP))
        treeCam_.y += panSpeed * 0.016f;
    if (input.isKeyHeld(SDL_SCANCODE_S) || input.isKeyHeld(SDL_SCANCODE_DOWN))
        treeCam_.y -= panSpeed * 0.016f;
    if (input.isKeyHeld(SDL_SCANCODE_A) || input.isKeyHeld(SDL_SCANCODE_LEFT))
        treeCam_.x += panSpeed * 0.016f;
    if (input.isKeyHeld(SDL_SCANCODE_D) || input.isKeyHeld(SDL_SCANCODE_RIGHT))
        treeCam_.x -= panSpeed * 0.016f;

    if (input.mouseMiddle) {
        treeCam_.x += static_cast<float>(input.mouseRelX);
        treeCam_.y += static_cast<float>(input.mouseRelY);
    }


    if (input.scrollY != 0) {
        treeCam_.y += static_cast<float>(input.scrollY) * std::round(u * 2.0f);
    }

    clampTreeCamera(treeCam_, layout);


    if (input.mouseLeftDown && player) {
        for (auto& [focusName, focusData] : player->decisionTree) {
            auto* fnClick = player->focusTreeEngine.getNode(focusName);
            if (!fnClick) continue;
            auto [nodeX, nodeY] = focusToScreen(fnClick->position.x, fnClick->position.y, layout, treeCam_);

            if (input.mouseX >= nodeX && input.mouseX <= nodeX + layout.nodeW &&
                input.mouseY >= nodeY && input.mouseY <= nodeY + layout.nodeH) {

                if (!player->focus.has_value() && player->focusTreeEngine.canStartFocus(focusName, player)) {
                    auto* node = player->focusTreeEngine.getNode(focusName);
                    if (node) {

                        player->politicalPower -= static_cast<float>(node->cost);


                        player->focusTreeEngine.completedFocuses.insert(focusName);


                        if (!node->mutuallyExclusive.empty()) {
                            for (auto& mx : node->mutuallyExclusive) {
                                auto* mxNode = player->focusTreeEngine.getNode(mx);
                                if (mxNode) mxNode->available = false;
                            }
                        }


                        std::vector<std::string> effectStrs;
                        for (auto& eff : node->effects) {
                            if (auto* s = std::get_if<std::string>(&eff)) effectStrs.push_back(*s);
                        }
                        player->focus = std::make_tuple(focusName, node->days, effectStrs);

                        Audio::instance().playSound("clickedSound");
                        toasts().show("Started focus: " + focusName, 2000);
                    } else {
                        Audio::instance().playSound("failedClickSound");
                    }
                } else {
                    Audio::instance().playSound("failedClickSound");
                }
                break;
            }

        }
    }


    if (input.isKeyDown(SDL_SCANCODE_ESCAPE)) {
        nextScreen = ScreenType::GAME;
    }
}


void DecisionTreeScreen::update(App& app, float dt) {

}


void DecisionTreeScreen::render(App& app) {
    auto& eng = Engine::instance();
    auto& gs = app.gameState();
    int W = eng.WIDTH;
    int H = eng.HEIGHT;
    auto* r = eng.renderer;
    float u = H / 100.0f * Engine::instance().uiScaleFactor();
    auto& assets = UIAssets::instance();
    Country* player = gs.getCountry(gs.controlledCountry);
    DecisionTreeLayout layout = buildTreeLayout(player, W, H);
    clampTreeCamera(treeCam_, layout);

    eng.clear(Theme::bg_dark);
    UIPrim::drawVignette(r, W, H, 60);

    if (!player) {
        UIPrim::drawText(r, "No country selected", std::max(16, (int)(u * 1.5f)),
                         W * 0.5f, H * 0.5f, "center", Theme::grey);
        return;
    }


    float headerH = layout.headerH;
    UIPrim::drawRectFilled(r, {14, 16, 20}, 0, 0, (float)W, headerH);
    SDL_Texture* headerTex = assets.panelHeader();
    if (headerTex) UIAssets::draw9Slice(r, headerTex, 0, 0, (float)W, headerH, 14);


    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, 255, 255, 255, 5);
    SDL_Rect hhl = {0, 0, W, (int)(headerH * 0.3f)};
    SDL_RenderFillRect(r, &hhl);

    std::string titleStr = "NATIONAL FOCUS TREE - " + replaceAll(player->name, "_", " ");
    int titleFs = std::max(16, (int)(headerH * 0.38f));
    UIPrim::drawText(r, titleStr, titleFs, W * 0.5f, headerH * 0.5f, "center", Theme::gold_bright, true);
    UIPrim::drawHLine(r, Theme::gold, 0, (float)W, headerH - 1, 2);


    int mx, my; SDL_GetMouseState(&mx, &my);
    float backW = std::round(u * 8.0f), backH = std::round(headerH * 0.65f);
    float backX = std::round(u * 1.5f), backY = (headerH - backH) / 2;
    UIPrim::drawMenuButton(r, backX, backY, backW, backH, "Back", mx, my, false, "", std::max(12, (int)(backH * 0.38f)));


    const auto& tree = player->decisionTree;

    if (tree.empty()) {

        SDL_Texture* infoIcon = assets.icon("Info");
        if (infoIcon) {
            float iSz = std::round(u * 4.0f);
            SDL_SetTextureColorMod(infoIcon, 50, 50, 60);
            SDL_Rect iDst = {(int)(W/2 - iSz/2), (int)(H * 0.38f - iSz/2), (int)iSz, (int)iSz};
            SDL_RenderCopy(r, infoIcon, nullptr, &iDst);
            SDL_SetTextureColorMod(infoIcon, 255, 255, 255);
        }
        int emFs = std::max(16, (int)(u * 1.4f));
        UIPrim::drawText(r, "No focus tree available for this country.", emFs,
                         W * 0.5f, H * 0.48f, "center", Theme::grey);
        int subFs = std::max(13, (int)(u * 1.0f));
        UIPrim::drawText(r, "Press ESC or click Back to return.", subFs,
                         W * 0.5f, H * 0.55f, "center", Theme::dark_grey);
        return;
    }


    UIPrim::drawRoundedRect(r, {6, 8, 12}, layout.contentX, layout.contentY,
                            layout.contentW, layout.contentH, 8, Theme::border);
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, 255, 255, 255, 4);
    SDL_Rect contentHighlight = {
        (int)layout.contentX + 2,
        (int)layout.contentY + 2,
        (int)layout.contentW - 4,
        (int)(layout.contentH * 0.16f)
    };
    SDL_RenderFillRect(r, &contentHighlight);

    std::unordered_map<std::string, std::pair<float, float>> nodePositions;
    for (auto& fnode : player->focusTreeEngine.nodes) {
        auto [sx, sy] = focusToScreen(fnode.position.x, fnode.position.y, layout, treeCam_);
        nodePositions[fnode.name] = {sx + layout.nodeW / 2, sy + layout.nodeH / 2};
    }

    SDL_Rect treeClip = {
        static_cast<int>(layout.contentX),
        static_cast<int>(layout.contentY),
        static_cast<int>(layout.contentW),
        static_cast<int>(layout.contentH)
    };
    SDL_RenderSetClipRect(r, &treeClip);

    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, Theme::border.r, Theme::border.g, Theme::border.b, 18);
    for (const auto& [laneKey, lanePos] : layout.laneX) {
        float guideX = layout.contentX + layout.padding + (lanePos - layout.minWorldX) + treeCam_.x + layout.nodeW * 0.5f;
        if (guideX < layout.contentX || guideX > layout.contentX + layout.contentW) continue;
        SDL_RenderDrawLine(r, (int)guideX, (int)layout.contentY, (int)guideX, (int)(layout.contentY + layout.contentH));
    }
    SDL_SetRenderDrawColor(r, 255, 255, 255, 10);
    for (const auto& [laneKey, lanePos] : layout.laneY) {
        float guideY = layout.contentY + layout.padding + (lanePos - layout.minWorldY) + treeCam_.y + layout.nodeH * 0.5f;
        if (guideY < layout.contentY || guideY > layout.contentY + layout.contentH) continue;
        SDL_RenderDrawLine(r, (int)layout.contentX, (int)guideY, (int)(layout.contentX + layout.contentW), (int)guideY);
    }


    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    for (auto& fnode : player->focusTreeEngine.nodes) {
        auto it1 = nodePositions.find(fnode.name);
        if (it1 == nodePositions.end()) continue;
        for (auto& prereq : fnode.prerequisites) {
            auto it2 = nodePositions.find(prereq);
            if (it2 == nodePositions.end()) continue;
            bool met = player->focusTreeEngine.completedFocuses.count(prereq) > 0;
            Color lineColor = met ? Theme::green : Theme::red;
            SDL_SetRenderDrawColor(r, lineColor.r, lineColor.g, lineColor.b, 180);
            int lineW2 = std::max(1, (int)(u * 0.3f));
            for (int t = -lineW2/2; t <= lineW2/2; t++) {
                SDL_RenderDrawLine(r, (int)it1->second.first, (int)it1->second.second + t,
                                  (int)it2->second.first, (int)it2->second.second + t);
            }
        }
    }

    int col = 0;
    int row2 = 0;

    for (auto& [focusName, focusData] : tree) {
        auto* fnodePos = player->focusTreeEngine.getNode(focusName);
        float nodeX, nodeY;
        if (fnodePos) {
            auto [sx, sy] = focusToScreen(fnodePos->position.x, fnodePos->position.y, layout, treeCam_);
            nodeX = sx;
            nodeY = sy;
        } else {
            auto [sx, sy] = focusToScreen(static_cast<float>(col * 200), static_cast<float>(row2), layout, treeCam_);
            nodeX = sx;
            nodeY = sy;
        }


        if (nodeX + layout.nodeW < layout.contentX || nodeX > layout.contentX + layout.contentW ||
            nodeY + layout.nodeH < layout.contentY || nodeY > layout.contentY + layout.contentH) {
            col++;
            continue;
        }

        bool isSelected = (focusName == selectedFocus_);
        bool isActive = player->focus.has_value() &&
                        std::get<0>(*player->focus) == focusName;
        bool isHov = mx >= nodeX && mx <= nodeX + layout.nodeW && my >= nodeY && my <= nodeY + layout.nodeH;


        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(r, 0, 0, 0, 30);
        SDL_Rect nsh = {(int)nodeX + 2, (int)nodeY + 3, (int)layout.nodeW, (int)layout.nodeH};
        SDL_RenderFillRect(r, &nsh);


        bool isCompleted = player->focusTreeEngine.completedFocuses.count(focusName) > 0;
        bool canStart = player->focusTreeEngine.canStartFocus(focusName);
        bool tooExpensive = false;
        if (canStart) {
            auto* fnode = player->focusTreeEngine.getNode(focusName);


        }


        Color nodeBg, nodeBrd;
        if (isCompleted) {
            nodeBg = {25, 55, 30}; nodeBrd = Theme::green;
        } else if (isActive) {
            nodeBg = {35, 50, 40}; nodeBrd = Theme::gold;
        } else if (canStart) {
            nodeBg = isHov ? Color{45, 48, 58} : Theme::btn; nodeBrd = isHov ? Theme::gold_dim : Theme::border;
        } else {
            nodeBg = {50, 20, 20}; nodeBrd = Theme::red;
        }
        UIPrim::drawRoundedRect(r, nodeBg, nodeX, nodeY, layout.nodeW, layout.nodeH, 5, nodeBrd);


        if (isActive) {
            SDL_SetRenderDrawColor(r, Theme::gold.r, Theme::gold.g, Theme::gold.b, 200);
            SDL_Rect ab1 = {(int)nodeX+1, (int)nodeY+1, (int)layout.nodeW-2, (int)layout.nodeH-2};
            SDL_RenderDrawRect(r, &ab1);
        }


        SDL_SetRenderDrawColor(r, 255, 255, 255, isHov ? 8 : 3);
        SDL_Rect nhl = {(int)nodeX + 3, (int)nodeY + 2, (int)layout.nodeW - 6, (int)(layout.nodeH * 0.25f)};
        SDL_RenderFillRect(r, &nhl);


        if (isActive) {
            SDL_SetRenderDrawColor(r, Theme::gold.r, Theme::gold.g, Theme::gold.b, 200);
            SDL_Rect lbar = {(int)nodeX + 2, (int)nodeY + 4, 4, (int)layout.nodeH - 8};
            SDL_RenderFillRect(r, &lbar);
        }


        float baseFontSize = std::max(10.0f, std::round(u * 1.0f));
        int labelFontSize = static_cast<int>(baseFontSize);
        auto wrappedName = wrapNodeLabel(titleCase(focusName), labelFontSize, static_cast<int>(layout.nodeW - u * 0.8f));
        if (wrappedName.size() > 1) {
            labelFontSize = std::max(8, static_cast<int>(std::round(baseFontSize * 0.82f)));
            wrappedName = wrapNodeLabel(titleCase(focusName), labelFontSize, static_cast<int>(layout.nodeW - u * 0.8f));
        }
        Color nameC = isCompleted ? Theme::green_light :
                      isActive ? Theme::gold_bright :
                      canStart ? (isHov ? Theme::cream : Color{185, 180, 170}) :
                      Theme::red_light;

        float lineGap = std::max(8.0f, std::round(labelFontSize * 0.95f));
        float labelCenterY = nodeY + (wrappedName.size() > 1 ? layout.nodeH * 0.30f : layout.nodeH * 0.38f);
        for (size_t lineIndex = 0; lineIndex < wrappedName.size(); ++lineIndex) {
            float lineY = labelCenterY + static_cast<float>(lineIndex) * lineGap;
            UIPrim::drawText(r, wrappedName[lineIndex],
                             labelFontSize,
                             nodeX + layout.nodeW * 0.5f, lineY,
                             "center", nameC, isActive);
        }


        if (!focusData.empty()) {

            for (auto& val : focusData) {
                if (std::holds_alternative<int>(val)) {
                    int days = std::get<int>(val);
                    if (days > 0 && days < 9999) {
                        float subFs = std::max(7.0f, std::round(u * (wrappedName.size() > 1 ? 0.58f : 0.7f)));
                        UIPrim::drawText(r, std::to_string(days) + " days", (int)subFs,
                                         nodeX + layout.nodeW * 0.5f, nodeY + layout.nodeH * (wrappedName.size() > 1 ? 0.80f : 0.70f),
                                         "center", Theme::grey);
                        break;
                    }
                }
            }
        }

        col++;
    }

    SDL_RenderSetClipRect(r, nullptr);


    {

        for (auto& [fn, fd] : tree) {
            auto* fnTip = player->focusTreeEngine.getNode(fn);
            float nx = 0, ny = 0;
            if (fnTip) {
                auto [tsx, tsy] = focusToScreen(fnTip->position.x, fnTip->position.y, layout, treeCam_);
                nx = tsx; ny = tsy;
            }
            if (mx >= nx && mx <= nx + layout.nodeW && my >= ny && my <= ny + layout.nodeH) {

                auto* fnode = player->focusTreeEngine.getNode(fn);
                bool fc = player->focusTreeEngine.completedFocuses.count(fn) > 0;
                bool fa = player->focusTreeEngine.canStartFocus(fn);

                std::string statusStr = fc ? "[COMPLETED]" : (fa ? "[AVAILABLE]" : "[LOCKED]");
                Color statusColor = fc ? Theme::green_light : (fa ? Theme::blue : Theme::red_light);

                int tipFs = std::max(11, (int)(u * 0.9f));
                float tipX = nx + layout.nodeW + std::round(u * 1.0f);
                float tipY = ny;
                if (tipX + u * 20 > W) tipX = nx - u * 20 - u;


                float tipW = std::round(u * 20.0f);
                float tipH = std::round(u * 10.0f);
                UIPrim::drawRoundedRect(r, Theme::header, tipX, tipY, tipW, tipH, 4, Theme::gold_dim);

                float ty = tipY + std::round(u * 0.8f);
                UIPrim::drawText(r, statusStr, tipFs, tipX + std::round(u * 0.8f), ty, "midleft", statusColor);
                ty += std::round(u * 1.8f);


                if (fnode) {
                    char costBuf[64];
                    snprintf(costBuf, sizeof(costBuf), "%d PP | %d days", fnode->cost, fnode->days);
                    UIPrim::drawText(r, costBuf, tipFs, tipX + std::round(u * 0.8f), ty, "midleft", Theme::cream);
                    ty += std::round(u * 1.8f);
                }


                if (fnode && !fnode->description.empty()) {
                    UIPrim::drawText(r, fnode->description, std::max(9, tipFs - 1),
                                     tipX + std::round(u * 0.8f), ty, "midleft", Theme::gold);
                    ty += std::round(u * 1.8f);
                }


                if (fnode && !fnode->prerequisites.empty()) {
                    std::string prereqStr = "Requires: ";
                    for (size_t pi = 0; pi < fnode->prerequisites.size(); pi++) {
                        if (pi > 0) prereqStr += ", ";
                        prereqStr += fnode->prerequisites[pi];
                    }
                    bool allMet = true;
                    for (auto& p : fnode->prerequisites) {
                        if (player->focusTreeEngine.completedFocuses.count(p) == 0) allMet = false;
                    }
                    UIPrim::drawText(r, prereqStr, std::max(9, tipFs - 1),
                                     tipX + std::round(u * 0.8f), ty, "midleft",
                                     allMet ? Theme::grey : Theme::red_light);
                }
                break;
            }
        }
    }


    float footH = layout.footH;
    UIPrim::drawRectFilled(r, {10, 12, 16, 200}, 0, H - footH, (float)W, footH);
    UIPrim::drawHLine(r, Theme::border, 0, (float)W, H - footH, 1);
    int instFs = std::max(12, (int)(u * 0.9f));
    UIPrim::drawText(r, "WASD / Arrow Keys or middle mouse to pan  |  Scroll to scroll  |  ESC to return",
                     instFs, W * 0.5f, H - footH * 0.5f, "center", Theme::dark_grey);
}
