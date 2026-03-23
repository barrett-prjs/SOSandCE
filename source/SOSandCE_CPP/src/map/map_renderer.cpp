#include "map/map_renderer.h"
#include "core/engine.h"
#include "data/region_data.h"
#include "ui/primitives.h"
#include "game/game_state.h"
#include "game/country.h"
#include "game/division.h"
#include "game/battle.h"
#include "game/fog_of_war.h"
#include "map/map_functions.h"
#include <unordered_set>
#include <cmath>

namespace {

float closestWrappedScreenX(const Camera& cam, float worldX, int screenW, float referenceX) {
    float bestX = cam.worldToScreenX(worldX, screenW);
    float bestDist = std::fabs(bestX - referenceX);

    const float mapW = cam.mapWidth;
    const float candidates[] = {
        cam.worldToScreenX(worldX + mapW, screenW),
        cam.worldToScreenX(worldX - mapW, screenW)
    };

    for (float candidate : candidates) {
        float dist = std::fabs(candidate - referenceX);
        if (dist < bestDist) {
            bestDist = dist;
            bestX = candidate;
        }
    }

    return bestX;
}

void drawThickLine(SDL_Renderer* r, float x1, float y1, float x2, float y2, float thickness, Color color) {
    float dx = x2 - x1;
    float dy = y2 - y1;
    float len = std::sqrt(dx * dx + dy * dy);
    if (len < 0.001f) return;

    float nx = -dy / len * (thickness * 0.5f);
    float ny = dx / len * (thickness * 0.5f);
    SDL_Color sdl = color.toSDL();

    SDL_Vertex verts[4] = {
        {{x1 + nx, y1 + ny}, sdl, {0.0f, 0.0f}},
        {{x1 - nx, y1 - ny}, sdl, {0.0f, 0.0f}},
        {{x2 - nx, y2 - ny}, sdl, {0.0f, 0.0f}},
        {{x2 + nx, y2 + ny}, sdl, {0.0f, 0.0f}}
    };
    const int indices[6] = {0, 1, 2, 0, 2, 3};
    SDL_RenderGeometry(r, nullptr, verts, 4, indices, 6);
}

bool rectVisible(float x, float y, float w, float h, int screenW, int screenH) {
    return !(x + w < 0.0f || x > (float)screenW || y + h < 0.0f || y > (float)screenH);
}

template <typename Fn>
void forEachWrappedScreenX(const Camera& cam, float worldX, int screenW, Fn&& fn) {
    fn(cam.worldToScreenX(worldX, screenW));
    fn(cam.worldToScreenX(worldX + cam.mapWidth, screenW));
    fn(cam.worldToScreenX(worldX - cam.mapWidth, screenW));
}

}


void Camera::pan(float dx, float dy) {
    x += dx / zoom;
    y += dy / zoom;


    x = normalizeX(x);
}

void Camera::zoomBy(float factor, float centerX, float centerY, int screenW, int screenH) {

    Vec2 worldBefore = screenToWorld(static_cast<int>(centerX), static_cast<int>(centerY), screenW, screenH);

    zoom *= factor;
    clampZoom();


    Vec2 worldAfter = screenToWorld(static_cast<int>(centerX), static_cast<int>(centerY), screenW, screenH);


    x += (worldAfter.x - worldBefore.x);
    y += (worldAfter.y - worldBefore.y);
}

void Camera::clampZoom(float minZoom, float maxZoom) {
    if (zoom < minZoom) zoom = minZoom;
    if (zoom > maxZoom) zoom = maxZoom;
}

float Camera::worldToScreenX(float wx, int screenW) const {
    return (wx + x) * zoom + screenW * 0.5f;
}

float Camera::worldToScreenY(float wy, int screenH) const {
    return (wy + y) * zoom + screenH * 0.5f;
}

Vec2 Camera::screenToWorld(int sx, int sy, int screenW, int screenH) const {
    Vec2 w;
    w.x = (sx - screenW * 0.5f) / zoom - x;
    w.y = (sy - screenH * 0.5f) / zoom - y;
    return w;
}

