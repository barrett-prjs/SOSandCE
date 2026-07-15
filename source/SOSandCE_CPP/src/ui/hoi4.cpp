#include "ui/hoi4.h"
#include "ui/theme.h"
#include "ui/primitives.h"
#include "ui/ui_assets.h"
#include "core/engine.h"

namespace HOI4 {

namespace {

constexpr Color kEdge     {  9,  10,  10 };
constexpr Color kBevelHi  { 92,  98,  88 };
constexpr Color kBevelLo  { 13,  15,  14 };
constexpr Color kPanelTop { 44,  48,  45 };
constexpr Color kPanelBot { 25,  28,  26 };
constexpr Color kPlateTop { 58,  63,  58 };
constexpr Color kPlateBot { 38,  42,  39 };
constexpr Color kInsetTop { 18,  20,  19 };
constexpr Color kInsetBot { 27,  30,  28 };
constexpr Color kHeadTop  { 54,  58,  52 };
constexpr Color kHeadBot  { 34,  37,  33 };
constexpr Color kBtnTop   { 68,  74,  68 };
constexpr Color kBtnBot   { 42,  46,  43 };

Color scale(Color c, float f) {
    auto ch = [&](uint8_t v) {
        return static_cast<uint8_t>(std::clamp(static_cast<int>(v * f), 0, 255));
    };
    return {ch(c.r), ch(c.g), ch(c.b), c.a};
}

void line(SDL_Renderer* r, Color c, int alpha, int x1, int y1, int x2, int y2) {
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, static_cast<uint8_t>(alpha));
    SDL_RenderDrawLine(r, x1, y1, x2, y2);
}

void grain(SDL_Renderer* r, float x, float y, float w, float h, int alpha) {
    SDL_Texture* tex = UIAssets::instance().hoi4Panel();
    if (!tex) return;
    int tw, th;
    SDL_QueryTexture(tex, nullptr, nullptr, &tw, &th);
    SDL_Rect src = {tw / 5, th / 5, tw * 3 / 5, th * 3 / 5};
    SDL_Rect dst = {(int)x, (int)y, (int)w, (int)h};
    SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
    SDL_SetTextureAlphaMod(tex, static_cast<uint8_t>(alpha));
    SDL_RenderCopy(r, tex, &src, &dst);
    SDL_SetTextureAlphaMod(tex, 255);
}

}

std::string caps(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) out += static_cast<char>(::toupper(static_cast<unsigned char>(c)));
    return out;
}

std::string tracked(const std::string& s) {
    std::string out;
    out.reserve(s.size() * 2);
    for (char c : s) {
        out += static_cast<char>(::toupper(static_cast<unsigned char>(c)));
        out += ' ';
    }
    if (!out.empty()) out.pop_back();
    return out;
}

void frame(SDL_Renderer* r, float fx, float fy, float fw, float fh, bool gold) {
    int x = (int)fx, y = (int)fy, w = (int)fw, h = (int)fh;
    if (w < 4 || h < 4) return;
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, kEdge.r, kEdge.g, kEdge.b, 255);
    SDL_Rect outer = {x, y, w, h};
    SDL_RenderDrawRect(r, &outer);
    Color hi = gold ? Theme::gold_dim : kBevelHi;
    line(r, hi, gold ? 220 : 150, x + 1, y + 1, x + w - 2, y + 1);
    line(r, hi, gold ? 180 : 105, x + 1, y + 2, x + 1, y + h - 3);
    line(r, kBevelLo, 200, x + 1, y + h - 2, x + w - 2, y + h - 2);
    line(r, kBevelLo, 150, x + w - 2, y + 2, x + w - 2, y + h - 3);
}

void panel(SDL_Renderer* r, float x, float y, float w, float h, int bodyAlpha) {
    Color top = kPanelTop, bot = kPanelBot;
    top.a = static_cast<uint8_t>(bodyAlpha);
    bot.a = static_cast<uint8_t>(bodyAlpha);
    UIPrim::drawGradientV(r, x, y, w, h, top, bot);
    grain(r, x + 2, y + 2, w - 4, h - 4, 26);
    frame(r, x, y, w, h, false);
}

void plate(SDL_Renderer* r, float x, float y, float w, float h) {
    UIPrim::drawGradientV(r, x, y, w, h, kPlateTop, kPlateBot);
    frame(r, x, y, w, h, false);
}

