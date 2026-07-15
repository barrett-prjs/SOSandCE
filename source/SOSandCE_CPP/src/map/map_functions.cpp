#include "map/map_functions.h"
#include "data/region_data.h"
#include "core/engine.h"

#include <algorithm>
#include <mutex>
#include <stack>
#include <unordered_set>


static const Color WATER_COLOR = {126, 142, 158};
static const Color BLACK_COLOR = {0, 0, 0};
static const Color BORDER_GREY = {105, 118, 132};

static bool isWater(Color c) {
    return c.r == WATER_COLOR.r && c.g == WATER_COLOR.g && c.b == WATER_COLOR.b;
}

static bool isBlack(Color c) {
    return c.r == 0 && c.g == 0 && c.b == 0;
}

static bool isMaskBarrier(Color c) {
    return isBlack(c) || isWater(c) || c == BORDER_GREY;
}

static bool findRegionAnchor(SDL_Surface* maskSurface, Color regionColor, int approxX, int approxY,
                             int& outX, int& outY) {
    if (!maskSurface) return false;
    if (isMaskBarrier(regionColor)) return false;

    approxX = std::clamp(approxX, 0, maskSurface->w - 1);
    approxY = std::clamp(approxY, 0, maskSurface->h - 1);

    Color current = getPixel(maskSurface, approxX, approxY);
    if (current == regionColor) {
        outX = approxX;
        outY = approxY;
        return true;
    }

    const int maxRadius = 96;
    for (int radius = 1; radius <= maxRadius; ++radius) {
        int minX = std::max(0, approxX - radius);
        int maxX = std::min(maskSurface->w - 1, approxX + radius);
        int minY = std::max(0, approxY - radius);
        int maxY = std::min(maskSurface->h - 1, approxY + radius);

        for (int y = minY; y <= maxY; ++y) {
            for (int x = minX; x <= maxX; ++x) {
                if (x != minX && x != maxX && y != minY && y != maxY) continue;
                if (getPixel(maskSurface, x, y) == regionColor) {
                    outX = x;
                    outY = y;
                    return true;
                }
            }
        }
    }

    for (int y = 0; y < maskSurface->h; ++y) {
        for (int x = 0; x < maskSurface->w; ++x) {
            if (getPixel(maskSurface, x, y) == regionColor) {
                outX = x;
                outY = y;
                return true;
            }
        }
    }

    return false;
}


void MapFunc::fill(SDL_Surface* surface, float x, float y, Color newColor) {
    if (!surface) return;

    int sx = static_cast<int>(x);
    int sy = static_cast<int>(y);
    if (sx < 0 || sy < 0 || sx >= surface->w || sy >= surface->h) return;

    SDL_LockSurface(surface);

    Color oldColor = getPixel(surface, sx, sy);
    if (oldColor == newColor) {
        SDL_UnlockSurface(surface);
        return;
    }

    std::stack<std::pair<int, int>> stack;
    stack.push({sx, sy});

    while (!stack.empty()) {
        auto [cx, cy] = stack.top();
        stack.pop();

        if (cx < 0 || cy < 0 || cx >= surface->w || cy >= surface->h) continue;

        Color current = getPixel(surface, cx, cy);
        if (current != oldColor) continue;

        setPixel(surface, cx, cy, newColor);

        stack.push({cx + 1, cy});
        stack.push({cx - 1, cy});
        stack.push({cx, cy + 1});
        stack.push({cx, cy - 1});
    }

    SDL_UnlockSurface(surface);
}

void MapFunc::fillRegionMask(SDL_Surface* surface, SDL_Surface* maskSurface,
                             float x, float y, Color newColor) {
    if (!surface || !maskSurface) return;

    int sx = static_cast<int>(x);
    int sy = static_cast<int>(y);
    if (sx < 0 || sy < 0 || sx >= surface->w || sy >= surface->h) return;
    if (sx >= maskSurface->w || sy >= maskSurface->h) return;

    SDL_LockSurface(surface);
    SDL_LockSurface(maskSurface);

    Color maskColor = getPixel(maskSurface, sx, sy);
    if (isMaskBarrier(maskColor)) {
        SDL_UnlockSurface(maskSurface);
        SDL_UnlockSurface(surface);
        return;
    }

    std::stack<std::pair<int, int>> stack;
    stack.push({sx, sy});

    while (!stack.empty()) {
        auto [cx, cy] = stack.top();
        stack.pop();

        if (cx < 0 || cy < 0 || cx >= surface->w || cy >= surface->h) continue;
        if (cx >= maskSurface->w || cy >= maskSurface->h) continue;

        Color currentMask = getPixel(maskSurface, cx, cy);
        if (currentMask != maskColor) continue;

        Color currentColor = getPixel(surface, cx, cy);
        if (currentColor == newColor) continue;

        setPixel(surface, cx, cy, newColor);

        stack.push({cx + 1, cy});
        stack.push({cx - 1, cy});
        stack.push({cx, cy + 1});
        stack.push({cx, cy - 1});
    }

    SDL_UnlockSurface(maskSurface);
    SDL_UnlockSurface(surface);
}