float Camera::normalizeX(float wx) const {

    while (wx > mapWidth * 0.5f) wx -= mapWidth;
    while (wx < -mapWidth * 0.5f) wx += mapWidth;
    return wx;
}


MapRenderer::MapRenderer() = default;

MapRenderer::~MapRenderer() {
    if (mapTexture_) SDL_DestroyTexture(mapTexture_);
}


void MapRenderer::renderWrapped(SDL_Renderer* r, SDL_Surface* mapSurface,
                                 const Camera& cam, int screenW, int screenH) {
    if (!mapSurface) return;

    int mw = mapSurface->w;
    int mh = mapSurface->h;


    if (!mapTexture_ || lastSurface_ != mapSurface || lastTextureW_ != mw || lastTextureH_ != mh) {
        if (mapTexture_) SDL_DestroyTexture(mapTexture_);

        SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
        mapTexture_ = SDL_CreateTextureFromSurface(r, mapSurface);
        lastTextureW_ = mw;
        lastTextureH_ = mh;
        lastSurface_ = mapSurface;
    }


    if (!mapTexture_) return;


    float visW = screenW / cam.zoom;
    float visH = screenH / cam.zoom;
    float visX = -cam.x - visW * 0.5f;
    float visY = -cam.y - visH * 0.5f;


    {
        float mapScreenX = cam.worldToScreenX(0, screenW);
        float mapScreenY = cam.worldToScreenY(0, screenH);
        float mapScreenW = mw * cam.zoom;
        float mapScreenH = mh * cam.zoom;

        SDL_Rect dst;
        dst.x = static_cast<int>(mapScreenX);
        dst.y = static_cast<int>(mapScreenY);
        dst.w = static_cast<int>(mapScreenW);
        dst.h = static_cast<int>(mapScreenH);

        SDL_RenderCopy(r, mapTexture_, nullptr, &dst);
    }


    {
        float mapScreenX = cam.worldToScreenX(static_cast<float>(mw), screenW);
        float mapScreenY = cam.worldToScreenY(0, screenH);
        float mapScreenW = mw * cam.zoom;
        float mapScreenH = mh * cam.zoom;

        SDL_Rect dst;
        dst.x = static_cast<int>(mapScreenX);
        dst.y = static_cast<int>(mapScreenY);
        dst.w = static_cast<int>(mapScreenW);
        dst.h = static_cast<int>(mapScreenH);

        SDL_RenderCopy(r, mapTexture_, nullptr, &dst);
    }


    {
        float mapScreenX = cam.worldToScreenX(static_cast<float>(-mw), screenW);
        float mapScreenY = cam.worldToScreenY(0, screenH);
        float mapScreenW = mw * cam.zoom;
        float mapScreenH = mh * cam.zoom;

        SDL_Rect dst;
        dst.x = static_cast<int>(mapScreenX);
        dst.y = static_cast<int>(mapScreenY);
        dst.w = static_cast<int>(mapScreenW);
        dst.h = static_cast<int>(mapScreenH);

        SDL_RenderCopy(r, mapTexture_, nullptr, &dst);
    }
}


void MapRenderer::renderMap(SDL_Renderer* r, SDL_Surface* mapSurface,
                             const Camera& cam, int screenW, int screenH) {
    renderWrapped(r, mapSurface, cam, screenW, screenH);
}