void inset(SDL_Renderer* r, float x, float y, float w, float h) {
    UIPrim::drawGradientV(r, x, y, w, h, kInsetTop, kInsetBot);
    int ix = (int)x, iy = (int)y, iw = (int)w, ih = (int)h;
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, kEdge.r, kEdge.g, kEdge.b, 220);
    SDL_Rect outer = {ix, iy, iw, ih};
    SDL_RenderDrawRect(r, &outer);
    line(r, {0, 0, 0}, 160, ix + 1, iy + 1, ix + iw - 2, iy + 1);
    line(r, {0, 0, 0}, 110, ix + 1, iy + 2, ix + 1, iy + ih - 3);
    line(r, kBevelHi, 55, ix + 1, iy + ih - 2, ix + iw - 2, iy + ih - 2);
}

void button(SDL_Renderer* r, float x, float y, float w, float h,
            bool hovered, bool pressed, bool enabled) {
    Color top = kBtnTop, bot = kBtnBot;
    if (!enabled) {
        top = scale(kBtnTop, 0.62f);
        bot = scale(kBtnBot, 0.62f);
    } else if (pressed && hovered) {
        top = scale(kBtnBot, 0.92f);
        bot = scale(kBtnTop, 0.92f);
    } else if (hovered) {
        top = scale(kBtnTop, 1.18f);
        bot = scale(kBtnBot, 1.18f);
    }
    UIPrim::drawGradientV(r, x, y, w, h, top, bot);

    int ix = (int)x, iy = (int)y, iw = (int)w, ih = (int)h;
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, kEdge.r, kEdge.g, kEdge.b, 255);
    SDL_Rect outer = {ix, iy, iw, ih};
    SDL_RenderDrawRect(r, &outer);

    if (pressed && hovered && enabled) {
        line(r, {0, 0, 0}, 140, ix + 1, iy + 1, ix + iw - 2, iy + 1);
    } else {
        line(r, kBevelHi, enabled ? 160 : 70, ix + 1, iy + 1, ix + iw - 2, iy + 1);
        line(r, kBevelLo, 170, ix + 1, iy + ih - 2, ix + iw - 2, iy + ih - 2);
    }
    if (hovered && enabled) {
        SDL_SetRenderDrawColor(r, Theme::gold_dim.r, Theme::gold_dim.g, Theme::gold_dim.b, 190);
        SDL_Rect rim = {ix + 1, iy + 1, iw - 2, ih - 2};
        SDL_RenderDrawRect(r, &rim);
    }
}

void tab(SDL_Renderer* r, float x, float y, float w, float h, bool active, bool hovered) {
    int ix = (int)x, iy = (int)y, iw = (int)w, ih = (int)h;
    int cut = std::min(ih / 4, 7);
    Color top = active ? kPlateTop : (hovered ? scale(kHeadBot, 1.35f) : kHeadBot);
    Color bot = active ? kPanelTop : scale(kInsetBot, 1.1f);

    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    for (int row = 0; row < ih; row++) {
        float t = ih > 1 ? static_cast<float>(row) / (ih - 1) : 0.0f;
        uint8_t cr = static_cast<uint8_t>(top.r + (bot.r - top.r) * t);
        uint8_t cg = static_cast<uint8_t>(top.g + (bot.g - top.g) * t);
        uint8_t cb = static_cast<uint8_t>(top.b + (bot.b - top.b) * t);
        int inset2 = std::max(0, cut - row);
        SDL_SetRenderDrawColor(r, cr, cg, cb, 255);
        SDL_RenderDrawLine(r, ix + inset2, iy + row, ix + iw - 1 - inset2, iy + row);
    }

    SDL_SetRenderDrawColor(r, kEdge.r, kEdge.g, kEdge.b, 255);
    SDL_RenderDrawLine(r, ix + cut, iy, ix + iw - 1 - cut, iy);
    SDL_RenderDrawLine(r, ix, iy + cut, ix + cut, iy);
    SDL_RenderDrawLine(r, ix + iw - 1 - cut, iy, ix + iw - 1, iy + cut);
    SDL_RenderDrawLine(r, ix, iy + cut, ix, iy + ih - 1);
    SDL_RenderDrawLine(r, ix + iw - 1, iy + cut, ix + iw - 1, iy + ih - 1);

    if (active) {
        line(r, Theme::gold, 235, ix + cut + 1, iy + 1, ix + iw - 2 - cut, iy + 1);
        line(r, Theme::gold_dim, 140, ix + cut, iy + 2, ix + iw - 1 - cut, iy + 2);
    } else {
        SDL_RenderDrawLine(r, ix, iy + ih - 1, ix + iw - 1, iy + ih - 1);
    }
}