void MapFunc::fillRegionMask(SDL_Surface* surface, SDL_Surface* maskSurface,
                             int regionId, float x, float y, Color newColor) {
    if (!surface || !maskSurface) return;

    int sx = static_cast<int>(x);
    int sy = static_cast<int>(y);
    if (sx < 0 || sy < 0 || sx >= maskSurface->w || sy >= maskSurface->h) return;

    Color regionColor = RegionData::instance().getRegionColor(regionId);

    SDL_LockSurface(maskSurface);
    int anchorX = sx;
    int anchorY = sy;
    bool foundAnchor = findRegionAnchor(maskSurface, regionColor, sx, sy, anchorX, anchorY);
    SDL_UnlockSurface(maskSurface);
    if (!foundAnchor) return;

    SDL_LockSurface(surface);
    SDL_LockSurface(maskSurface);

    std::stack<std::pair<int, int>> stack;
    stack.push({anchorX, anchorY});

    while (!stack.empty()) {
        auto [cx, cy] = stack.top();
        stack.pop();

        if (cx < 0 || cy < 0 || cx >= surface->w || cy >= surface->h) continue;
        if (cx >= maskSurface->w || cy >= maskSurface->h) continue;

        if (getPixel(maskSurface, cx, cy) != regionColor) continue;
        if (getPixel(surface, cx, cy) == newColor) continue;

        setPixel(surface, cx, cy, newColor);

        stack.push({cx + 1, cy});
        stack.push({cx - 1, cy});
        stack.push({cx, cy + 1});
        stack.push({cx, cy - 1});
    }

    SDL_UnlockSurface(maskSurface);
    SDL_UnlockSurface(surface);
}


void MapFunc::fillWithBorder(SDL_Surface* surface, SDL_Surface* referenceSurface,
                             float x, float y, Color newColor) {
    if (!surface || !referenceSurface) return;

    int sx = static_cast<int>(x);
    int sy = static_cast<int>(y);
    if (sx < 0 || sy < 0 || sx >= surface->w || sy >= surface->h) return;

    SDL_LockSurface(surface);
    SDL_LockSurface(referenceSurface);

    Color oldColor = getPixel(surface, sx, sy);
    if (oldColor == newColor) {
        SDL_UnlockSurface(referenceSurface);
        SDL_UnlockSurface(surface);
        return;
    }


    Color borderColor;
    borderColor.r = static_cast<uint8_t>(std::max(0.0f, newColor.r / 1.4f));
    borderColor.g = static_cast<uint8_t>(std::max(0.0f, newColor.g / 1.4f));
    borderColor.b = static_cast<uint8_t>(std::max(0.0f, newColor.b / 1.4f));
    borderColor.a = 255;

    std::stack<std::pair<int, int>> stack;
    stack.push({sx, sy});

    while (!stack.empty()) {
        auto [cx, cy] = stack.top();
        stack.pop();

        if (cx < 0 || cy < 0 || cx >= surface->w || cy >= surface->h) continue;

        Color current = getPixel(surface, cx, cy);
        if (current != oldColor) continue;


        Color refColor = getPixel(referenceSurface, cx, cy);

        if (isWater(refColor)) {

            continue;
        } else if (isBlack(refColor)) {

            setPixel(surface, cx, cy, BLACK_COLOR);
            continue;
        } else if (refColor == BORDER_GREY) {

            setPixel(surface, cx, cy, borderColor);
            continue;
        }


        setPixel(surface, cx, cy, newColor);

        stack.push({cx + 1, cy});
        stack.push({cx - 1, cy});
        stack.push({cx, cy + 1});
        stack.push({cx, cy - 1});
    }

    SDL_UnlockSurface(referenceSurface);
    SDL_UnlockSurface(surface);
}


void MapFunc::fillFixBorder(SDL_Surface* surface, float x, float y, Color newColor) {
    if (!surface) return;

    int sx = static_cast<int>(x);
    int sy = static_cast<int>(y);
    if (sx < 0 || sy < 0 || sx >= surface->w || sy >= surface->h) return;

    SDL_LockSurface(surface);

    Color oldColor = getPixel(surface, sx, sy);
    if (oldColor == newColor) {
        SDL_UnlockSurface(surface);
        return;
    }


    Color borderColor;
    borderColor.r = static_cast<uint8_t>(newColor.r / 1.4f);
    borderColor.g = static_cast<uint8_t>(newColor.g / 1.4f);
    borderColor.b = static_cast<uint8_t>(newColor.b / 1.4f);
    borderColor.a = 255;


    std::vector<std::pair<int, int>> borderPixels;
    std::stack<std::pair<int, int>> stack;
    stack.push({sx, sy});

    while (!stack.empty()) {
        auto [cx, cy] = stack.top();
        stack.pop();

        if (cx < 0 || cy < 0 || cx >= surface->w || cy >= surface->h) continue;

        Color current = getPixel(surface, cx, cy);
        if (current != oldColor) continue;

        setPixel(surface, cx, cy, newColor);


        for (int dy = -1; dy <= 1; dy++) {
            for (int dx = -1; dx <= 1; dx++) {
                if (dx == 0 && dy == 0) continue;
                int nx2 = cx + dx, ny2 = cy + dy;
                if (nx2 < 0 || ny2 < 0 || nx2 >= surface->w || ny2 >= surface->h) continue;
                Color nc = getPixel(surface, nx2, ny2);
                if (isBlack(nc)) {
                    borderPixels.push_back({nx2, ny2});
                }
            }
        }

        stack.push({cx + 1, cy});
        stack.push({cx - 1, cy});
        stack.push({cx, cy + 1});
        stack.push({cx, cy - 1});
    }


    SDL_Surface* copy = SDL_ConvertSurface(surface, surface->format, 0);
    if (copy) {
        SDL_LockSurface(copy);
        for (auto& [bx, by] : borderPixels) {

            std::vector<Color> neighborColors;
            for (int dy = -1; dy <= 1; dy++) {
                for (int dx = -1; dx <= 1; dx++) {
                    int nx2 = bx + dx, ny2 = by + dy;
                    if (nx2 < 0 || ny2 < 0 || nx2 >= surface->w || ny2 >= surface->h) continue;
                    Color sc = getPixel(copy, nx2, ny2);
                    if (!isBlack(sc) && !isWater(sc) && sc != BORDER_GREY) {
                        neighborColors.push_back(sc);
                    }
                }
            }

            if (neighborColors.size() == 1) {

                setPixel(surface, bx, by, borderColor);
            } else if (neighborColors.size() > 1) {

                bool allSame = true;
                for (size_t i = 1; i < neighborColors.size(); i++) {
                    if (neighborColors[i] != neighborColors[0]) { allSame = false; break; }
                }
                if (allSame) {
                    setPixel(surface, bx, by, borderColor);
                } else {
                    setPixel(surface, bx, by, BLACK_COLOR);
                }
            }
        }
        SDL_UnlockSurface(copy);
        SDL_FreeSurface(copy);
    }

    SDL_UnlockSurface(surface);
}