void MapRenderer::renderDivisions(SDL_Renderer* r, GameState& gs,
                                   const Camera& cam, int screenW, int screenH,
                                   const std::vector<Division*>* highlightedDivisions) {

    std::unordered_map<int, int> divsPerRegion;
    std::unordered_map<int, int> regionDrawCount;
    std::unordered_set<const Division*> highlighted;
    if (highlightedDivisions) {
        highlighted.insert(highlightedDivisions->begin(), highlightedDivisions->end());
    }

    for (auto& [name, country] : gs.countries) {
        if (!country) continue;
        for (auto& div : country->divisions) {
            if (div) divsPerRegion[div->region]++;
        }
    }

    for (auto& [name, country] : gs.countries) {
        if (!country) continue;
        for (auto& div : country->divisions) {
            if (!div) continue;


            int totalInRegion = divsPerRegion[div->region];
            int myIndex = regionDrawCount[div->region]++;
            float stackOffset = div->spriteH * myIndex - div->spriteH * totalInRegion / 2.0f;

            float sy = cam.worldToScreenY(div->yBlit, screenH) + stackOffset;

            int w = div->spriteW;
            int h = div->spriteH;
            if (w <= 0 || h <= 0) { w = 40; h = 28; }

            auto drawMovePath = [&](float anchorX) {
                if (div->commands.empty() || div->fighting) return;

                auto& rd = RegionData::instance();
                Vec2 startLoc = rd.getLocation(div->region);
                float prevX = closestWrappedScreenX(cam, startLoc.x, screenW, anchorX);
                float prevY = cam.worldToScreenY(startLoc.y, screenH);
                bool activeSegment = true;
                float lineW = std::max(2.0f, Engine::instance().uiScale * 0.18f);

                SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
                for (int regionId : div->commands) {
                    Vec2 targetLoc = rd.getLocation(regionId);
                    float tx = closestWrappedScreenX(cam, targetLoc.x, screenW, prevX);
                    float ty = cam.worldToScreenY(targetLoc.y, screenH);
                    Color segmentColor = activeSegment
                        ? Color{210, 48, 48, 220}
                        : Color{118, 124, 136, 160};
                    float segmentWidth = activeSegment ? lineW : std::max(1.5f, lineW * 0.8f);
                    drawThickLine(r, prevX, prevY, tx, ty, segmentWidth, segmentColor);

                    prevX = tx;
                    prevY = ty;
                    activeSegment = false;
                }
            };

            auto drawDiv = [&](float drawX) {
                SDL_Rect dst;
                dst.x = static_cast<int>(drawX - w * 0.5f);
                dst.y = static_cast<int>(sy);
                dst.w = w;
                dst.h = h;

                if (!rectVisible((float)dst.x, (float)dst.y, (float)dst.w, (float)dst.h, screenW, screenH))
                    return;

                drawMovePath(drawX);

                if (div->sprite) {
                    SDL_RenderCopy(r, div->sprite, nullptr, &dst);
                } else {
                    auto* divCountry = gs.getCountry(div->country);
                    Color col = divCountry ? divCountry->color : Color{128, 128, 128};
                    SDL_SetRenderDrawColor(r, col.r, col.g, col.b, 255);
                    SDL_RenderFillRect(r, &dst);
                }

                if (highlighted.count(div.get()) > 0) {
                    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
                    SDL_SetRenderDrawColor(r, 255, 235, 120, 215);
                    SDL_Rect hl = {dst.x - 2, dst.y - 2, dst.w + 4, dst.h + 4};
                    SDL_RenderDrawRect(r, &hl);
                }

                if (div->divisionStack >= 1) {
                    int countFs = std::max(7, std::min(dst.h * 2 / 3, 14));
                    UIPrim::drawText(r, std::to_string(div->divisionStack), countFs,
                                     (float)(dst.x + dst.w) - countFs * 0.3f,
                                     (float)(dst.y + dst.h * 0.3f),
                                     "center", {255, 255, 255}, true);
                }
            };

            forEachWrappedScreenX(cam, div->xBlit, screenW, drawDiv);
        }
    }
}


void MapRenderer::renderBattles(SDL_Renderer* r, GameState& gs,
                                 const Camera& cam, int screenW, int screenH) {
    for (auto& battle : gs.battles) {
        if (!battle) continue;

        float sy = cam.worldToScreenY(battle->location.y, screenH);
        forEachWrappedScreenX(cam, battle->location.x, screenW, [&](float sx) {
            if (sx < -100.0f || sx > screenW + 100.0f || sy < -100.0f || sy > screenH + 100.0f) return;

            if (battle->image) {
                SDL_Rect dst;
                dst.x = static_cast<int>(sx - battle->imageW * 0.5f);
                dst.y = static_cast<int>(sy - battle->imageH * 0.5f);
                dst.w = battle->imageW;
                dst.h = battle->imageH;
                SDL_RenderCopy(r, battle->image, nullptr, &dst);
            } else {
                int size = static_cast<int>(8 * cam.zoom);
                if (size < 4) size = 4;
                if (size > 32) size = 32;

                SDL_SetRenderDrawColor(r, 200, 50, 50, 220);
                SDL_Rect marker = {
                    static_cast<int>(sx - size / 2),
                    static_cast<int>(sy - size / 2),
                    size, size
                };
                SDL_RenderFillRect(r, &marker);

                SDL_SetRenderDrawColor(r, 255, 255, 255, 200);
                SDL_RenderDrawLine(r, marker.x, marker.y,
                                   marker.x + size, marker.y + size);
                SDL_RenderDrawLine(r, marker.x + size, marker.y,
                                   marker.x, marker.y + size);
            }
        });
    }
}


