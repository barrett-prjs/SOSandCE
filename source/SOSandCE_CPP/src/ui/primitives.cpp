#include "ui/primitives.h"
#include "ui/theme.h"
#include "ui/text_cache.h"
#include "ui/ui_assets.h"
#include "core/engine.h"

namespace UIPrim {

namespace {

void drawHeadlessButtonBase(SDL_Renderer* r, float x, float y, float w, float h,
                            bool hovered, bool pressed, bool enabled) {
    SDL_Texture* panelTex = UIAssets::instance().panelBodyHeadless();
    float drawY = (pressed && hovered && enabled) ? y + 1.0f : y;

    if (panelTex) {
        uint8_t mod = !enabled ? 148 : (pressed && hovered ? 214 : (hovered ? 255 : 238));
        uint8_t alpha = !enabled ? 190 : (hovered ? 255 : 246);
        SDL_SetTextureColorMod(panelTex, mod, mod, mod);
        SDL_SetTextureAlphaMod(panelTex, alpha);
        UIAssets::draw9Slice(r, panelTex, x, drawY, w, h, 18);
        SDL_SetTextureColorMod(panelTex, 255, 255, 255);
        SDL_SetTextureAlphaMod(panelTex, 255);
    } else {
        Color bg = !enabled ? Theme::btn_disabled : (hovered ? Theme::btn_hover : Theme::btn);
        if (pressed && hovered && enabled) bg = Theme::btn_press;
        Color brd = hovered && enabled ? Theme::gold_dim : Theme::border;
        drawRoundedRect(r, bg, x, drawY, w, h, 7, brd);
    }

    drawHLine(r, hovered && enabled ? Theme::gold_dim : Color{Theme::border.r, Theme::border.g, Theme::border.b, 90},
              x + 6, x + w - 6, drawY + h - 2, 1);
}

}

SDL_Texture* renderText(const std::string& txt, int size, Color color) {
    return textCache().get(txt, size, color);
}

SDL_Rect drawText(SDL_Renderer* r, const std::string& txt, int size, float x, float y,
                  const std::string& anchor, Color color, bool shadow) {
    if (txt.empty()) return {0, 0, 0, 0};

    auto entry = textCache().getEntry(txt, size, color);
    if (!entry.texture) return {0, 0, 0, 0};

    SDL_Rect dst;
    dst.w = entry.w;
    dst.h = entry.h;

    if (anchor == "center") {
        dst.x = static_cast<int>(x) - dst.w / 2;
        dst.y = static_cast<int>(y) - dst.h / 2;
    } else if (anchor == "topleft") {
        dst.x = static_cast<int>(x);
        dst.y = static_cast<int>(y);
    } else if (anchor == "topright") {
        dst.x = static_cast<int>(x) - dst.w;
        dst.y = static_cast<int>(y);
    } else if (anchor == "topcenter") {
        dst.x = static_cast<int>(x) - dst.w / 2;
        dst.y = static_cast<int>(y);
    } else if (anchor == "midleft") {
        dst.x = static_cast<int>(x);
        dst.y = static_cast<int>(y) - dst.h / 2;
    } else if (anchor == "midright") {
        dst.x = static_cast<int>(x) - dst.w;
        dst.y = static_cast<int>(y) - dst.h / 2;
    } else if (anchor == "bottomleft") {
        dst.x = static_cast<int>(x);
        dst.y = static_cast<int>(y) - dst.h;
    } else if (anchor == "bottomright") {
        dst.x = static_cast<int>(x) - dst.w;
        dst.y = static_cast<int>(y) - dst.h;
    } else if (anchor == "bottomcenter") {
        dst.x = static_cast<int>(x) - dst.w / 2;
        dst.y = static_cast<int>(y) - dst.h;
    } else {
        dst.x = static_cast<int>(x) - dst.w / 2;
        dst.y = static_cast<int>(y) - dst.h / 2;
    }

    if (shadow) {
        auto shadowEntry = textCache().getEntry(txt, size, Theme::text_shadow);
        if (shadowEntry.texture) {
            SDL_Rect sdst = {dst.x + 1, dst.y + 1, dst.w, dst.h};
            SDL_RenderCopy(r, shadowEntry.texture, nullptr, &sdst);
        }
    }

    SDL_RenderCopy(r, entry.texture, nullptr, &dst);
    return dst;
}

int textWidth(const std::string& txt, int size) {
    auto& engine = Engine::instance();
    TTF_Font* f = engine.getFont(std::max(1, size));
    if (!f || txt.empty()) return 0;
    int w = 0, h = 0;
    TTF_SizeUTF8(f, txt.c_str(), &w, &h);
    return w;
}

int textHeight(int size) {
    auto& engine = Engine::instance();
    TTF_Font* f = engine.getFont(std::max(1, size));
    return f ? TTF_FontLineSkip(f) : size;
}

SDL_Rect drawRect(SDL_Renderer* r, Color color, float x, float y, float w, float h,
                  Color borderColor, int radius, int borderWidth) {
    SDL_Rect rc = {static_cast<int>(x), static_cast<int>(y),
                   static_cast<int>(std::max(0.0f, w)), static_cast<int>(std::max(0.0f, h))};
    if (rc.w <= 0 || rc.h <= 0) return rc;

    if (borderWidth > 0) {
        SDL_SetRenderDrawColor(r, color.r, color.g, color.b, color.a);
        for (int i = 0; i < borderWidth; i++) {
            SDL_Rect outline = {rc.x + i, rc.y + i, rc.w - i * 2, rc.h - i * 2};
            SDL_RenderDrawRect(r, &outline);
        }
    } else {
        SDL_SetRenderDrawColor(r, color.r, color.g, color.b, color.a);
        SDL_RenderFillRect(r, &rc);
        if (borderColor.a > 0) {
            SDL_SetRenderDrawColor(r, borderColor.r, borderColor.g, borderColor.b, borderColor.a);
            SDL_RenderDrawRect(r, &rc);
        }
    }
    return rc;
}

void drawRectFilled(SDL_Renderer* r, Color color, float x, float y, float w, float h) {
    SDL_Rect rc = {static_cast<int>(x), static_cast<int>(y),
                   static_cast<int>(w), static_cast<int>(h)};
    SDL_SetRenderDrawColor(r, color.r, color.g, color.b, color.a);
    SDL_RenderFillRect(r, &rc);
}

void drawRoundedRect(SDL_Renderer* r, Color color, float x, float y, float w, float h,
                     int radius, Color borderColor) {
    if (radius <= 0 || w < radius * 2 || h < radius * 2) {
        drawRectFilled(r, color, x, y, w, h);
        if (borderColor.a > 0) {
            SDL_SetRenderDrawColor(r, borderColor.r, borderColor.g, borderColor.b, borderColor.a);
            SDL_Rect rc = {static_cast<int>(x), static_cast<int>(y),
                           static_cast<int>(w), static_cast<int>(h)};
            SDL_RenderDrawRect(r, &rc);
        }
        return;
    }

    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, color.r, color.g, color.b, color.a);

