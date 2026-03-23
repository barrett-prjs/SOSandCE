#pragma once
#include "core/common.h"

class ToastSystem {
public:
    void show(const std::string& text, int durationMs = 1500);
    void render(SDL_Renderer* r, int screenW, int screenH, float uiSize);
    void clear();

private:
    struct Toast {
        std::string text;
        uint32_t expireTime;
    };
    std::vector<Toast> toasts_;
};

ToastSystem& toasts();