void MapFunc::repaintRegionBorders(SDL_Surface* surface, SDL_Surface* maskSurface,
                                   int regionId, float x, float y) {
    if (!surface || !maskSurface) return;

    int sx = static_cast<int>(x);
    int sy = static_cast<int>(y);
    if (sx < 0 || sy < 0 || sx >= maskSurface->w || sy >= maskSurface->h) return;

    Color regionColor = RegionData::instance().getRegionColor(regionId);

    SDL_LockSurface(maskSurface);
    int anchorX = sx;
    int anchorY = sy;
    bool foundAnchor = findRegionAnchor(maskSurface, regionColor, sx, sy, anchorX, anchorY);
    SDL_UnlockSurface(maskSurface);
    if (!foundAnchor) return;

    SDL_LockSurface(surface);
    SDL_LockSurface(maskSurface);

    const int w = maskSurface->w;
    const int h = maskSurface->h;
    auto key = [w](int cx, int cy) { return cy * w + cx; };
    auto inBounds = [&](int cx, int cy) {
        return cx >= 0 && cy >= 0 && cx < w && cy < h && cx < surface->w && cy < surface->h;
    };

    std::unordered_set<int> visited;
    std::unordered_set<int> borderSet;
    std::vector<std::pair<int, int>> borderList;
    std::stack<std::pair<int, int>> stack;
    stack.push({anchorX, anchorY});
    visited.insert(key(anchorX, anchorY));

    while (!stack.empty()) {
        auto [cx, cy] = stack.top();
        stack.pop();

        const int nbs[4][2] = {{cx + 1, cy}, {cx - 1, cy}, {cx, cy + 1}, {cx, cy - 1}};
        for (auto& nb : nbs) {
            int nx = nb[0], ny = nb[1];
            if (!inBounds(nx, ny)) continue;
            int k = key(nx, ny);
            if (visited.count(k)) continue;
            Color mc = getPixel(maskSurface, nx, ny);
            if (mc == regionColor) {
                visited.insert(k);
                stack.push({nx, ny});
            } else if (mc == BORDER_GREY) {
                if (borderSet.insert(k).second) borderList.push_back({nx, ny});
            }
        }
    }

    size_t initialCount = borderList.size();
    for (size_t i = 0; i < initialCount; i++) {
        auto [bx, by] = borderList[i];
        for (int dy = -1; dy <= 1; dy++) {
            for (int dx = -1; dx <= 1; dx++) {
                if (dx == 0 && dy == 0) continue;
                int nx = bx + dx, ny = by + dy;
                if (!inBounds(nx, ny)) continue;
                if (getPixel(maskSurface, nx, ny) == BORDER_GREY) {
                    int k = key(nx, ny);
                    if (borderSet.insert(k).second) borderList.push_back({nx, ny});
                }
            }
        }
    }

    for (auto& [bx, by] : borderList) {
        Color first = {0, 0, 0, 0};
        bool have = false;
        bool mixed = false;
        for (int dy = -1; dy <= 1 && !mixed; dy++) {
            for (int dx = -1; dx <= 1; dx++) {
                if (dx == 0 && dy == 0) continue;
                int nx = bx + dx, ny = by + dy;
                if (!inBounds(nx, ny)) continue;
                if (isMaskBarrier(getPixel(maskSurface, nx, ny))) continue;
                Color sc = getPixel(surface, nx, ny);
                if (!have) {
                    first = sc;
                    have = true;
                } else if (sc != first) {
                    mixed = true;
                    break;
                }
            }
        }
        if (!have) continue;
        if (mixed) {
            setPixel(surface, bx, by, BLACK_COLOR);
        } else {
            Color bc;
            bc.r = static_cast<uint8_t>(first.r / 1.4f);
            bc.g = static_cast<uint8_t>(first.g / 1.4f);
            bc.b = static_cast<uint8_t>(first.b / 1.4f);
            bc.a = 255;
            setPixel(surface, bx, by, bc);
        }
    }

    SDL_UnlockSurface(maskSurface);
    SDL_UnlockSurface(surface);
}