    int ix = static_cast<int>(x), iy = static_cast<int>(y);
    int iw = static_cast<int>(w), ih = static_cast<int>(h);
    int rad = radius;


    SDL_Rect top = {ix + rad, iy, iw - rad * 2, rad};
    SDL_RenderFillRect(r, &top);

    SDL_Rect mid = {ix, iy + rad, iw, ih - rad * 2};
    SDL_RenderFillRect(r, &mid);

    SDL_Rect bot = {ix + rad, iy + ih - rad, iw - rad * 2, rad};
    SDL_RenderFillRect(r, &bot);


    auto fillCircle = [&](int cx, int cy, int cr) {
        for (int dy = -cr; dy <= cr; dy++) {
            int dx = static_cast<int>(std::sqrt(static_cast<float>(cr * cr - dy * dy)));
            SDL_RenderDrawLine(r, cx - dx, cy + dy, cx + dx, cy + dy);
        }
    };
    fillCircle(ix + rad, iy + rad, rad);
    fillCircle(ix + iw - rad - 1, iy + rad, rad);
    fillCircle(ix + rad, iy + ih - rad - 1, rad);
    fillCircle(ix + iw - rad - 1, iy + ih - rad - 1, rad);


    if (borderColor.a > 0) {
        SDL_SetRenderDrawColor(r, borderColor.r, borderColor.g, borderColor.b, borderColor.a);

        SDL_RenderDrawLine(r, ix + rad, iy, ix + iw - rad, iy);

        SDL_RenderDrawLine(r, ix + rad, iy + ih - 1, ix + iw - rad, iy + ih - 1);

        SDL_RenderDrawLine(r, ix, iy + rad, ix, iy + ih - rad);

        SDL_RenderDrawLine(r, ix + iw - 1, iy + rad, ix + iw - 1, iy + ih - rad);

        auto drawArc = [&](int cx, int cy, int cr, int quadrant) {
            for (int i = 0; i <= cr; i++) {
                int j = static_cast<int>(std::sqrt(static_cast<float>(cr * cr - i * i)));
                switch (quadrant) {
                    case 0: SDL_RenderDrawPoint(r, cx - i, cy - j); SDL_RenderDrawPoint(r, cx - j, cy - i); break;
                    case 1: SDL_RenderDrawPoint(r, cx + i, cy - j); SDL_RenderDrawPoint(r, cx + j, cy - i); break;
                    case 2: SDL_RenderDrawPoint(r, cx - i, cy + j); SDL_RenderDrawPoint(r, cx - j, cy + i); break;
                    case 3: SDL_RenderDrawPoint(r, cx + i, cy + j); SDL_RenderDrawPoint(r, cx + j, cy + i); break;
                }
            }
        };
        drawArc(ix + rad, iy + rad, rad, 0);
        drawArc(ix + iw - rad - 1, iy + rad, rad, 1);
        drawArc(ix + rad, iy + ih - rad - 1, rad, 2);
        drawArc(ix + iw - rad - 1, iy + ih - rad - 1, rad, 3);
    }
}

