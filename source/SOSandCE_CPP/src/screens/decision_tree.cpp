#include "screens/decision_tree.h"
#include "core/app.h"
#include "core/engine.h"
#include "core/audio.h"
#include "core/input.h"
#include "ui/primitives.h"
#include "ui/anim.h"
#include "ui/theme.h"
#include "ui/hoi4.h"
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
    float nodePad = 0.0f;
    int titleFs = 10;
    int captionFs = 8;
    float titleLineH = 0.0f;
    float captionH = 0.0f;
    float scaleFactor = 0.0f;
    float ySpacing = 0.0f;
    float minWorldX = 0.0f;
    float minWorldY = 0.0f;
    float treeWidth = 0.0f;
    float treeHeight = 0.0f;
    float minPanX = 0.0f;
    float minPanY = 0.0f;
    float centerX = 0.0f;
    float centerY = 0.0f;
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
    layout.nodeW = std::round(layout.u * (laneCount > 26 ? 11.0f : (laneCount > 18 ? 11.8f : 12.5f)));
    layout.titleFs = std::max(11, (int)(layout.u * 1.05f));
    layout.captionFs = std::max(9, (int)(layout.u * 0.8f));
    layout.titleLineH = (float)UIPrim::textHeight(layout.titleFs);
    layout.captionH = (float)UIPrim::textHeight(layout.captionFs);
    layout.nodePad = std::round(layout.u * 0.5f);
    layout.nodeH = std::round(layout.nodePad + layout.titleLineH * 2.0f +
                              layout.u * 0.15f + layout.captionH + layout.nodePad);
    layout.scaleFactor = layout.nodeW + std::round(layout.u * (laneCount > 22 ? 1.0f : 1.4f));
    layout.ySpacing = layout.nodeH + std::round(layout.u * 1.8f);

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
    layout.centerX = std::max(0.0f, (layout.contentW - (layout.treeWidth + layout.padding * 2.0f)) * 0.5f);
    layout.centerY = std::max(0.0f, (layout.contentH - (layout.treeHeight + layout.padding * 2.0f)) * 0.5f);

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
    float sx = layout.contentX + layout.padding + layout.centerX + (wx - layout.minWorldX) + cam.x;
    float sy = layout.contentY + layout.padding + layout.centerY + (wy - layout.minWorldY) + cam.y;
    return {sx, sy};
}
}


