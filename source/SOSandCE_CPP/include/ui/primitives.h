#pragma once

#include "core/common.h"


namespace UIPrim {


SDL_Texture* renderText(const std::string& txt, int size,
                        Color color = {210, 200, 172});


SDL_Rect drawText(SDL_Renderer* r, const std::string& txt, int size,
                  float x, float y,
                  const std::string& anchor = "center",
                  Color color = {210, 200, 172},
                  bool shadow = false);


int textWidth(const std::string& txt, int size);
int textHeight(int size);


SDL_Rect drawRect(SDL_Renderer* r, Color color,
                  float x, float y, float w, float h,
                  Color borderColor = {0, 0, 0, 0},
                  int radius = 0, int borderWidth = 0);


void drawRectFilled(SDL_Renderer* r, Color color,
                    float x, float y, float w, float h);


void drawRoundedRect(SDL_Renderer* r, Color color,
                     float x, float y, float w, float h,
                     int radius, Color borderColor = {0, 0, 0, 0});


void drawHLine(SDL_Renderer* r, Color color,
               float x1, float x2, float y, int width = 1);


void drawVLine(SDL_Renderer* r, Color color,
               float x, float y1, float y2, int width = 1);


void drawGradientV(SDL_Renderer* r, float x, float y, float w, float h,
                    Color top, Color bottom);


void drawShadow(SDL_Renderer* r,
                float x, float y, float w, float h,
                int alpha = 50, int offset = 3);


void drawVignette(SDL_Renderer* r, int screenW, int screenH, int strength = 120);


void drawBeveledRect(SDL_Renderer* r, Color color,
                     float x, float y, float w, float h);


void drawOrnamentalFrame(SDL_Renderer* r,
                         float x, float y, float w, float h,
                         int thick = 2);


void drawMapDim(SDL_Renderer* r, int screenW, int screenH, int alpha = 30);


void drawMenuWindow(SDL_Renderer* r,
                    float x, float y, float w, float h,
                    const std::string& title = "");


bool drawMenuButton(SDL_Renderer* r,
                    float x, float y, float w, float h,
                    const std::string& label,
                    int mouseX, int mouseY,
                    bool mouseDown = false,
                    const std::string& value = "",
                    int fontSize = 0);


void drawActionRow(SDL_Renderer* r,
                   float x, float y, float w, float h,
                   const std::string& label,
                   int mouseX, int mouseY,
                   bool mouseDown = false,
                   const std::string& costText = "",
                   const std::string& sublabel = "",
                   bool enabled = true);


float drawSectionHeader(SDL_Renderer* r,
                        float x, float y, float w, float h,
                        const std::string& label);


float drawInfoRow(SDL_Renderer* r,
                  float x, float y, float w, float h,
                  const std::string& label,
                  const std::string& value = "",
                  Color lc = {0, 0, 0, 0},
                  Color vc = {0, 0, 0, 0});


void drawIcon(SDL_Renderer* r, const std::string& iconName,
              float x, float y, float size);


bool drawIconButton(SDL_Renderer* r, const std::string& iconName,
                    float x, float y, float size,
                    int mouseX, int mouseY, bool mouseDown = false);

}