void drawHLine(SDL_Renderer* r, Color color, float x1, float x2, float y, int width) {
    SDL_SetRenderDrawColor(r, color.r, color.g, color.b, color.a);
    for (int i = 0; i < width; i++) {
        SDL_RenderDrawLine(r, static_cast<int>(x1), static_cast<int>(y) + i,
                          static_cast<int>(x2), static_cast<int>(y) + i);
    }
}

void drawVLine(SDL_Renderer* r, Color color, float x, float y1, float y2, int width) {
    SDL_SetRenderDrawColor(r, color.r, color.g, color.b, color.a);
    for (int i = 0; i < width; i++) {
        SDL_RenderDrawLine(r, static_cast<int>(x) + i, static_cast<int>(y1),
                          static_cast<int>(x) + i, static_cast<int>(y2));
    }
}

void drawGradientV(SDL_Renderer* r, float x, float y, float w, float h,
                    Color top, Color bottom) {
    if (h <= 0 || w <= 0) return;
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);

    int ih = static_cast<int>(h);
    int bandSize = 4;
    for (int i = 0; i < ih; i += bandSize) {
        float t = static_cast<float>(i) / std::max(1, ih - 1);
        uint8_t cr = static_cast<uint8_t>(top.r + (bottom.r - top.r) * t);
        uint8_t cg = static_cast<uint8_t>(top.g + (bottom.g - top.g) * t);
        uint8_t cb = static_cast<uint8_t>(top.b + (bottom.b - top.b) * t);
        uint8_t ca = static_cast<uint8_t>(top.a + (bottom.a - top.a) * t);
        SDL_SetRenderDrawColor(r, cr, cg, cb, ca);
        int bandH = std::min(bandSize, ih - i);
        SDL_Rect band = {static_cast<int>(x), static_cast<int>(y) + i,
                         static_cast<int>(w), bandH};
        SDL_RenderFillRect(r, &band);
    }
}

void drawShadow(SDL_Renderer* r, float x, float y, float w, float h, int alpha, int offset) {
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);

    for (int layer = offset; layer >= 1; layer--) {
        float falloff = static_cast<float>(layer) / offset;
        int layerAlpha = static_cast<int>(alpha * falloff * 0.5f);
        SDL_SetRenderDrawColor(r, 0, 0, 0, static_cast<uint8_t>(std::min(255, layerAlpha)));
        SDL_Rect rc = {static_cast<int>(x) + 1, static_cast<int>(y) + layer,
                       static_cast<int>(w), static_cast<int>(h)};
        SDL_RenderFillRect(r, &rc);
    }
}