namespace {

int pixelNoise(int x, int y, int amplitude) {
    uint32_t h = static_cast<uint32_t>(x) * 374761393u + static_cast<uint32_t>(y) * 668265263u;
    h = (h ^ (h >> 13)) * 1274126177u;
    h ^= h >> 16;
    return static_cast<int>(h % (2 * amplitude + 1)) - amplitude;
}

float smoothNoiseWrap(float x, float y, int periodX) {
    int x0 = static_cast<int>(std::floor(x));
    int y0 = static_cast<int>(std::floor(y));
    float fx = x - x0;
    float fy = y - y0;
    fx = fx * fx * (3.0f - 2.0f * fx);
    fy = fy * fy * (3.0f - 2.0f * fy);
    int xa = ((x0 % periodX) + periodX) % periodX;
    int xb = (xa + 1) % periodX;
    float a = pixelNoise(xa, y0, 1000) / 1000.0f;
    float b = pixelNoise(xb, y0, 1000) / 1000.0f;
    float c = pixelNoise(xa, y0 + 1, 1000) / 1000.0f;
    float d = pixelNoise(xb, y0 + 1, 1000) / 1000.0f;
    float top = a + (b - a) * fx;
    float bot = c + (d - c) * fx;
    return top + (bot - top) * fy;
}

uint8_t clampChannel(int v) {
    return static_cast<uint8_t>(std::clamp(v, 0, 255));
}

struct Px { uint8_t r, g, b, a; };

inline Px* rowPx(SDL_Surface* s, int y) {
    return reinterpret_cast<Px*>(static_cast<uint8_t*>(s->pixels) + y * s->pitch);
}
inline bool samePx(Px a, Px b) { return a.r == b.r && a.g == b.g && a.b == b.b; }
inline bool isWaterPx(Px p) { return p.r == 126 && p.g == 142 && p.b == 158; }
inline bool isGreyPx(Px p)  { return p.r == 105 && p.g == 118 && p.b == 132; }
inline bool isLinePx(Px p)  { return (p.r == 0 && p.g == 0 && p.b == 0) || isGreyPx(p); }

SDL_Surface* reconstructFills(SDL_Surface* colorSrc, SDL_Surface* mask) {
    if (!colorSrc || !mask) return nullptr;

    SDL_Surface* out = SDL_ConvertSurfaceFormat(colorSrc, SDL_PIXELFORMAT_RGBA32, 0);
    if (!out) return nullptr;
    SDL_Surface* m = mask->format->format == SDL_PIXELFORMAT_RGBA32
                   ? mask
                   : SDL_ConvertSurfaceFormat(mask, SDL_PIXELFORMAT_RGBA32, 0);
    if (!m) {
        SDL_FreeSurface(out);
        return nullptr;
    }

    const int w = std::min(out->w, m->w);
    const int h = std::min(out->h, m->h);

    SDL_LockSurface(out);
    SDL_LockSurface(m);

    for (int y = 0; y < h; y++) {
        Px* mr = rowPx(m, y);
        Px* orow = rowPx(out, y);
        for (int x = 0; x < w; x++) {
            if (!isLinePx(mr[x])) continue;

            bool found = false;
            bool haveWater = false;
            Px waterFill = {0, 0, 0, 0};
            for (int radius = 1; radius <= 4 && !found; radius++) {
                for (int dy = -radius; dy <= radius && !found; dy++) {
                    int ny = y + dy;
                    if (ny < 0 || ny >= h) continue;
                    Px* mrn = rowPx(m, ny);
                    Px* orn = rowPx(out, ny);
                    for (int dx = -radius; dx <= radius; dx++) {
                        if (std::max(std::abs(dx), std::abs(dy)) != radius) continue;
                        int nx = x + dx;
                        if (nx < 0 || nx >= w) continue;
                        if (isLinePx(mrn[nx])) continue;
                        Px cand = orn[nx];
                        if (radius == 1 && (isWaterPx(cand) || isGreyPx(cand))) {
                            if (!haveWater) { waterFill = cand; haveWater = true; }
                            continue;
                        }
                        orow[x] = cand;
                        found = true;
                        break;
                    }
                }
                if (!found && radius == 1 && haveWater) {
                    orow[x] = waterFill;
                    found = true;
                }
            }
            if (!found && haveWater) orow[x] = waterFill;
        }
    }

    SDL_UnlockSurface(m);
    SDL_UnlockSurface(out);
    if (m != mask) SDL_FreeSurface(m);
    return out;
}

}

