#pragma once

#include "core/common.h"
#include <functional>


struct ClickZone {
    std::string             name;
    SDL_Rect                rect;
    std::function<void()>   callback;
    std::function<void()>   hoverCallback;
    int                     zOrder = 0;
};

class ClickRegistry {
public:


    void clear();


    void registerZone(const std::string& name,
                      SDL_Rect rect,
                      std::function<void()> callback,
                      int zOrder = 0,
                      std::function<void()> hoverCallback = nullptr);


    bool dispatchClick(int mx, int my);


    std::string updateHover(int mx, int my);


    bool isHovered(const std::string& name) const;


    bool hitAny(int mx, int my) const;

private:
    std::vector<ClickZone> zones_;
    std::string            hoveredZone_;
};