void drawVignette(SDL_Renderer* r, int screenW, int screenH, int strength) {
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);

    int bandH = screenH / 4;
    int step = 4;
    for (int i = 0; i < bandH; i += step) {
        float t = 1.0f - static_cast<float>(i) / bandH;
        int a = static_cast<int>(strength * t * t);
        SDL_SetRenderDrawColor(r, 0, 0, 0, static_cast<uint8_t>(std::min(255, a)));

        SDL_Rect topBand = {0, i, screenW, std::min(step, bandH - i)};
        SDL_RenderFillRect(r, &topBand);

        SDL_Rect botBand = {0, screenH - i - step, screenW, std::min(step, bandH - i)};
        SDL_RenderFillRect(r, &botBand);
    }

    int bandW = screenW / 5;
    for (int i = 0; i < bandW; i += step) {
        float t = 1.0f - static_cast<float>(i) / bandW;
        int a = static_cast<int>(strength * 0.5f * t * t);
        SDL_SetRenderDrawColor(r, 0, 0, 0, static_cast<uint8_t>(std::min(255, a)));

        SDL_Rect leftBand = {i, 0, std::min(step, bandW - i), screenH};
        SDL_RenderFillRect(r, &leftBand);

        SDL_Rect rightBand = {screenW - i - step, 0, std::min(step, bandW - i), screenH};
        SDL_RenderFillRect(r, &rightBand);
    }
}

void drawBeveledRect(SDL_Renderer* r, Color color, float x, float y, float w, float h) {
    drawRoundedRect(r, color, x, y, w, h, 4);


    Color hl = {static_cast<uint8_t>(std::min(255, color.r + 25)),
                static_cast<uint8_t>(std::min(255, color.g + 25)),
                static_cast<uint8_t>(std::min(255, color.b + 25))};
    drawHLine(r, hl, x + 3, x + w - 3, y + 1);


    Color sh = {static_cast<uint8_t>(std::max(0, color.r - 25)),
                static_cast<uint8_t>(std::max(0, color.g - 25)),
                static_cast<uint8_t>(std::max(0, color.b - 25))};
    drawHLine(r, sh, x + 3, x + w - 3, y + h - 2);


    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, 255, 255, 255, 6);
    SDL_Rect topG = {(int)x + 2, (int)y + 2, (int)w - 4, (int)(h * 0.35f)};
    SDL_RenderFillRect(r, &topG);
}

void drawOrnamentalFrame(SDL_Renderer* r, float x, float y, float w, float h, int thick) {
    int ix = static_cast<int>(x), iy = static_cast<int>(y);
    int iw = static_cast<int>(w), ih = static_cast<int>(h);

    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);


    SDL_SetRenderDrawColor(r, Theme::gold_dim.r, Theme::gold_dim.g, Theme::gold_dim.b, 20);
    SDL_Rect glow1 = {ix - 2, iy - 2, iw + 4, ih + 4};
    SDL_RenderDrawRect(r, &glow1);
    SDL_Rect glow2 = {ix - 1, iy - 1, iw + 2, ih + 2};
    SDL_RenderDrawRect(r, &glow2);


    SDL_SetRenderDrawColor(r, Theme::gold.r, Theme::gold.g, Theme::gold.b, 180);
    for (int i = 0; i < thick; i++) {
        SDL_Rect outline = {ix + i, iy + i, iw - i * 2, ih - i * 2};
        SDL_RenderDrawRect(r, &outline);
    }


    int inset = thick + 2;
    SDL_SetRenderDrawColor(r, Theme::gold_dim.r, Theme::gold_dim.g, Theme::gold_dim.b, 60);
    SDL_Rect inner = {ix + inset, iy + inset, iw - inset * 2, ih - inset * 2};
    SDL_RenderDrawRect(r, &inner);


    int cl = std::max(20, Theme::si(14));
    int ct = 3;
    struct Corner { int cx, cy, dx, dy; };
    Corner corners[] = {
        {ix, iy, 1, 1},
        {ix + iw - 1, iy, -1, 1},
        {ix, iy + ih - 1, 1, -1},
        {ix + iw - 1, iy + ih - 1, -1, -1}
    };
    for (auto& c : corners) {

        SDL_SetRenderDrawColor(r, Theme::gold_bright.r, Theme::gold_bright.g, Theme::gold_bright.b, 255);
        for (int t = 0; t < ct; t++) {

            SDL_RenderDrawLine(r, c.cx, c.cy + t * c.dy, c.cx + c.dx * cl, c.cy + t * c.dy);

            SDL_RenderDrawLine(r, c.cx + t * c.dx, c.cy, c.cx + t * c.dx, c.cy + c.dy * cl);
        }

        int dotSz = 4;
        SDL_Rect dot = {
            c.dx > 0 ? c.cx : c.cx - dotSz + 1,
            c.dy > 0 ? c.cy : c.cy - dotSz + 1,
            dotSz, dotSz
        };
        SDL_RenderFillRect(r, &dot);


        SDL_SetRenderDrawColor(r, Theme::gold.r, Theme::gold.g, Theme::gold.b, 40);
        SDL_Rect cornerGlow = {dot.x - 2, dot.y - 2, dot.w + 4, dot.h + 4};
        SDL_RenderFillRect(r, &cornerGlow);
    }
}

