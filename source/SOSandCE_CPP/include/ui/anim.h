#pragma once
#include "core/common.h"

namespace UIAnim {

constexpr float MENU_OPEN_SECONDS = 0.15f;
constexpr float HOVER_LIFT_PX = -2.0f;

void beginFrame(float dt);
float frameDt();

void trigger(const std::string& id);
void forget(const std::string& id);
float progress(const std::string& id);

void pushMenuTransition(SDL_Renderer* r, const std::string& id, float centerX, float centerY);
void popMenuTransition(SDL_Renderer* r);
void drawTransitionFade(SDL_Renderer* r, const std::string& id, int screenW, int screenH);

float springOffset(const std::string& id, bool active, float target = HOVER_LIFT_PX);

float countTo(const std::string& id, float target);

Color brighten(Color c, float factor);

void clearAll();

}