SDL_Surface* MapFunc::buildTerrainSurface(SDL_Surface* biomeMap, SDL_Surface* grainTex,
                                          int scale) {
    if (!biomeMap) return nullptr;
    scale = std::max(1, scale);

    SDL_Surface* pass1 = fixBorders(biomeMap, {{0, 0, 0}, BORDER_GREY}, {});
    if (!pass1) return nullptr;
    SDL_Surface* pass2 = fixBorders(pass1, {{0, 0, 0}, BORDER_GREY}, {});
    SDL_FreeSurface(pass1);
    if (!pass2) return nullptr;

    SDL_Surface* src = SDL_ConvertSurfaceFormat(pass2, SDL_PIXELFORMAT_RGBA32, 0);
    SDL_FreeSurface(pass2);
    if (!src) return nullptr;

    SDL_Surface* orig = SDL_ConvertSurfaceFormat(biomeMap, SDL_PIXELFORMAT_RGBA32, 0);
    if (orig) {
        SDL_LockSurface(orig);
        SDL_LockSurface(src);
        for (int y = 0; y < src->h; y++) {
            Px* srow = rowPx(src, y);
            Px* orow = rowPx(orig, y);
            for (int x = 0; x < src->w; x++) {
                if (!isLinePx(orow[x])) continue;
                bool nearWater = false;
                for (int dy = -2; dy <= 2 && !nearWater; dy++) {
                    int ny = y + dy;
                    if (ny < 0 || ny >= orig->h) continue;
                    Px* nrow = rowPx(orig, ny);
                    for (int dx = -2; dx <= 2; dx++) {
                        int nx = x + dx;
                        if (nx < 0 || nx >= orig->w) continue;
                        if (isWaterPx(nrow[nx])) { nearWater = true; break; }
                    }
                }
                if (nearWater) srow[x] = {126, 142, 158, 255};
            }
        }
        SDL_UnlockSurface(src);
        SDL_UnlockSurface(orig);
        SDL_FreeSurface(orig);
    }

    const int bw = src->w;
    const int bh = src->h;
    const int w = bw * scale;
    const int h = bh * scale;
    const float pi = 3.14159265f;

    SDL_Surface* grain = grainTex
        ? SDL_ConvertSurfaceFormat(grainTex, SDL_PIXELFORMAT_RGBA32, 0)
        : nullptr;

    SDL_Surface* terrain = SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_RGBA32);
    if (!terrain) {
        if (grain) SDL_FreeSurface(grain);
        SDL_FreeSurface(src);
        return nullptr;
    }

    SDL_LockSurface(terrain);
    SDL_LockSurface(src);
    if (grain) SDL_LockSurface(grain);

    const int gw = grain ? grain->w : 0;
    const int gh = grain ? grain->h : 0;

    const bool oceanDetail = Engine::instance().oceanDetail;
    const uint16_t kFarDist = 60000;
    const int kPad = 64;
    const int ew = bw + kPad * 2;
    auto wrapCol = [&](int x) { return ((x % bw) + bw) % bw; };

    auto chamfer = [&](std::vector<uint16_t>& d) {
        for (int y = 0; y < bh; y++) {
            size_t i = static_cast<size_t>(y) * ew;
            for (int x = 0; x < ew; x++, i++) {
                uint16_t v = d[i];
                if (v == 0) continue;
                if (x > 0) v = std::min<uint16_t>(v, d[i - 1] + 3);
                if (y > 0) {
                    v = std::min<uint16_t>(v, d[i - ew] + 3);
                    if (x > 0) v = std::min<uint16_t>(v, d[i - ew - 1] + 4);
                    if (x + 1 < ew) v = std::min<uint16_t>(v, d[i - ew + 1] + 4);
                }
                d[i] = v;
            }
        }
        for (int y = bh - 1; y >= 0; y--) {
            size_t i = static_cast<size_t>(y) * ew + (ew - 1);
            for (int x = ew - 1; x >= 0; x--, i--) {
                uint16_t v = d[i];
                if (v == 0) continue;
                if (x + 1 < ew) v = std::min<uint16_t>(v, d[i + 1] + 3);
                if (y + 1 < bh) {
                    v = std::min<uint16_t>(v, d[i + ew] + 3);
                    if (x + 1 < ew) v = std::min<uint16_t>(v, d[i + ew + 1] + 4);
                    if (x > 0) v = std::min<uint16_t>(v, d[i + ew - 1] + 4);
                }
                d[i] = v;
            }
        }
    };

    std::vector<uint16_t> distAll;
    std::vector<uint16_t> landDepth;
    std::vector<uint16_t> distBig;
    if (oceanDetail) {
        distAll.resize(static_cast<size_t>(ew) * bh);
        landDepth.resize(static_cast<size_t>(ew) * bh);
        for (int y = 0; y < bh; y++) {
            Px* row = rowPx(src, y);
            size_t base = static_cast<size_t>(y) * ew;
            for (int ex = 0; ex < ew; ex++) {
                bool isW = false;
                {
                    Px p = row[wrapCol(ex - kPad)];
                    isW = isWaterPx(p) || isGreyPx(p);
                }
                distAll[base + ex] = isW ? kFarDist : 0;
                landDepth[base + ex] = isW ? 0 : kFarDist;
            }
        }
        chamfer(distAll);
        chamfer(landDepth);

        distBig.resize(static_cast<size_t>(ew) * bh);
        for (size_t i = 0; i < distBig.size(); i++) {
            distBig[i] = (landDepth[i] > 9) ? 0 : kFarDist;
        }
        chamfer(distBig);
    }

    auto sampleField = [&](const std::vector<uint16_t>& f, int px, int py) -> float {
        float fx = (px + 0.5f) / scale - 0.5f + kPad;
        float fy = (py + 0.5f) / scale - 0.5f;
        int x0 = std::clamp(static_cast<int>(std::floor(fx)), 0, ew - 1);
        int y0 = std::clamp(static_cast<int>(std::floor(fy)), 0, bh - 1);
        int x1 = std::min(x0 + 1, ew - 1);
        int y1 = std::min(y0 + 1, bh - 1);
        float tx = std::clamp(fx - x0, 0.0f, 1.0f);
        float ty = std::clamp(fy - y0, 0.0f, 1.0f);
        float d00 = f[static_cast<size_t>(y0) * ew + x0];
        float d10 = f[static_cast<size_t>(y0) * ew + x1];
        float d01 = f[static_cast<size_t>(y1) * ew + x0];
        float d11 = f[static_cast<size_t>(y1) * ew + x1];
        float top = d00 + (d10 - d00) * tx;
        float bot = d01 + (d11 - d01) * tx;
        return (top + (bot - top) * ty) / 3.0f;
    };

    const int noiseCells1 = std::max(1, static_cast<int>(std::lround(w / 88.0)));
    const float noiseWl1 = static_cast<float>(w) / noiseCells1;
    const int noiseCells2 = std::max(1, static_cast<int>(std::lround(w / 152.0)));
    const float noiseWl2 = static_cast<float>(w) / noiseCells2;
    const int noiseCellsW = std::max(1, static_cast<int>(std::lround(w / 214.0)));
    const float noiseWlW = static_cast<float>(w) / noiseCellsW;
    const float gridStep = static_cast<float>(w) /
                           std::max(1, static_cast<int>(std::lround(w / (50.0 * scale))));
    const float grainTileW = (gw > 0)
        ? static_cast<float>(w) / std::max(1, static_cast<int>(std::lround(w / (gw * 2.0))))
        : 1.0f;

    for (int y = 0; y < h; y++) {
        float latFactor = 0.90f + 0.18f * std::sin(pi * static_cast<float>(y) / h);
        Px* trow = rowPx(terrain, y);
        Px* srow = rowPx(src, y / scale);
        Px* grow = nullptr;
        if (grain && gw > 0 && gh > 0) {
            int gy = y % (gh * 2);
            if (gy >= gh) gy = gh * 2 - 1 - gy;
            grow = rowPx(grain, gy);
        }
        bool gridRow = (y % (50 * scale) < scale);

        for (int x = 0; x < w; x++) {
            Px s = srow[x / scale];
            int noise = pixelNoise(x, y, 4);
            int r, g, b;

            if (isWaterPx(s) || isGreyPx(s)) {
                if (!oceanDetail) {
                    r = static_cast<int>(30 * latFactor);
                    g = static_cast<int>(48 * latFactor);
                    b = static_cast<int>(64 * latFactor);
                    if (gridRow || std::fmod(static_cast<float>(x), gridStep) < scale) {
                        r += 9; g += 9; b += 10;
                    }
                    r += noise / 2; g += noise / 2; b += noise / 2;
                    if (grow) {
                        float gxf0 = std::fmod(static_cast<float>(x), grainTileW) / grainTileW * (gw * 2);
                        int gx0 = std::clamp(static_cast<int>(gxf0), 0, gw * 2 - 1);
                        if (gx0 >= gw) gx0 = gw * 2 - 1 - gx0;
                        Px gp0 = grow[gx0];
                        float lum0 = (gp0.r + gp0.g + gp0.b) / (3.0f * 255.0f);
                        float factor0 = 0.84f + 0.32f * lum0;
                        r = static_cast<int>(r * factor0);
                        g = static_cast<int>(g * factor0);
                        b = static_cast<int>(b * factor0);
                    }
                    trow[x] = {clampChannel(r), clampChannel(g), clampChannel(b), 255};
                    continue;
                }
                float dAll = sampleField(distAll, x, y);
                float dBig = sampleField(distBig, x, y);
                float shelfN = std::clamp(1.0f - dAll / 11.0f, 0.0f, 1.0f);
                shelfN = shelfN * shelfN * (3.0f - 2.0f * shelfN);
                float shelfW = std::clamp(1.0f - dBig / 42.0f, 0.0f, 1.0f);
                shelfW = shelfW * shelfW * (3.0f - 2.0f * shelfW);
                float shelf = std::max(shelfW, shelfN * 0.7f);
                float rim = dAll < 3.5f ? (1.0f - dAll / 3.5f) : 0.0f;

                float warp = smoothNoiseWrap(x / noiseWlW, y / noiseWlW, noiseCellsW) * 30.0f;
                float n = smoothNoiseWrap(x / noiseWl1, (y + warp) / noiseWl1, noiseCells1) * 0.6f +
                          smoothNoiseWrap(x / noiseWl2 + 37.0f, (y - warp) / noiseWl2 + 11.0f, noiseCells2) * 0.4f;
                float relief = (n - 0.30f) * 3.4f * (1.0f - shelf);

                float br = 26.0f + 20.0f * shelf + 8.0f * rim + relief;
                float bg2 = 42.0f + 27.0f * shelf + 10.0f * rim + relief;
                float bb = 58.0f + 30.0f * shelf + 11.0f * rim + relief;

                r = static_cast<int>(br * latFactor);
                g = static_cast<int>(bg2 * latFactor);
                b = static_cast<int>(bb * latFactor);
                if (gridRow || std::fmod(static_cast<float>(x), gridStep) < scale) {
                    r += 9; g += 9; b += 10;
                }
                r += noise / 2; g += noise / 2; b += noise / 2;
            } else {
                int grey = (s.r + s.g + s.b) / 3;
                r = static_cast<int>((s.r + (grey - s.r) * 0.35f) * 0.90f) + noise;
                g = static_cast<int>((s.g + (grey - s.g) * 0.35f) * 0.90f) + noise;
                b = static_cast<int>((s.b + (grey - s.b) * 0.35f) * 0.90f) + noise;
            }

            if (grow) {
                float gxf = std::fmod(static_cast<float>(x), grainTileW) / grainTileW * (gw * 2);
                int gx = std::clamp(static_cast<int>(gxf), 0, gw * 2 - 1);
                if (gx >= gw) gx = gw * 2 - 1 - gx;
                Px gp = grow[gx];
                float lum = (gp.r + gp.g + gp.b) / (3.0f * 255.0f);
                float factor = 0.84f + 0.32f * lum;
                r = static_cast<int>(r * factor);
                g = static_cast<int>(g * factor);
                b = static_cast<int>(b * factor);
            }

            trow[x] = {clampChannel(r), clampChannel(g), clampChannel(b), 255};
        }
    }

    if (grain) {
        SDL_UnlockSurface(grain);
        SDL_FreeSurface(grain);
    }
    SDL_UnlockSurface(src);
    SDL_UnlockSurface(terrain);
    SDL_FreeSurface(src);
    return terrain;
}