void drawMapDim(SDL_Renderer* r, int screenW, int screenH, int alpha) {
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, 0, 0, 0, static_cast<uint8_t>(alpha));
    SDL_Rect rc = {0, 0, screenW, screenH};
    SDL_RenderFillRect(r, &rc);
}

void drawMenuWindow(SDL_Renderer* r, float x, float y, float w, float h, const std::string& title) {
    auto& assets = UIAssets::instance();


    drawShadow(r, x, y, w, h, 50, 16);
    drawShadow(r, x, y, w, h, 70, 8);
    drawShadow(r, x, y, w, h, 100, 3);


    SDL_Texture* panelTex = title.empty() ? assets.panelBodyHeadless() : assets.panelBodyHeaded();
    if (panelTex) {
        UIAssets::draw9Slice(r, panelTex, x, y, w, h, 24);
    } else {
        drawRoundedRect(r, Theme::panel, x, y, w, h, 10);
    }


    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, 255, 255, 255, 5);
    SDL_Rect topGrad = {(int)(x + 6), (int)(y + 6), (int)(w - 12), (int)(h * 0.08f)};
    SDL_RenderFillRect(r, &topGrad);

    if (!title.empty()) {

        int hh = Theme::si(32);
        SDL_Texture* headerTex = assets.panelHeader();
        if (headerTex) {
            UIAssets::draw9Slice(r, headerTex, x + 4, y + 4, w - 8, static_cast<float>(hh + 6), 16);
        }


        drawHLine(r, Theme::gold_dim, x + 8, x + w - 8, y + hh + 6, 1);
        drawHLine(r, {Theme::gold_dim.r, Theme::gold_dim.g, Theme::gold_dim.b, 80},
                  x + 12, x + w - 12, y + hh + 8, 1);


        drawText(r, title, Theme::si(14), x + w / 2, y + (hh + 6) / 2.0f, "center", Theme::gold, true);
    }


    drawOrnamentalFrame(r, x, y, w, h);
}

bool drawMenuButton(SDL_Renderer* r, float x, float y, float w, float h, const std::string& label,
                    int mouseX, int mouseY, bool mouseDown, const std::string& value, int fontSize) {
    bool hovered = mouseX >= x && mouseX <= x + w && mouseY >= y && mouseY <= y + h;

    Color tc;
    if (mouseDown && hovered) {
        tc = Theme::gold_bright;
    } else if (hovered) {
        tc = Theme::gold;
    } else {
        tc = Theme::cream;
    }

    float drawY = (mouseDown && hovered) ? y + 1.0f : y;
    drawHeadlessButtonBase(r, x, y, w, h, hovered, mouseDown, true);

    int fs = fontSize > 0 ? fontSize : static_cast<int>(h * 0.45f);
    if (!value.empty()) {
        drawText(r, label, fs, x + Theme::s(8), drawY + h / 2, "midleft", tc);
        drawText(r, value, fs, x + w - Theme::s(8), drawY + h / 2, "midright",
                 hovered ? Theme::gold : Theme::grey);
    } else {
        drawText(r, label, fs, x + w / 2, drawY + h / 2, "center", tc, true);
    }
    return hovered && mouseDown;
}

void drawActionRow(SDL_Renderer* r, float x, float y, float w, float h, const std::string& label,
                   int mouseX, int mouseY, bool mouseDown, const std::string& costText,
                   const std::string& sublabel, bool enabled) {
    bool hovered = enabled && mouseX >= x && mouseX <= x + w && mouseY >= y && mouseY <= y + h;
    Color tc = !enabled ? Theme::dark_grey
             : (mouseDown && hovered ? Theme::gold_bright
             : (hovered ? Theme::gold : Theme::cream));

    float drawY = (mouseDown && hovered) ? y + 1.0f : y;
    drawHeadlessButtonBase(r, x, y, w, h, hovered, mouseDown, enabled);

    int fs = std::max(14, static_cast<int>(h * 0.42f));
    float pad = std::max(14.0f, w * 0.04f);
    if (!sublabel.empty()) {
        drawText(r, label, fs, x + pad, drawY + h * 0.3f, "midleft", tc);
        drawText(r, sublabel, std::max(fs - 2, 6), x + pad, drawY + h * 0.7f, "midleft", Theme::grey);
    } else {
        drawText(r, label, fs, x + pad, drawY + h / 2, "midleft", tc);
    }
    if (!costText.empty()) {
        drawText(r, costText, fs, x + w - pad, drawY + h / 2, "midright",
                 enabled ? Theme::gold : Theme::dark_grey);
    }
}

