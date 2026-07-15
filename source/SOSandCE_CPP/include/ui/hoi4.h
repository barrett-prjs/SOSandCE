#pragma once

#include "core/common.h"

namespace HOI4 {

void panel(SDL_Renderer* r, float x, float y, float w, float h, int bodyAlpha = 255);
void plate(SDL_Renderer* r, float x, float y, float w, float h);
void inset(SDL_Renderer* r, float x, float y, float w, float h);
void frame(SDL_Renderer* r, float x, float y, float w, float h, bool gold = false);
void button(SDL_Renderer* r, float x, float y, float w, float h,
            bool hovered, bool pressed, bool enabled);
void tab(SDL_Renderer* r, float x, float y, float w, float h, bool active, bool hovered);
void diagPlate(SDL_Renderer* r, float x, float y, float w, float h, int cut, bool cutLeft);
void topBar(SDL_Renderer* r, float w, float h);
void vDivider(SDL_Renderer* r, float x, float y1, float y2);
void cell(SDL_Renderer* r, float x, float y, float w, float h);
void goldDivider(SDL_Renderer* r, float x1, float x2, float y);
float sectionHeader(SDL_Renderer* r, float x, float y, float w, float h,
                    const std::string& title);
void sectionLabel(SDL_Renderer* r, float x, float y, const std::string& title, int fontSize);
std::string caps(const std::string& s);
std::string tracked(const std::string& s);

}