SDL_Surface* MapFunc::stylizeForDisplay(SDL_Surface* mapSurface,
                                        SDL_Surface* regionsMask, int scale) {
    if (!mapSurface) return nullptr;
    const int S = std::max(1, scale);

    static std::mutex stylizeMutex;
    std::lock_guard<std::mutex> stylizeLock(stylizeMutex);

    SDL_Surface* fillC = regionsMask ? reconstructFills(mapSurface, regionsMask) : nullptr;
    if (!fillC) fillC = SDL_ConvertSurfaceFormat(mapSurface, SDL_PIXELFORMAT_RGBA32, 0);
    if (!fillC) return nullptr;

    static SDL_Surface* cachedRegionFills = nullptr;
    static SDL_Surface* cachedMaskKey = nullptr;
    if (regionsMask != cachedMaskKey) {
        if (cachedRegionFills) SDL_FreeSurface(cachedRegionFills);
        cachedRegionFills = regionsMask ? reconstructFills(regionsMask, regionsMask) : nullptr;
        cachedMaskKey = regionsMask;
    }
    SDL_Surface* fillR = cachedRegionFills;

    const int bw = fillC->w;
    const int bh = fillC->h;

    SDL_Surface* out = SDL_CreateRGBSurfaceWithFormat(0, bw * S, bh * S, 32,
                                                      SDL_PIXELFORMAT_RGBA32);
    if (!out) {
        SDL_FreeSurface(fillC);
        return nullptr;
    }

    SDL_LockSurface(out);
    SDL_LockSurface(fillC);
    if (fillR) SDL_LockSurface(fillR);

    const Px kCountryBorder = {10, 10, 12, 255};
    const Px kCoast         = {13, 17, 23, 255};
    const Px kClear         = {0, 0, 0, 0};

    auto cls = [](Px f, Px n) -> int {
        if (isWaterPx(n) || isGreyPx(n)) return 2;
        return samePx(n, f) ? 0 : 1;
    };

    for (int by = 0; by < bh; by++) {
        Px* fc  = rowPx(fillC, by);
        Px* fcU = by > 0 ? rowPx(fillC, by - 1) : nullptr;
        Px* fcD = by + 1 < bh ? rowPx(fillC, by + 1) : nullptr;
        Px* fr  = fillR ? rowPx(fillR, by) : nullptr;
        Px* frD = (fillR && by + 1 < bh) ? rowPx(fillR, by + 1) : nullptr;

        for (int bx = 0; bx < bw; bx++) {
            Px f = fc[bx];

            if (isWaterPx(f) || isGreyPx(f)) {
                for (int sy = 0; sy < S; sy++) {
                    Px* seg = rowPx(out, by * S + sy) + bx * S;
                    for (int i = 0; i < S; i++) seg[i] = kClear;
                }
                continue;
            }

            int grey = (f.r + f.g + f.b) / 3;
            Px muted = {static_cast<uint8_t>(f.r + (grey - f.r) * 15 / 100),
                        static_cast<uint8_t>(f.g + (grey - f.g) * 15 / 100),
                        static_cast<uint8_t>(f.b + (grey - f.b) * 15 / 100), 255};

            int L = bx > 0 ? cls(f, fc[bx - 1]) : 0;
            int R = bx + 1 < bw ? cls(f, fc[bx + 1]) : 0;
            int T = fcU ? cls(f, fcU[bx]) : 0;
            int B = fcD ? cls(f, fcD[bx]) : 0;

            bool regR = false, regB = false;
            if (fr) {
                if (R == 0 && bx + 1 < bw && !samePx(fr[bx + 1], fr[bx])) regR = true;
                if (B == 0 && frD && !samePx(frD[bx], fr[bx])) regB = true;
            }
            Px regionCol = {static_cast<uint8_t>(muted.r * 2 / 3),
                            static_cast<uint8_t>(muted.g * 2 / 3),
                            static_cast<uint8_t>(muted.b * 2 / 3), 255};

            auto sideCol = [&](int c) -> Px { return c == 2 ? kCoast : kCountryBorder; };

            for (int sy = 0; sy < S; sy++) {
                Px* seg = rowPx(out, by * S + sy) + bx * S;
                if (sy == 0 && T) {
                    Px c = sideCol(T);
                    for (int i = 0; i < S; i++) seg[i] = c;
                    continue;
                }
                if (sy == S - 1 && (B || regB)) {
                    Px c = B ? sideCol(B) : regionCol;
                    for (int i = 0; i < S; i++) seg[i] = c;
                    continue;
                }
                for (int i = 0; i < S; i++) seg[i] = muted;
                if (L) seg[0] = sideCol(L);
                if (R) seg[S - 1] = sideCol(R);
                else if (regR) seg[S - 1] = regionCol;
            }
        }
    }

    if (fillR) SDL_UnlockSurface(fillR);
    SDL_UnlockSurface(fillC);
    SDL_UnlockSurface(out);
    SDL_FreeSurface(fillC);
    return out;
}