float drawSectionHeader(SDL_Renderer* r, float x, float y, float w, float h, const std::string& label) {

    drawRectFilled(r, Theme::header, x, y, w, h);


    drawHLine(r, Theme::gold_dim, x, x + w, y, 1);

    drawHLine(r, {Theme::border.r, Theme::border.g, Theme::border.b, 60}, x, x + w, y + h - 1, 1);


    SDL_SetRenderDrawColor(r, Theme::gold.r, Theme::gold.g, Theme::gold.b, 180);
    SDL_Rect leftBar = {(int)x, (int)y + 1, 3, (int)h - 2};
    SDL_RenderFillRect(r, &leftBar);

    int fs = std::max(14, static_cast<int>(h * 0.52f));
    drawText(r, label, fs, x + w / 2, y + h / 2, "center", Theme::gold, true);
    return y + h;
}

float drawInfoRow(SDL_Renderer* r, float x, float y, float w, float h, const std::string& label,
                  const std::string& value, Color lc, Color vc) {
    if (lc.a == 0) lc = Theme::cream;
    if (vc.a == 0) vc = Theme::grey;


    static int rowCounter = 0;
    if (rowCounter % 2 == 0) {
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(r, 255, 255, 255, 4);
        SDL_Rect rowBg = {(int)x, (int)y, (int)w, (int)h};
        SDL_RenderFillRect(r, &rowBg);
    }
    rowCounter++;


    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, Theme::border.r, Theme::border.g, Theme::border.b, 40);
    SDL_RenderDrawLine(r, (int)x, (int)(y + h - 1), (int)(x + w), (int)(y + h - 1));

    int fs = std::max(14, static_cast<int>(h * 0.60f));

    drawText(r, label, fs, x + Theme::s(4), y + h / 2, "midleft", lc);
    if (!value.empty()) {
        drawText(r, value, fs, x + w - Theme::s(4), y + h / 2, "midright", vc);
    }
    return y + h;
}

void drawIcon(SDL_Renderer* r, const std::string& iconName,
              float x, float y, float size) {
    auto& assets = UIAssets::instance();
    SDL_Texture* tex = assets.icon(iconName);
    if (!tex) return;
    SDL_Rect dst = {(int)(x - size / 2), (int)(y - size / 2), (int)size, (int)size};
    SDL_RenderCopy(r, tex, nullptr, &dst);
}

bool drawIconButton(SDL_Renderer* r, const std::string& iconName,
                    float x, float y, float size,
                    int mouseX, int mouseY, bool mouseDown) {
    auto& assets = UIAssets::instance();
    bool hovered = mouseX >= x && mouseX <= x + size && mouseY >= y && mouseY <= y + size;


    SDL_Texture* btnTex = assets.btnSquare(iconName, hovered);
    if (btnTex) {
        if (mouseDown && hovered) SDL_SetTextureColorMod(btnTex, 160, 160, 165);
        else if (hovered) SDL_SetTextureColorMod(btnTex, 220, 220, 225);
        SDL_Rect dst = {(int)x, (int)y, (int)size, (int)size};
        SDL_RenderCopy(r, btnTex, nullptr, &dst);
        SDL_SetTextureColorMod(btnTex, 255, 255, 255);
    } else {

        Color bg = hovered ? Theme::btn_hover : Theme::btn;
        drawRoundedRect(r, bg, x, y, size, size, 6, hovered ? Theme::gold_dim : Theme::border);
        SDL_Texture* iconTex = assets.icon(iconName);
        if (iconTex) {
            float pad = size * 0.25f;
            SDL_Rect idst = {(int)(x + pad), (int)(y + pad), (int)(size - pad * 2), (int)(size - pad * 2)};
            SDL_RenderCopy(r, iconTex, nullptr, &idst);
        }
    }
    return hovered && mouseDown;
}

}