static void drawStar(SDL_Renderer* r, float cx, float cy, float outerR, float innerR,
                     uint8_t cr, uint8_t cg, uint8_t cb, uint8_t ca) {

    constexpr int points = 5;
    constexpr float pi = 3.14159265f;
    SDL_Vertex verts[12];
    SDL_FPoint center = {cx, cy};


    verts[0].position = center;
    verts[0].color = {cr, cg, cb, ca};
    verts[0].tex_coord = {0, 0};

    for (int i = 0; i < 10; i++) {
        float angle = -pi / 2.0f + i * pi / 5.0f;
        float radius = (i % 2 == 0) ? outerR : innerR;
        verts[i + 1].position = {cx + cosf(angle) * radius, cy + sinf(angle) * radius};
        verts[i + 1].color = {cr, cg, cb, ca};
        verts[i + 1].tex_coord = {0, 0};
    }

    verts[11] = verts[1];


    int indices[30];
    for (int i = 0; i < 10; i++) {
        indices[i * 3]     = 0;
        indices[i * 3 + 1] = i + 1;
        indices[i * 3 + 2] = i + 2;
    }
    SDL_RenderGeometry(r, nullptr, verts, 12, indices, 30);
}


void MapRenderer::renderCities(SDL_Renderer* r, GameState& gs,
                                const Camera& cam, int screenW, int screenH,
                                bool onlyCities) {
    auto& regData = RegionData::instance();
    const auto& cities = regData.getCities();


    std::unordered_set<std::string> capitalCities;
    for (auto& [name, country] : gs.countries) {
        if (!country->capital.empty()) {
            capitalCities.insert(country->capital);
        }
    }

    for (auto& [cityName, info] : cities) {
        Vec2 loc = regData.getLocation(info.region);
        float sy = cam.worldToScreenY(loc.y, screenH);

        bool isCapital = capitalCities.count(cityName) > 0;
        forEachWrappedScreenX(cam, loc.x, screenW, [&](float sx) {
            if (sx < -50.0f || sx > screenW + 50.0f || sy < -50.0f || sy > screenH + 50.0f) return;

            if (isCapital) {
                float starOuter = std::max(4.0f, std::min(12.0f, 6.0f * cam.zoom));
                float starInner = starOuter * 0.4f;
                drawStar(r, sx, sy, starOuter + 1.5f, starInner + 0.8f, 20, 20, 20, 255);
                drawStar(r, sx, sy, starOuter, starInner, 230, 210, 140, 255);
            } else {
                int dotSize = std::max(2, static_cast<int>(3 * cam.zoom));
                if (dotSize > 8) dotSize = 8;

                SDL_SetRenderDrawColor(r, 20, 20, 20, 255);
                SDL_Rect dot = {
                    static_cast<int>(sx - dotSize / 2) - 1,
                    static_cast<int>(sy - dotSize / 2) - 1,
                    dotSize + 2, dotSize + 2
                };
                SDL_RenderFillRect(r, &dot);

                SDL_SetRenderDrawColor(r, 210, 200, 172, 255);
                dot = {
                    static_cast<int>(sx - dotSize / 2),
                    static_cast<int>(sy - dotSize / 2),
                    dotSize, dotSize
                };
                SDL_RenderFillRect(r, &dot);
            }
        });
    }

    if (onlyCities) return;


    for (int portRegion : gs.ports) {
        Vec2 loc = regData.getLocation(portRegion);
        float sy = cam.worldToScreenY(loc.y, screenH);
        forEachWrappedScreenX(cam, loc.x, screenW, [&](float sx) {
            if (sx < -50.0f || sx > screenW + 50.0f || sy < -50.0f || sy > screenH + 50.0f) return;

            int size = std::max(3, static_cast<int>(4 * cam.zoom));
            if (size > 10) size = 10;

            SDL_SetRenderDrawColor(r, 40, 100, 180, 220);
            SDL_Rect anchor = {
                static_cast<int>(sx - size / 2),
                static_cast<int>(sy - size / 2 + size),
                size, size
            };
            SDL_RenderFillRect(r, &anchor);
        });
    }


    for (int canalRegion : gs.canals) {
        Vec2 loc = regData.getLocation(canalRegion);
        float sy = cam.worldToScreenY(loc.y, screenH);
        forEachWrappedScreenX(cam, loc.x, screenW, [&](float sx) {
            if (sx < -50.0f || sx > screenW + 50.0f || sy < -50.0f || sy > screenH + 50.0f) return;

            int size = std::max(3, static_cast<int>(4 * cam.zoom));
            if (size > 10) size = 10;

            SDL_SetRenderDrawColor(r, 140, 100, 50, 220);
            SDL_Rect canal = {
                static_cast<int>(sx - size / 2),
                static_cast<int>(sy - size / 2 + size + 4),
                size, size / 2
            };
            SDL_RenderFillRect(r, &canal);
        });
    }
}