void MapFunc::expandFillToLines(SDL_Surface* overlay, SDL_Surface* mask) {
    if (!overlay || !mask) return;
    if (overlay->format->format != SDL_PIXELFORMAT_RGBA32 ||
        mask->format->format != SDL_PIXELFORMAT_RGBA32) return;

    const int w = std::min(overlay->w, mask->w);
    const int h = std::min(overlay->h, mask->h);

    SDL_LockSurface(overlay);
    SDL_LockSurface(mask);

    for (int y = 0; y < h; y++) {
        Px* orow = rowPx(overlay, y);
        Px* mrow = rowPx(mask, y);
        for (int x = 0; x < w; x++) {
            if (orow[x].a != 0) continue;
            if (!isLinePx(mrow[x])) continue;

            Px found = {0, 0, 0, 0};
            for (int dy = -1; dy <= 1 && !found.a; dy++) {
                int ny = y + dy;
                if (ny < 0 || ny >= h) continue;
                Px* orn = rowPx(overlay, ny);
                Px* mrn = rowPx(mask, ny);
                for (int dx = -1; dx <= 1; dx++) {
                    int nx = x + dx;
                    if (nx < 0 || nx >= w) continue;
                    if (isLinePx(mrn[nx])) continue;
                    if (orn[nx].a != 0) { found = orn[nx]; break; }
                }
            }
            if (found.a) orow[x] = found;
        }
    }

    SDL_UnlockSurface(mask);
    SDL_UnlockSurface(overlay);
}