void DecisionTreeScreen::enter(App& app) {
    UIAnim::trigger("screen:dtree");
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
    UIAnim::pushMenuTransition(r, "screen:dtree", W * 0.5f, H * 0.5f);

    if (!player) {
        UIPrim::drawText(r, "No country selected", std::max(16, (int)(u * 1.5f)),
                         W * 0.5f, H * 0.5f, "center", Theme::grey);
        UIAnim::popMenuTransition(r);
        UIAnim::drawTransitionFade(r, "screen:dtree", W, H);
        return;
    }


    float headerH = layout.headerH;
    UIPrim::drawGradientV(r, 0, 0, (float)W, headerH, {54, 58, 52, 255}, {34, 37, 33, 255});

    std::string titleStr = HOI4::tracked("National Focus Tree") + "  -  " +
                           HOI4::caps(replaceAll(player->name, "_", " "));
    int titleFs = std::max(16, (int)(headerH * 0.38f));
    UIPrim::drawText(r, UIPrim::fitText(titleStr, titleFs, W * 0.7f), titleFs,
                     W * 0.5f, headerH * 0.5f, "center", Theme::gold_bright, true);
    HOI4::goldDivider(r, 0, (float)W, headerH - 1);


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
        UIAnim::popMenuTransition(r);
        UIAnim::drawTransitionFade(r, "screen:dtree", W, H);
        return;
    }


    HOI4::inset(r, layout.contentX, layout.contentY, layout.contentW, layout.contentH);
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
        nodePositions[fnode.name] = {sx, sy};
    }

    SDL_Rect treeClip = {
        static_cast<int>(layout.contentX),
        static_cast<int>(layout.contentY),
        static_cast<int>(layout.contentW),
        static_cast<int>(layout.contentH)
    };
    SDL_RenderSetClipRect(r, &treeClip);

    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, Theme::border.r, Theme::border.g, Theme::border.b, 14);
    for (const auto& [laneKey, lanePos] : layout.laneX) {
        float guideX = layout.contentX + layout.padding + layout.centerX +
                       (lanePos - layout.minWorldX) + treeCam_.x + layout.nodeW * 0.5f;
        if (guideX < layout.contentX || guideX > layout.contentX + layout.contentW) continue;
        SDL_RenderDrawLine(r, (int)guideX, (int)layout.contentY, (int)guideX, (int)(layout.contentY + layout.contentH));
    }


    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    for (auto& fnode : player->focusTreeEngine.nodes) {
        auto it1 = nodePositions.find(fnode.name);
        if (it1 == nodePositions.end()) continue;
        for (auto& prereq : fnode.prerequisites) {
            auto it2 = nodePositions.find(prereq);
            if (it2 == nodePositions.end()) continue;
            bool met = player->focusTreeEngine.completedFocuses.count(prereq) > 0;

            float depX = it1->second.first + layout.nodeW * 0.5f;
            float depTop = it1->second.second;
            float depBottom = it1->second.second + layout.nodeH;
            float preX = it2->second.first + layout.nodeW * 0.5f;
            float preTop = it2->second.second;
            float preBottom = it2->second.second + layout.nodeH;

            Color lineColor = met ? Theme::gold_dim : Color{150, 156, 166};
            uint8_t lineAlpha = met ? 235 : 200;
            SDL_SetRenderDrawColor(r, lineColor.r, lineColor.g, lineColor.b, lineAlpha);
            int lineW2 = std::max(2, (int)(u * 0.22f));

            auto drawSegment = [&](float ax, float ay, float bx, float by) {
                bool vertical = std::fabs(bx - ax) < std::fabs(by - ay);
                for (int t = -lineW2 / 2; t <= lineW2 / 2; t++) {
                    if (vertical) {
                        SDL_RenderDrawLine(r, (int)(ax + t), (int)ay, (int)(bx + t), (int)by);
                    } else {
                        SDL_RenderDrawLine(r, (int)ax, (int)(ay + t), (int)bx, (int)(by + t));
                    }
                }
            };

            if (preBottom <= depTop + 1.0f) {
                float midY = std::round((preBottom + depTop) * 0.5f);
                drawSegment(preX, preBottom, preX, midY);
                if (std::fabs(depX - preX) > 1.0f) drawSegment(preX, midY, depX, midY);
                drawSegment(depX, midY, depX, depTop);
            } else if (depBottom <= preTop + 1.0f) {
                float midY = std::round((depBottom + preTop) * 0.5f);
                drawSegment(preX, preTop, preX, midY);
                if (std::fabs(depX - preX) > 1.0f) drawSegment(preX, midY, depX, midY);
                drawSegment(depX, midY, depX, depBottom);
            } else {
                float preMidY = it2->second.second + layout.nodeH * 0.5f;
                float depMidY = it1->second.second + layout.nodeH * 0.5f;
                float preEdge = preX < depX ? it2->second.first + layout.nodeW : it2->second.first;
                float depEdge = preX < depX ? it1->second.first : it1->second.first + layout.nodeW;
                float midX = std::round((preEdge + depEdge) * 0.5f);
                drawSegment(preEdge, preMidY, midX, preMidY);
                if (std::fabs(depMidY - preMidY) > 1.0f) drawSegment(midX, preMidY, midX, depMidY);
                drawSegment(midX, depMidY, depEdge, depMidY);
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

        bool isActive = player->focus.has_value() &&
                        std::get<0>(*player->focus) == focusName;
        bool isHov = mx >= nodeX && mx <= nodeX + layout.nodeW && my >= nodeY && my <= nodeY + layout.nodeH;

        bool isCompleted = player->focusTreeEngine.completedFocuses.count(focusName) > 0;
        bool canStart = player->focusTreeEngine.canStartFocus(focusName);

        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(r, 0, 0, 0, 40);
        SDL_Rect nsh = {(int)nodeX + 2, (int)nodeY + 3, (int)layout.nodeW, (int)layout.nodeH};
        SDL_RenderFillRect(r, &nsh);

        Color nameC, dayC;
        HOI4::button(r, nodeX, nodeY, layout.nodeW, layout.nodeH, isHov && canStart, false,
                     canStart || isCompleted || isActive);
        if (isCompleted || isActive) {
            SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(r, Theme::gold.r, Theme::gold.g, Theme::gold.b, isActive ? 255 : 200);
            SDL_Rect goldRim = {(int)nodeX, (int)nodeY, (int)layout.nodeW, (int)layout.nodeH};
            SDL_RenderDrawRect(r, &goldRim);
        }
        if (isCompleted) {
            nameC = Theme::gold_bright;
            dayC = Theme::gold_dim;
        } else if (isActive) {
            nameC = Theme::gold_bright;
            dayC = Theme::gold;
        } else if (canStart) {
            nameC = isHov ? Theme::gold_bright : Theme::cream;
            dayC = Theme::grey;
        } else {
            nameC = Theme::grey;
            dayC = Theme::dark_grey;
        }

        if (isActive) {
            SDL_SetRenderDrawColor(r, Theme::gold.r, Theme::gold.g, Theme::gold.b, 220);
            SDL_Rect lbar = {(int)nodeX + 2, (int)nodeY + 4, 3, (int)layout.nodeH - 8};
            SDL_RenderFillRect(r, &lbar);
        }

        std::string daysStr;
        for (auto& val : focusData) {
            if (std::holds_alternative<int>(val)) {
                int days = std::get<int>(val);
                if (days > 0 && days < 9999) {
                    daysStr = std::to_string(days) + " days";
                }
                break;
            }
        }

        auto wrappedName = wrapNodeLabel(titleCase(focusName), layout.titleFs,
                                         static_cast<int>(layout.nodeW - u * 0.9f));
        float blockH = wrappedName.size() * layout.titleLineH +
                       (daysStr.empty() ? 0.0f : (u * 0.15f + layout.captionH));
        float ty = nodeY + std::max(layout.nodePad * 0.6f, (layout.nodeH - blockH) * 0.5f);
        for (const auto& line : wrappedName) {
            UIPrim::drawText(r, line, layout.titleFs,
                             nodeX + layout.nodeW * 0.5f, ty, "topcenter", nameC, isActive || isCompleted);
            ty += layout.titleLineH;
        }
        if (!daysStr.empty()) {
            ty += u * 0.15f;
            UIPrim::drawText(r, daysStr, layout.captionFs,
                             nodeX + layout.nodeW * 0.5f, ty, "topcenter", dayC);
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

                std::string statusStr = fc ? "COMPLETED" : (fa ? "AVAILABLE" : "LOCKED");
                Color statusColor = fc ? Theme::gold_bright : (fa ? Theme::green_light : Theme::grey);

                int tipFs = Theme::fsCaption(u) + 1;
                int descFs = Theme::fsCaption(u);
                float lineH = (float)UIPrim::textHeight(tipFs);
                float descLineH = (float)UIPrim::textHeight(descFs);
                float tipPad = std::round(u * 0.8f);
                float tipW = std::round(u * 22.0f);
                float innerW = tipW - tipPad * 2.0f;
                float lineStep = std::round(u * 0.35f);

                auto countWrappedLines = [&](const std::string& text, int fs, float maxW) {
                    if (text.empty()) return 0;
                    std::istringstream words(text);
                    std::string word, line;
                    int count = 0;
                    while (words >> word) {
                        std::string candidate = line.empty() ? word : line + " " + word;
                        if (!line.empty() && UIPrim::textWidth(candidate, fs) > maxW) {
                            count++;
                            line = word;
                        } else {
                            line = candidate;
                        }
                    }
                    if (!line.empty()) count++;
                    return count;
                };

                std::string titleStr2 = titleCase(fn);
                std::string costStr;
                std::string descStr;
                std::string prereqStr;
                bool prereqMet = true;
                if (fnode) {
                    char costBuf[64];
                    snprintf(costBuf, sizeof(costBuf), "%d PP  |  %d days", fnode->cost, fnode->days);
                    costStr = costBuf;
                    descStr = fnode->description;
                    if (!fnode->prerequisites.empty()) {
                        prereqStr = "Requires: ";
                        for (size_t pi = 0; pi < fnode->prerequisites.size(); pi++) {
                            if (pi > 0) prereqStr += ", ";
                            prereqStr += titleCase(fnode->prerequisites[pi]);
                        }
                        for (auto& p : fnode->prerequisites) {
                            if (player->focusTreeEngine.completedFocuses.count(p) == 0) prereqMet = false;
                        }
                    }
                }

                int descLines = countWrappedLines(descStr, descFs, innerW);
                int prereqLines = countWrappedLines(prereqStr, descFs, innerW);
                float tipH = tipPad + lineH + lineStep + lineH;
                if (!costStr.empty()) tipH += lineStep + lineH;
                if (descLines > 0) tipH += lineStep + descLines * descLineH;
                if (prereqLines > 0) tipH += lineStep + prereqLines * descLineH;
                tipH += tipPad;

                float tipX = nx + layout.nodeW + std::round(u * 0.8f);
                float tipY = ny;
                if (tipX + tipW > W - 4) tipX = nx - tipW - std::round(u * 0.8f);
                tipX = std::clamp(tipX, 4.0f, std::max(4.0f, W - tipW - 4.0f));
                tipY = std::clamp(tipY, 4.0f, std::max(4.0f, H - tipH - 4.0f));

                UIPrim::drawShadow(r, tipX, tipY, tipW, tipH, 70, 6);
                HOI4::plate(r, tipX, tipY, tipW, tipH);

                float ty = tipY + tipPad;
                UIPrim::drawText(r, UIPrim::fitText(titleStr2, tipFs, innerW), tipFs,
                                 tipX + tipPad, ty, "topleft", Theme::cream, true);
                ty += lineH + lineStep;
                UIPrim::drawText(r, statusStr, tipFs, tipX + tipPad, ty, "topleft", statusColor);
                ty += lineH;
                if (!costStr.empty()) {
                    ty += lineStep;
                    UIPrim::drawText(r, costStr, tipFs, tipX + tipPad, ty, "topleft", Theme::gold);
                    ty += lineH;
                }
                if (descLines > 0) {
                    ty += lineStep;
                    ty = UIPrim::drawTextWrapped(r, descStr, descFs, tipX + tipPad, ty, innerW, Theme::grey);
                }
                if (prereqLines > 0) {
                    ty += lineStep;
                    UIPrim::drawTextWrapped(r, prereqStr, descFs, tipX + tipPad, ty, innerW,
                                            prereqMet ? Theme::grey : Theme::orange);
                }
                break;
            }
        }
    }


    float footH = layout.footH;
    UIPrim::drawRectFilled(r, {15, 17, 16, 220}, 0, H - footH, (float)W, footH);
    HOI4::goldDivider(r, 0, (float)W, H - footH);
    int instFs = std::max(12, (int)(u * 0.9f));
    UIPrim::drawText(r, "WASD / Arrow Keys or middle mouse to pan  |  Scroll to scroll  |  ESC to return",
                     instFs, W * 0.5f, H - footH * 0.5f, "center", Theme::dark_grey);
    UIAnim::popMenuTransition(r);
    UIAnim::drawTransitionFade(r, "screen:dtree", W, H);
}
