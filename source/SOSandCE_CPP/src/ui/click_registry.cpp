#include "ui/click_registry.h"

void ClickRegistry::clear() {
    zones_.clear();
    hoveredZone_.clear();
}

void ClickRegistry::registerZone(const std::string& name, SDL_Rect rect,
                                  std::function<void()> callback, int zOrder,
                                  std::function<void()> hoverCallback) {
    zones_.push_back({name, rect, std::move(callback), std::move(hoverCallback), zOrder});
}

bool ClickRegistry::dispatchClick(int mx, int my) {

    std::sort(zones_.begin(), zones_.end(),
              [](const ClickZone& a, const ClickZone& b) { return a.zOrder > b.zOrder; });

    SDL_Point pt = {mx, my};
    for (auto& zone : zones_) {
        if (SDL_PointInRect(&pt, &zone.rect)) {
            if (zone.callback) zone.callback();
            return true;
        }
    }
    return false;
}

std::string ClickRegistry::updateHover(int mx, int my) {
    hoveredZone_.clear();
    std::sort(zones_.begin(), zones_.end(),
              [](const ClickZone& a, const ClickZone& b) { return a.zOrder > b.zOrder; });

    SDL_Point pt = {mx, my};
    for (auto& zone : zones_) {
        if (SDL_PointInRect(&pt, &zone.rect)) {
            hoveredZone_ = zone.name;
            if (zone.hoverCallback) zone.hoverCallback();
            return hoveredZone_;
        }
    }
    return "";
}

bool ClickRegistry::isHovered(const std::string& name) const {
    return hoveredZone_ == name;
}

bool ClickRegistry::hitAny(int mx, int my) const {
    SDL_Point pt = {mx, my};
    for (auto& zone : zones_) {
        if (SDL_PointInRect(&pt, &zone.rect)) return true;
    }
    return false;
}