SDL_Surface* MapFunc::fixBorders(SDL_Surface* mapSurface,
                                  const std::vector<Color>& toChange,
                                  const std::vector<Color>& toIgnore) {
    if (!mapSurface) return nullptr;


    SDL_Surface* result = SDL_ConvertSurface(mapSurface, mapSurface->format, 0);
    if (!result) return nullptr;

    SDL_LockSurface(result);
    SDL_LockSurface(mapSurface);

    int w = mapSurface->w;
    int h = mapSurface->h;

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            Color current = getPixel(mapSurface, x, y);


            bool shouldChange = false;
            for (const auto& tc : toChange) {
                if (current.r == tc.r && current.g == tc.g && current.b == tc.b) {
                    shouldChange = true;
                    break;
                }
            }
            if (!shouldChange) continue;


            std::map<uint32_t, std::pair<Color, int>> colorCounts;
            Color bestColor = current;
            int bestCount = 0;

            for (int dy = -1; dy <= 1; dy++) {
                for (int dx = -1; dx <= 1; dx++) {
                    if (dx == 0 && dy == 0) continue;
                    int nx = x + dx;
                    int ny = y + dy;
                    if (nx < 0 || ny < 0 || nx >= w || ny >= h) continue;

                    Color nc = getPixel(mapSurface, nx, ny);


                    bool skip = false;
                    for (const auto& tc : toChange) {
                        if (nc.r == tc.r && nc.g == tc.g && nc.b == tc.b) {
                            skip = true;
                            break;
                        }
                    }
                    if (skip) continue;


                    for (const auto& ti : toIgnore) {
                        if (nc.r == ti.r && nc.g == ti.g && nc.b == ti.b) {
                            skip = true;
                            break;
                        }
                    }
                    if (skip) continue;

                    uint32_t key = nc.toUint32();
                    colorCounts[key].first = nc;
                    colorCounts[key].second++;
                    if (colorCounts[key].second > bestCount) {
                        bestCount = colorCounts[key].second;
                        bestColor = nc;
                    }
                }
            }

            if (bestCount > 0) {
                setPixel(result, x, y, bestColor);
            }
        }
    }

    SDL_UnlockSurface(mapSurface);
    SDL_UnlockSurface(result);

    return result;
}