void MapRenderer::renderCommands(SDL_Renderer* r, GameState& gs,
                                  const Camera& cam, int screenW, int screenH,
                                  const std::vector<int>& selectedRegions,
                                  const std::vector<Division*>& selectedDivisions) {
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);

    if (!selectedRegions.empty() && gs.regionsMapSurf) {
        SDL_Surface* overlay = SDL_CreateRGBSurfaceWithFormat(0, gs.regionsMapSurf->w, gs.regionsMapSurf->h, 32, SDL_PIXELFORMAT_RGBA32);
        if (overlay) {
            SDL_FillRect(overlay, nullptr, SDL_MapRGBA(overlay->format, 0, 0, 0, 0));

            auto& rd = RegionData::instance();
            for (int regId : selectedRegions) {
                if (regId <= 0) continue;
                Vec2 loc = rd.getLocation(regId);
                MapFunc::fillRegionMask(overlay, gs.regionsMapSurf, regId, loc.x, loc.y, {175, 40, 40, 110});
            }

            SDL_Texture* overlayTex = SDL_CreateTextureFromSurface(r, overlay);
            if (overlayTex) {
                SDL_SetTextureBlendMode(overlayTex, SDL_BLENDMODE_BLEND);
                auto drawOverlayCopy = [&](float offsetX) {
                    SDL_Rect dst;
                    dst.x = static_cast<int>(cam.worldToScreenX(offsetX, screenW));
                    dst.y = static_cast<int>(cam.worldToScreenY(0.0f, screenH));
                    dst.w = static_cast<int>(overlay->w * cam.zoom);
                    dst.h = static_cast<int>(overlay->h * cam.zoom);
                    SDL_RenderCopy(r, overlayTex, nullptr, &dst);
                };
                drawOverlayCopy(0.0f);
                drawOverlayCopy((float)overlay->w);
                drawOverlayCopy((float)-overlay->w);
                SDL_DestroyTexture(overlayTex);
            }

            SDL_FreeSurface(overlay);
        }
    }

    auto& rd = RegionData::instance();
    float chainThickness = std::max(2.0f, Engine::instance().uiScale * 0.16f);
    if (selectedRegions.size() >= 2) {
        float anchorX = 0.0f;
        bool hasAnchor = false;
        if (!selectedDivisions.empty()) {
            float sumWorldX = 0.0f;
            int count = 0;
            for (auto* div : selectedDivisions) {
                if (!div) continue;
                sumWorldX += div->xBlit;
                count++;
            }
            if (count > 0) {
                anchorX = cam.worldToScreenX(sumWorldX / static_cast<float>(count), screenW);
                hasAnchor = true;
            }
        }

        float prevX = 0.0f;
        float prevY = 0.0f;
        bool hasPrev = false;
        for (int regId : selectedRegions) {
            Vec2 loc = rd.getLocation(regId);
            float targetY = cam.worldToScreenY(loc.y, screenH);
            float targetX = hasPrev
                ? closestWrappedScreenX(cam, loc.x, screenW, prevX)
                : (hasAnchor ? closestWrappedScreenX(cam, loc.x, screenW, anchorX)
                             : cam.worldToScreenX(loc.x, screenW));

            if (hasPrev) {
                drawThickLine(r, prevX, prevY, targetX, targetY, chainThickness, {220, 40, 40, 230});
            }

            prevX = targetX;
            prevY = targetY;
            hasPrev = true;
        }
    }

    if (selectedRegions.size() == 1 && !selectedDivisions.empty()) {
        int targetRegion = selectedRegions.front();
        Vec2 targetLoc = rd.getLocation(targetRegion);
        std::unordered_set<int> drawnOrigins;

        for (auto* div : selectedDivisions) {
            if (!div || div->region <= 0 || !drawnOrigins.insert(div->region).second) continue;

            Vec2 originLoc = rd.getLocation(div->region);
            float spriteAnchorX = cam.worldToScreenX(div->xBlit, screenW);
            float startX = closestWrappedScreenX(cam, originLoc.x, screenW, spriteAnchorX);
            float startY = cam.worldToScreenY(originLoc.y, screenH);
            float targetX = closestWrappedScreenX(cam, targetLoc.x, screenW, startX);
            float targetY = cam.worldToScreenY(targetLoc.y, screenH);

            drawThickLine(r, startX, startY, targetX, targetY, chainThickness, {220, 40, 40, 230});
        }
    }

    if (selectedRegions.empty()) return;
}