void diagPlate(SDL_Renderer* r, float x, float y, float w, float h, int cut, bool cutLeft) {
    int ix = (int)x, iy = (int)y, iw = (int)w, ih = (int)h;
    if (iw < cut + 4 || ih < 4) return;
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, kEdge.r, kEdge.g, kEdge.b, 255);
    if (cutLeft) {
        SDL_RenderDrawLine(r, ix + cut, iy, ix, iy + ih - 1);
        line(r, Theme::gold_dim, 170, ix + cut + 1, iy + 1, ix + 1, iy + ih - 1);
    } else {
        SDL_RenderDrawLine(r, ix + iw - 1 - cut, iy, ix + iw - 1, iy + ih - 1);
        line(r, Theme::gold_dim, 170, ix + iw - 2 - cut, iy + 1, ix + iw - 2, iy + ih - 1);
    }
}

void topBar(SDL_Renderer* r, float w, float h) {
    UIPrim::drawGradientV(r, 0, 0, w, h, {41, 45, 43, 255}, {21, 24, 23, 255});
    float seg = h * 6.0f;
    for (float gx = 0; gx < w; gx += seg) {
        grain(r, gx, 0, std::min(seg, w - gx), h - 2, 20);
    }
    line(r, kBevelHi, 90, 0, 0, (int)w, 0);
    line(r, Theme::gold_dim, 200, 0, (int)h - 2, (int)w, (int)h - 2);
    line(r, kEdge, 255, 0, (int)h - 1, (int)w, (int)h - 1);
}

void vDivider(SDL_Renderer* r, float x, float y1, float y2) {
    line(r, kEdge, 170, (int)x, (int)y1, (int)x, (int)y2);
    line(r, kBevelHi, 35, (int)x + 1, (int)y1, (int)x + 1, (int)y2);
}

void cell(SDL_Renderer* r, float x, float y, float w, float h) {
    int ix = (int)x, iy = (int)y, iw = (int)w, ih = (int)h;
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, 0, 0, 0, 85);
    SDL_Rect fill = {ix + 1, iy + 1, iw - 2, ih - 2};
    SDL_RenderFillRect(r, &fill);
    SDL_SetRenderDrawColor(r, kEdge.r, kEdge.g, kEdge.b, 230);
    SDL_Rect outer = {ix, iy, iw, ih};
    SDL_RenderDrawRect(r, &outer);
    line(r, {0, 0, 0}, 80, ix + 1, iy + 1, ix + iw - 2, iy + 1);
    line(r, kBevelHi, 45, ix + 1, iy + ih - 2, ix + iw - 2, iy + ih - 2);
}

void goldDivider(SDL_Renderer* r, float x1, float x2, float y) {
    float len = x2 - x1;
    if (len <= 0) return;
    float fade = std::min(len * 0.18f, 26.0f);
    line(r, Theme::gold_dim, 60, (int)x1, (int)y, (int)(x1 + fade), (int)y);
    line(r, Theme::gold_dim, 210, (int)(x1 + fade), (int)y, (int)(x2 - fade), (int)y);
    line(r, Theme::gold_dim, 60, (int)(x2 - fade), (int)y, (int)x2, (int)y);
}

float sectionHeader(SDL_Renderer* r, float x, float y, float w, float h,
                    const std::string& title) {
    UIPrim::drawGradientV(r, x, y, w, h, kHeadTop, kHeadBot);
    line(r, kBevelHi, 120, (int)x, (int)y, (int)(x + w), (int)y);
    goldDivider(r, x, x + w, y + h - 1);
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, Theme::gold.r, Theme::gold.g, Theme::gold.b, 220);
    SDL_Rect notch = {(int)x, (int)y + 2, 3, (int)h - 4};
    SDL_RenderFillRect(r, &notch);
    int fs = std::max(12, static_cast<int>(h * 0.42f));
    UIPrim::drawText(r, UIPrim::fitText(tracked(title), fs, w - 20.0f), fs,
                     x + 12.0f, y + h * 0.5f, "midleft", Theme::gold_bright, true);
    return y + h;
}

void sectionLabel(SDL_Renderer* r, float x, float y, const std::string& title, int fontSize) {
    std::string t = caps(title);
    UIPrim::drawText(r, t, fontSize, x, y, "topleft", Theme::gold, true);
    int tw = UIPrim::textWidth(t, fontSize);
    int th = UIPrim::textHeight(fontSize);
    line(r, Theme::gold_dim, 170, (int)x, (int)(y + th + 1), (int)(x + tw), (int)(y + th + 1));
    line(r, Theme::gold_dim, 55, (int)(x + tw), (int)(y + th + 1), (int)(x + tw + 18), (int)(y + th + 1));
}

}
