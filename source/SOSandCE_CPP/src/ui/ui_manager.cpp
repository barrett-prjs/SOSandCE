#include "ui/ui_manager.h"
#include "ui/panel.h"
#include "core/input.h"
#include "ui/theme.h"


UIManager::UIManager() = default;
UIManager::~UIManager() = default;


void UIManager::addPanel(Panel* p) {
    if (!p) return;


    removePanel(p);

    p->zOrder = nextZ_++;
    panels.push_back(p);
}

void UIManager::removePanel(Panel* p) {
    panels.erase(
        std::remove(panels.begin(), panels.end(), p),
        panels.end());
}


void UIManager::handleInput(const InputState& input) {


    for (int i = static_cast<int>(popupList.size()) - 1; i >= 0; i--) {
        if (popupList[i]) {
            float uiSize = Theme::uiScale;
            popupList[i]->update(input, uiSize, popupList);

            if (i >= static_cast<int>(popupList.size())) continue;

            if (!popupList.empty()) return;
        }
    }


    std::vector<Panel*> sorted = panels;
    std::sort(sorted.begin(), sorted.end(),
              [](const Panel* a, const Panel* b) {
                  return a->zOrder > b->zOrder;
              });

    for (auto* panel : sorted) {
        if (!panel || !panel->visible) continue;
        panel->handleInput(input);


        if (panel->hovered && input.mouseLeftDown) {
            panel->zOrder = nextZ_++;
        }
    }


    topBar.updateHover(input.mouseX, input.mouseY);
}


void UIManager::update(float dt) {
    for (auto* panel : panels) {
        if (panel && panel->visible) {
            panel->update(dt);
        }
    }
}


void UIManager::render(SDL_Renderer* r, int screenW, int screenH, float uiSize) {

    std::vector<Panel*> sorted = panels;
    std::sort(sorted.begin(), sorted.end(),
              [](const Panel* a, const Panel* b) {
                  return a->zOrder < b->zOrder;
              });

    for (auto* panel : sorted) {
        if (panel && panel->visible) {
            panel->render(r);
        }
    }


    for (auto& popup : popupList) {
        if (popup) {
            popup->draw(r, uiSize);
        }
    }


    topBar.render(r);


    tooltip.render(r, screenW, screenH);
}


bool UIManager::anyPanelOpen() const {
    for (auto* panel : panels) {
        if (panel && panel->visible) return true;
    }
    return false;
}


bool UIManager::hitUI(int mx, int my) const {

    if (topBar.hit(mx, my)) return true;


    for (auto& popup : popupList) {
        if (popup) {
            float halfW = popup->xSize * Theme::uiScale * 0.5f;
            float halfH = popup->ySize * Theme::uiScale * 0.5f;
            if (mx >= popup->x - halfW && mx <= popup->x + halfW &&
                my >= popup->y - halfH && my <= popup->y + halfH) {
                return true;
            }
        }
    }


    for (auto* panel : panels) {
        if (panel && panel->visible && panel->containsPoint(
                static_cast<float>(mx), static_cast<float>(my))) {
            return true;
        }
    }

    return false;
}