void MapRenderer::renderFogOverlay(SDL_Renderer* r, FogOfWar& fog,
                                    const Camera& cam, int screenW, int screenH) {
    if (!fog.enabled) return;

    SDL_Surface* overlay = fog.getOverlay();
    if (!overlay) return;

    SDL_Texture* fogTex = SDL_CreateTextureFromSurface(r, overlay);
    if (!fogTex) return;

    SDL_SetTextureBlendMode(fogTex, SDL_BLENDMODE_BLEND);
    SDL_SetTextureAlphaMod(fogTex, 140);


    int mw = overlay->w;
    int mh = overlay->h;

    auto renderCopy = [&](float offsetX) {
        float mapScreenX = cam.worldToScreenX(offsetX, screenW);
        float mapScreenY = cam.worldToScreenY(0, screenH);
        float mapScreenW = mw * cam.zoom;
        float mapScreenH = mh * cam.zoom;

        SDL_Rect dst;
        dst.x = static_cast<int>(mapScreenX);
        dst.y = static_cast<int>(mapScreenY);
        dst.w = static_cast<int>(mapScreenW);
        dst.h = static_cast<int>(mapScreenH);
        SDL_RenderCopy(r, fogTex, nullptr, &dst);
    };

    renderCopy(0);
    renderCopy(static_cast<float>(mw));
    renderCopy(static_cast<float>(-mw));

    SDL_DestroyTexture(fogTex);
}


int MapRenderer::regionAtScreen(int sx, int sy, const Camera& cam,
                                 int screenW, int screenH,
                                 SDL_Surface* regionsMap) {
    if (!regionsMap) return -1;

    Vec2 world = cam.screenToWorld(sx, sy, screenW, screenH);


    int mapW = regionsMap->w;
    int mapH = regionsMap->h;

    int wx = static_cast<int>(world.x);
    int wy = static_cast<int>(world.y);


    while (wx < 0) wx += mapW;
    while (wx >= mapW) wx -= mapW;

    if (wy < 0 || wy >= mapH) return -1;

    SDL_LockSurface(regionsMap);
    Color c = getPixel(regionsMap, wx, wy);
    SDL_UnlockSurface(regionsMap);

    return RegionData::instance().getRegion(c);
}
