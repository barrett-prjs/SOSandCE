#include "map/map_renderer.h"
#include "core/engine.h"
#include "data/region_data.h"
#include "ui/primitives.h"
#include "ui/ui_assets.h"
#include "game/game_state.h"
#include "game/country.h"
#include "game/division.h"
#include "game/battle.h"
#include "game/puppet.h"
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

SDL_Texture* createStaticRGBA(SDL_Renderer* r, SDL_Surface* s) {
    if (!s || s->format->format != SDL_PIXELFORMAT_RGBA32) return nullptr;
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
    SDL_Texture* t = SDL_CreateTexture(r, SDL_PIXELFORMAT_RGBA32,
                                       SDL_TEXTUREACCESS_STATIC, s->w, s->h);
    if (!t) return nullptr;
    if (SDL_UpdateTexture(t, nullptr, s->pixels, s->pitch) != 0) {
        SDL_DestroyTexture(t);
        return nullptr;
    }
    return t;
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
    if (styleJob_.valid()) {
        SDL_Surface* leftover = styleJob_.get();
        if (leftover) SDL_FreeSurface(leftover);
    }
    if (styleSnapshot_) SDL_FreeSurface(styleSnapshot_);
    if (pendingDisplay_) SDL_FreeSurface(pendingDisplay_);
    if (mapTexture_) SDL_DestroyTexture(mapTexture_);
    if (terrainTexture_) SDL_DestroyTexture(terrainTexture_);
    if (nightTex_) SDL_DestroyTexture(nightTex_);
    if (selOverlayTex_) SDL_DestroyTexture(selOverlayTex_);
    if (hlTex_) SDL_DestroyTexture(hlTex_);
    if (pupTex_) SDL_DestroyTexture(pupTex_);
}


void MapRenderer::renderWrapped(SDL_Renderer* r, SDL_Surface* mapSurface,
                                 const Camera& cam, int screenW, int screenH,
                                 SDL_Surface* terrainSurface) {
    if (!mapSurface) return;

    int mw = mapSurface->w;
    int mh = mapSurface->h;


    if (terrainSurface && (!terrainTexture_ || lastTerrainSurface_ != terrainSurface)) {
        if (terrainTexture_) SDL_DestroyTexture(terrainTexture_);
        SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
        terrainTexture_ = SDL_CreateTextureFromSurface(r, terrainSurface);
        lastTerrainSurface_ = terrainSurface;
    }


    auto buildDisplayTexture = [&](SDL_Surface* source) -> SDL_Texture* {
        SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
        SDL_Texture* tex = nullptr;
        if (terrainSurface) {
            SDL_Surface* display = MapFunc::stylizeForDisplay(source, styleMask_, 4);
            if (display) {
                tex = createStaticRGBA(r, display);
                SDL_FreeSurface(display);
                if (!tex) {
                    display = MapFunc::stylizeForDisplay(source, styleMask_, 1);
                    if (display) {
                        tex = createStaticRGBA(r, display);
                        SDL_FreeSurface(display);
                    }
                }
            }
        }
        if (!tex) {
            SDL_Surface* conv = SDL_ConvertSurfaceFormat(source, SDL_PIXELFORMAT_RGBA32, 0);
            if (conv) {
                tex = createStaticRGBA(r, conv);
                SDL_FreeSurface(conv);
            }
            if (!tex) tex = SDL_CreateTextureFromSurface(r, source);
        }
        if (tex) SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
        return tex;
    };

    bool structural = !mapTexture_ || lastSurface_ != mapSurface ||
                      lastTextureW_ != mw || lastTextureH_ != mh;
    if (structural) {
        if (styleJob_.valid()) {
            SDL_Surface* stale = styleJob_.get();
            if (stale) SDL_FreeSurface(stale);
        }
        if (styleSnapshot_) { SDL_FreeSurface(styleSnapshot_); styleSnapshot_ = nullptr; }
        if (pendingDisplay_) { SDL_FreeSurface(pendingDisplay_); pendingDisplay_ = nullptr; }

        if (mapTexture_) SDL_DestroyTexture(mapTexture_);
        mapTexture_ = buildDisplayTexture(mapSurface);
        mapTextureDirty_ = false;
        lastStyleBuildTicks_ = SDL_GetTicks();
        lastTextureW_ = mw;
        lastTextureH_ = mh;
        lastSurface_ = mapSurface;
    } else if (mapTextureDirty_ && !styleJob_.valid() && !pendingDisplay_ && terrainSurface &&
               SDL_GetTicks() - lastStyleBuildTicks_ >= 250) {
        styleSnapshot_ = SDL_ConvertSurfaceFormat(mapSurface, SDL_PIXELFORMAT_RGBA32, 0);
        if (styleSnapshot_) {
            SDL_Surface* snap = styleSnapshot_;
            SDL_Surface* maskCopy = styleMask_
                ? SDL_ConvertSurfaceFormat(styleMask_, SDL_PIXELFORMAT_RGBA32, 0)
                : nullptr;
            styleJob_ = std::async(std::launch::async, [snap, maskCopy]() {
                SDL_Surface* out = MapFunc::stylizeForDisplay(snap, maskCopy, 4);
                if (maskCopy) SDL_FreeSurface(maskCopy);
                return out;
            });
            mapTextureDirty_ = false;
            lastStyleBuildTicks_ = SDL_GetTicks();
        } else {
            mapTextureDirty_ = false;
        }
    } else if (mapTextureDirty_ && !terrainSurface) {
        if (mapTexture_) SDL_DestroyTexture(mapTexture_);
        mapTexture_ = buildDisplayTexture(mapSurface);
        mapTextureDirty_ = false;
    }

    if (styleJob_.valid() &&
        styleJob_.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
        SDL_Surface* display = styleJob_.get();
        if (styleSnapshot_) { SDL_FreeSurface(styleSnapshot_); styleSnapshot_ = nullptr; }
        if (display) {
            int tw = 0, th = 0;
            if (mapTexture_) SDL_QueryTexture(mapTexture_, nullptr, nullptr, &tw, &th);
            if (mapTexture_ && tw == display->w && th == display->h) {
                pendingDisplay_ = display;
                pendingRow_ = 0;
            } else {
                SDL_Texture* tex = createStaticRGBA(r, display);
                SDL_FreeSurface(display);
                if (tex) {
                    SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
                    if (mapTexture_) SDL_DestroyTexture(mapTexture_);
                    mapTexture_ = tex;
                }
            }
        }
    }

    if (pendingDisplay_ && mapTexture_) {
        int bandH = std::min(128, pendingDisplay_->h - pendingRow_);
        SDL_Rect band = {0, pendingRow_, pendingDisplay_->w, bandH};
        const uint8_t* src = static_cast<const uint8_t*>(pendingDisplay_->pixels) +
                             static_cast<size_t>(pendingRow_) * pendingDisplay_->pitch;
        SDL_UpdateTexture(mapTexture_, &band, src, pendingDisplay_->pitch);
        pendingRow_ += bandH;
        if (pendingRow_ >= pendingDisplay_->h) {
            SDL_FreeSurface(pendingDisplay_);
            pendingDisplay_ = nullptr;
            pendingRow_ = 0;
        }
    }


    if (!mapTexture_) return;


    int x0 = static_cast<int>(std::lround(cam.worldToScreenX(0, screenW)));
    int y0 = static_cast<int>(std::lround(cam.worldToScreenY(0, screenH)));
    int drawW = std::max(1, static_cast<int>(std::lround(mw * cam.zoom)));
    int drawH = std::max(1, static_cast<int>(std::lround(mh * cam.zoom)));
    SDL_Rect dsts[3] = {
        {x0, y0, drawW, drawH},
        {x0 + drawW, y0, drawW, drawH},
        {x0 - drawW, y0, drawW, drawH}
    };


    if (terrainTexture_) {
        float t = std::clamp((cam.zoom - 1.5f) / 4.5f, 0.0f, 1.0f);
        SDL_SetTextureAlphaMod(mapTexture_, static_cast<Uint8>(245 - t * 85));
        for (auto& dst : dsts) {
            if (dst.x + dst.w < 0 || dst.x > screenW) continue;
            SDL_RenderCopy(r, terrainTexture_, nullptr, &dst);
        }
    } else {
        SDL_SetTextureAlphaMod(mapTexture_, 255);
    }

    for (auto& dst : dsts) {
        if (dst.x + dst.w < 0 || dst.x > screenW) continue;
        SDL_RenderCopy(r, mapTexture_, nullptr, &dst);
    }

    if (clockHours_ >= 0.0f && terrainTexture_) {
        const int NW = 256, NH = 128;
        if (!nightTex_) {
            SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
            nightTex_ = SDL_CreateTexture(r, SDL_PIXELFORMAT_RGBA32,
                                          SDL_TEXTUREACCESS_STREAMING, NW, NH);
            if (nightTex_) SDL_SetTextureBlendMode(nightTex_, SDL_BLENDMODE_BLEND);
        }
        uint32_t nowTicks = SDL_GetTicks();
        if (nightTex_ &&
            (std::fabs(clockHours_ - nightTexHours_) > 0.02f ||
             std::fabs(clockAnchorX_ - nightTexAnchor_) > 1.0f) &&
            (nightTexHours_ < -100.0f || nowTicks - nightTexBuiltTicks_ >= 50)) {
            nightTexHours_ = clockHours_;
            nightTexAnchor_ = clockAnchorX_;
            nightTexBuiltTicks_ = nowTicks;
            static std::vector<uint8_t> nightPx;
            nightPx.resize(NW * NH * 4);
            const float pi = 3.14159265f;
            float sunX = clockAnchorX_ + (12.0f - clockHours_) / 24.0f * mw;
            float decl = -23.44f * (pi / 180.0f) *
                         std::cos(2.0f * pi * (clockDayOfYear_ + 10) / 365.24f);
            float sinD = std::sin(decl), cosD = std::cos(decl);
            float rowA[NH], rowB[NH], colC[NW];
            for (int ty = 0; ty < NH; ty++) {
                float lat = (0.5f - (ty + 0.5f) / NH) * pi;
                rowA[ty] = std::sin(lat) * sinD;
                rowB[ty] = std::cos(lat) * cosD;
            }
            for (int tx = 0; tx < NW; tx++) {
                float worldX = (tx + 0.5f) / NW * mw;
                colC[tx] = std::cos(2.0f * pi * (worldX - sunX) / mw);
            }
            for (int ty = 0; ty < NH; ty++) {
                for (int tx = 0; tx < NW; tx++) {
                    float sinAlt = rowA[ty] + rowB[ty] * colC[tx];
                    float nightT = std::clamp((0.03f - sinAlt) / 0.24f, 0.0f, 1.0f);
                    int i = (ty * NW + tx) * 4;
                    nightPx[i] = 7;
                    nightPx[i + 1] = 11;
                    nightPx[i + 2] = 27;
                    nightPx[i + 3] = static_cast<uint8_t>(nightT * 118.0f);
                }
            }
            SDL_UpdateTexture(nightTex_, nullptr, nightPx.data(), NW * 4);
        }
        if (nightTex_) {
            for (auto& dst : dsts) {
                if (dst.x + dst.w < 0 || dst.x > screenW) continue;
                SDL_RenderCopy(r, nightTex_, nullptr, &dst);
            }
        }
    }
}

void MapRenderer::setClock(float hours, float anchorWorldX, int month, int day) {
    clockHours_ = hours;
    clockAnchorX_ = anchorWorldX;
    static const int cumDays[12] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
    int m = std::clamp(month - 1, 0, 11);
    clockDayOfYear_ = cumDays[m] + std::clamp(day, 1, 31);
}


void MapRenderer::renderMap(SDL_Renderer* r, SDL_Surface* mapSurface,
                             const Camera& cam, int screenW, int screenH,
                             SDL_Surface* terrainSurface) {
    renderWrapped(r, mapSurface, cam, screenW, screenH, terrainSurface);
}


void MapRenderer::renderDivisions(SDL_Renderer* r, GameState& gs,
                                   const Camera& cam, int screenW, int screenH,
                                   const std::vector<Division*>* highlightedDivisions) {
    static Uint32 lastSmoothTicks = 0;
    Uint32 nowTicks = SDL_GetTicks();
    float smoothDt = lastSmoothTicks ? std::min(0.1f, (nowTicks - lastSmoothTicks) / 1000.0f) : 0.016f;
    lastSmoothTicks = nowTicks;
    float smoothK = std::min(1.0f, smoothDt * 7.0f);
    const float mapWf = cam.mapWidth;

    std::unordered_map<int, int> divsPerRegion;
    std::unordered_map<int, int> regionDrawCount;
    std::unordered_set<const Division*> highlighted;
    if (highlightedDivisions) {
        highlighted.insert(highlightedDivisions->begin(), highlightedDivisions->end());
    }

    const Color relAlly = {60, 110, 220};
    const Color relEnemy = {210, 55, 45};
    std::unordered_map<std::string, int> relationCache;
    Country* playerCountry = gs.controlledCountry.empty() ? nullptr : gs.getCountry(gs.controlledCountry);
    auto relationFor = [&](const std::string& divCountry) -> int {
        if (!playerCountry || divCountry == playerCountry->name) return 0;
        auto it = relationCache.find(divCountry);
        if (it != relationCache.end()) return it->second;
        int rel = 0;
        if (std::find(playerCountry->atWarWith.begin(), playerCountry->atWarWith.end(), divCountry) !=
            playerCountry->atWarWith.end()) {
            rel = 2;
        } else if (!playerCountry->faction.empty()) {
            Country* other = gs.getCountry(divCountry);
            if (other && other->faction == playerCountry->faction) rel = 1;
        }
        relationCache[divCountry] = rel;
        return rel;
    };

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

            if (div->dispX < 0.0f) {
                div->dispX = div->xBlit;
                div->dispY = div->yBlit;
            } else {
                float ddx = div->xBlit - div->dispX;
                if (mapWf > 0.0f && std::fabs(ddx) > mapWf * 0.5f) {
                    ddx += (ddx > 0.0f) ? -mapWf : mapWf;
                }
                float ddy = div->yBlit - div->dispY;
                if (std::fabs(ddx) > 90.0f || std::fabs(ddy) > 90.0f) {
                    div->dispX = div->xBlit;
                    div->dispY = div->yBlit;
                } else {
                    div->dispX += ddx * smoothK;
                    div->dispY += ddy * smoothK;
                    if (mapWf > 0.0f) {
                        while (div->dispX < 0.0f) div->dispX += mapWf;
                        while (div->dispX >= mapWf) div->dispX -= mapWf;
                    }
                }
            }

            int totalInRegion = divsPerRegion[div->region];
            int myIndex = regionDrawCount[div->region]++;
            float stackOffset = div->spriteH * myIndex - div->spriteH * totalInRegion / 2.0f;

            float sy = cam.worldToScreenY(div->dispY, screenH) + stackOffset;

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

                    int rel = relationFor(div->country);
                    if (rel != 0) {
                        Color rc2 = (rel == 2) ? relEnemy : relAlly;
                        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
                        SDL_SetRenderDrawColor(r, rc2.r, rc2.g, rc2.b, 255);
                        for (int t = 0; t < std::max(2, div->borderPx); t++) {
                            SDL_Rect fr = {dst.x + t, dst.y + t, dst.w - t * 2, dst.h - t * 2};
                            SDL_RenderDrawRect(r, &fr);
                        }
                    }

                    int innerX = dst.x + div->borderPx;
                    int innerW = dst.w - div->borderPx * 2;
                    int zoneY = dst.y + dst.h - div->borderPx - div->barZoneH;
                    int hBar = std::max(2, div->barZoneH / 2);
                    float healthPct = div->maxUnits > 0.0f ? std::clamp(div->units / div->maxUnits, 0.0f, 1.0f) : 0.0f;
                    float resPct = div->maxResources > 0.0f ? std::clamp(div->resources / div->maxResources, 0.0f, 1.0f) : 0.0f;

                    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
                    SDL_SetRenderDrawColor(r, 52, 190, 82, 255);
                    SDL_Rect hb = {innerX, zoneY, (int)(innerW * healthPct), hBar};
                    SDL_RenderFillRect(r, &hb);
                    SDL_SetRenderDrawColor(r, 208, 165, 66, 255);
                    SDL_Rect rb = {innerX, zoneY + hBar, (int)(innerW * resPct),
                                   std::max(1, div->barZoneH - hBar)};
                    SDL_RenderFillRect(r, &rb);

                    int numAreaX = dst.x + div->flagAreaW;
                    int numAreaW = dst.w - div->flagAreaW - div->borderPx;
                    int flagRowH = dst.h - div->barZoneH - div->borderPx * 2;
                    std::string countStr = std::to_string(div->divisionStack);
                    int numFs = std::max(9, (int)(flagRowH * 0.85f));
                    while (numFs > 8 && UIPrim::textWidth(countStr, numFs) > numAreaW - 2) {
                        numFs--;
                    }
                    UIPrim::drawText(r, countStr, numFs,
                                     (float)numAreaX + numAreaW * 0.5f,
                                     (float)dst.y + div->borderPx + flagRowH * 0.5f,
                                     "center", {255, 255, 255}, true);
                } else {
                    auto* divCountry = gs.getCountry(div->country);
                    Color col = divCountry ? divCountry->color : Color{128, 128, 128};
                    SDL_SetRenderDrawColor(r, col.r, col.g, col.b, 255);
                    SDL_RenderFillRect(r, &dst);
                    int countFs = std::max(7, std::min(dst.h * 2 / 3, 14));
                    UIPrim::drawText(r, std::to_string(div->divisionStack), countFs,
                                     (float)(dst.x + dst.w * 0.5f),
                                     (float)(dst.y + dst.h * 0.5f),
                                     "center", {255, 255, 255}, true);
                }

                if (highlighted.count(div.get()) > 0) {
                    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
                    SDL_SetRenderDrawColor(r, 70, 220, 110, 230);
                    SDL_Rect hl = {dst.x - 2, dst.y - 2, dst.w + 4, dst.h + 4};
                    SDL_RenderDrawRect(r, &hl);
                    SDL_Rect hl2 = {dst.x - 3, dst.y - 3, dst.w + 6, dst.h + 6};
                    SDL_RenderDrawRect(r, &hl2);
                }
            };

            forEachWrappedScreenX(cam, div->dispX, screenW, drawDiv);
        }
    }
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
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


    std::unordered_map<std::string, std::string> capitalOf;
    for (auto& [name, country] : gs.countries) {
        if (country && !country->capital.empty()) {
            capitalOf[country->capital] = name;
        }
    }

    Country* pc = gs.controlledCountry.empty() ? nullptr : gs.getCountry(gs.controlledCountry);
    const Color cOwn   = {82, 128, 78, 255};
    const Color cOther = {150, 148, 140, 255};
    const Color cEnemy = {196, 68, 55, 255};
    auto relationColor = [&](const std::string& owner) -> Color {
        if (!pc || owner.empty() || owner == "Error") return cOther;
        if (owner == pc->name) return cOwn;
        if (std::find(pc->atWarWith.begin(), pc->atWarWith.end(), owner) != pc->atWarWith.end())
            return cEnemy;
        return cOther;
    };

    auto fillCircleAt = [&](float cx, float cy, float rad, Color c) {
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
        int ir = static_cast<int>(std::ceil(rad));
        for (int dy = -ir; dy <= ir; dy++) {
            float span = rad * rad - static_cast<float>(dy) * dy;
            if (span < 0.0f) continue;
            int dx = static_cast<int>(std::sqrt(span));
            SDL_RenderDrawLine(r, static_cast<int>(cx) - dx, static_cast<int>(cy) + dy,
                               static_cast<int>(cx) + dx, static_cast<int>(cy) + dy);
        }
    };

    const float pi = 3.14159265f;
    float sunX = 0.0f, sinD = 0.0f, cosD = 1.0f;
    bool nightLights = clockHours_ >= 0.0f && cam.mapWidth > 0.0f && cam.mapHeight > 0.0f;
    if (nightLights) {
        sunX = clockAnchorX_ + (12.0f - clockHours_) / 24.0f * cam.mapWidth;
        float decl = -23.44f * (pi / 180.0f) *
                     std::cos(2.0f * pi * (clockDayOfYear_ + 10) / 365.24f);
        sinD = std::sin(decl);
        cosD = std::cos(decl);
    }

    for (auto& [cityName, info] : cities) {
        Vec2 loc = regData.getLocation(info.region);
        float sy = cam.worldToScreenY(loc.y, screenH);

        auto capIt = capitalOf.find(cityName);
        bool isCapital = capIt != capitalOf.end();
        Color statusC = relationColor(isCapital ? capIt->second : regData.getOwner(info.region));

        float lightT = 0.0f;
        if (nightLights) {
            float lat = (0.5f - loc.y / cam.mapHeight) * pi;
            float hourAngle = 2.0f * pi * (loc.x - sunX) / cam.mapWidth;
            float sinAlt = std::sin(lat) * sinD + std::cos(lat) * cosD * std::cos(hourAngle);
            lightT = std::clamp(-sinAlt / 0.12f, 0.0f, 1.0f);
        }

        forEachWrappedScreenX(cam, loc.x, screenW, [&](float sx) {
            if (sx < -50.0f || sx > screenW + 50.0f || sy < -50.0f || sy > screenH + 50.0f) return;

            if (lightT > 0.0f) {
                float glowR = std::clamp(2.6f * cam.zoom, 2.2f, 9.0f);
                fillCircleAt(sx, sy, glowR, {255, 208, 128, static_cast<uint8_t>(46.0f * lightT)});
                fillCircleAt(sx, sy, glowR * 0.45f, {255, 222, 160, static_cast<uint8_t>(120.0f * lightT)});
            }

            if (isCapital) {
                float starOuter = std::clamp(3.6f * cam.zoom, 2.4f, 14.0f);
                float starInner = starOuter * 0.4f;
                drawStar(r, sx, sy, starOuter + 1.2f, starInner + 0.6f, 14, 15, 14, 255);
                drawStar(r, sx, sy, starOuter, starInner, statusC.r, statusC.g, statusC.b, 255);
            } else {
                float rad = std::clamp(1.9f * cam.zoom, 1.4f, 8.0f);
                fillCircleAt(sx, sy, rad + 1.0f, {14, 15, 14, 255});
                fillCircleAt(sx, sy, rad, statusC);
                fillCircleAt(sx, sy, rad * 0.45f, {14, 15, 14, 235});
            }
        });
    }

    if (onlyCities) return;


    SDL_Texture* anchorTex = UIAssets::instance().icon("Anchor");
    for (int portRegion : gs.ports) {
        Vec2 loc = regData.getLocation(portRegion);
        float sy = cam.worldToScreenY(loc.y, screenH);
        forEachWrappedScreenX(cam, loc.x, screenW, [&](float sx) {
            if (sx < -50.0f || sx > screenW + 50.0f || sy < -50.0f || sy > screenH + 50.0f) return;

            int size = std::clamp(static_cast<int>(4.5f * cam.zoom), 5, 24);
            SDL_Rect dst = {
                static_cast<int>(sx - size / 2),
                static_cast<int>(sy - size / 2 + size),
                size, size
            };

            if (anchorTex) {
                SDL_SetTextureColorMod(anchorTex, 8, 10, 12);
                SDL_SetTextureAlphaMod(anchorTex, 190);
                SDL_Rect sh = {dst.x + 1, dst.y + 1, dst.w, dst.h};
                SDL_RenderCopy(r, anchorTex, nullptr, &sh);
                SDL_SetTextureColorMod(anchorTex, 92, 170, 255);
                SDL_SetTextureAlphaMod(anchorTex, 245);
                SDL_RenderCopy(r, anchorTex, nullptr, &dst);
                SDL_SetTextureColorMod(anchorTex, 255, 255, 255);
                SDL_SetTextureAlphaMod(anchorTex, 255);
            } else {
                SDL_SetRenderDrawColor(r, 40, 100, 180, 220);
                SDL_RenderFillRect(r, &dst);
            }
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
        if (!selOverlayTex_ || selOverlayKey_ != selectedRegions ||
            selOverlayMask_ != gs.regionsMapSurf) {
            SDL_Surface* overlay = SDL_CreateRGBSurfaceWithFormat(0, gs.regionsMapSurf->w, gs.regionsMapSurf->h, 32, SDL_PIXELFORMAT_RGBA32);
            if (overlay) {
                SDL_FillRect(overlay, nullptr, SDL_MapRGBA(overlay->format, 0, 0, 0, 0));

                auto& rd = RegionData::instance();
                for (int regId : selectedRegions) {
                    if (regId <= 0) continue;
                    Vec2 loc = rd.getLocation(regId);
                    MapFunc::fillRegionMask(overlay, gs.regionsMapSurf, regId, loc.x, loc.y, {175, 40, 40, 110});
                }
                MapFunc::expandFillToLines(overlay, gs.regionsMapSurf);

                SDL_Texture* overlayTex = SDL_CreateTextureFromSurface(r, overlay);
                SDL_FreeSurface(overlay);
                if (overlayTex) {
                    SDL_SetTextureBlendMode(overlayTex, SDL_BLENDMODE_BLEND);
                    if (selOverlayTex_) SDL_DestroyTexture(selOverlayTex_);
                    selOverlayTex_ = overlayTex;
                    selOverlayKey_ = selectedRegions;
                    selOverlayMask_ = gs.regionsMapSurf;
                }
            }
        }

        if (selOverlayTex_) {
            int mw = gs.regionsMapSurf->w;
            int mh = gs.regionsMapSurf->h;
            auto drawOverlayCopy = [&](float offsetX) {
                SDL_Rect dst;
                dst.x = static_cast<int>(std::lround(cam.worldToScreenX(offsetX, screenW)));
                dst.y = static_cast<int>(std::lround(cam.worldToScreenY(0.0f, screenH)));
                dst.w = static_cast<int>(std::lround(mw * cam.zoom));
                dst.h = static_cast<int>(std::lround(mh * cam.zoom));
                SDL_RenderCopy(r, selOverlayTex_, nullptr, &dst);
            };
            drawOverlayCopy(0.0f);
            drawOverlayCopy(static_cast<float>(mw));
            drawOverlayCopy(static_cast<float>(-mw));
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


namespace {

void drawWrappedOverlay(SDL_Renderer* r, SDL_Texture* tex, const Camera& cam,
                        int screenW, int screenH, int mw, int mh) {
    int x0 = static_cast<int>(std::lround(cam.worldToScreenX(0, screenW)));
    int y0 = static_cast<int>(std::lround(cam.worldToScreenY(0, screenH)));
    int drawW = std::max(1, static_cast<int>(std::lround(mw * cam.zoom)));
    int drawH = std::max(1, static_cast<int>(std::lround(mh * cam.zoom)));
    SDL_Rect dsts[3] = {
        {x0, y0, drawW, drawH},
        {x0 + drawW, y0, drawW, drawH},
        {x0 - drawW, y0, drawW, drawH}
    };
    for (auto& dst : dsts) {
        if (dst.x + dst.w < 0 || dst.x > screenW) continue;
        SDL_RenderCopy(r, tex, nullptr, &dst);
    }
}

}

void MapRenderer::renderCountryHighlight(SDL_Renderer* r, GameState& gs, const Camera& cam,
                                         int screenW, int screenH, const std::string& countryName) {
    if (countryName.empty() || !gs.regionsMapSurf) return;
    Country* c = gs.getCountry(countryName);
    if (!c || c->regions.empty()) return;

    std::string key = countryName + ":" + std::to_string(c->regions.size());
    if (!hlTex_ || key != hlKey_ || hlMask_ != gs.regionsMapSurf) {
        SDL_Surface* overlay = SDL_CreateRGBSurfaceWithFormat(0, gs.regionsMapSurf->w, gs.regionsMapSurf->h,
                                                              32, SDL_PIXELFORMAT_RGBA32);
        if (!overlay) return;
        SDL_FillRect(overlay, nullptr, SDL_MapRGBA(overlay->format, 0, 0, 0, 0));

        auto& rd = RegionData::instance();
        for (int regId : c->regions) {
            if (regId <= 0) continue;
            Vec2 loc = rd.getLocation(regId);
            MapFunc::fillRegionMask(overlay, gs.regionsMapSurf, regId, loc.x, loc.y, {255, 248, 228, 30});
        }
        MapFunc::expandFillToLines(overlay, gs.regionsMapSurf);

        SDL_LockSurface(overlay);
        int ow = overlay->w, oh = overlay->h;
        int pitch = overlay->pitch;
        uint8_t* px = static_cast<uint8_t*>(overlay->pixels);
        for (int y = 0; y < oh; y++) {
            for (int x = 0; x < ow; x++) {
                uint8_t* p = px + y * pitch + x * 4;
                if (p[3] == 0) continue;
                bool edge = false;
                if (x == 0 || px[y * pitch + (x - 1) * 4 + 3] == 0) edge = true;
                else if (x == ow - 1 || px[y * pitch + (x + 1) * 4 + 3] == 0) edge = true;
                else if (y == 0 || px[(y - 1) * pitch + x * 4 + 3] == 0) edge = true;
                else if (y == oh - 1 || px[(y + 1) * pitch + x * 4 + 3] == 0) edge = true;
                if (edge && p[3] != 0 && p[3] != 215) {
                    p[0] = 216; p[1] = 195; p[2] = 134; p[3] = 214;
                }
            }
        }
        SDL_UnlockSurface(overlay);

        SDL_Texture* tex = SDL_CreateTextureFromSurface(r, overlay);
        SDL_FreeSurface(overlay);
        if (!tex) return;
        SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
        if (hlTex_) SDL_DestroyTexture(hlTex_);
        hlTex_ = tex;
        hlKey_ = key;
        hlMask_ = gs.regionsMapSurf;
    }

    if (hlTex_) {
        float pulse = 0.72f + 0.28f * std::sin(SDL_GetTicks() * 0.004f);
        SDL_SetTextureAlphaMod(hlTex_, static_cast<Uint8>(255.0f * pulse));
        drawWrappedOverlay(r, hlTex_, cam, screenW, screenH,
                           gs.regionsMapSurf->w, gs.regionsMapSurf->h);
        SDL_SetTextureAlphaMod(hlTex_, 255);
    }
}

void MapRenderer::renderPuppetStripes(SDL_Renderer* r, GameState& gs, const Camera& cam,
                                      int screenW, int screenH) {
    if (!gs.regionsMapSurf) return;

    std::string key;
    for (auto& ps : gs.puppetStates) {
        if (!ps.active) continue;
        Country* pc = gs.getCountry(ps.puppet);
        if (!pc || pc->regions.empty()) continue;
        key += ps.puppet + ":" + std::to_string(pc->regions.size()) + ";";
    }
    if (key.empty()) {
        if (pupTex_) { SDL_DestroyTexture(pupTex_); pupTex_ = nullptr; }
        pupKey_.clear();
        return;
    }

    if (!pupTex_ || key != pupKey_ || pupMask_ != gs.regionsMapSurf) {
        SDL_Surface* overlay = SDL_CreateRGBSurfaceWithFormat(0, gs.regionsMapSurf->w, gs.regionsMapSurf->h,
                                                              32, SDL_PIXELFORMAT_RGBA32);
        if (!overlay) return;
        SDL_FillRect(overlay, nullptr, SDL_MapRGBA(overlay->format, 0, 0, 0, 0));

        auto& rd = RegionData::instance();
        for (auto& ps : gs.puppetStates) {
            if (!ps.active) continue;
            Country* pc = gs.getCountry(ps.puppet);
            if (!pc) continue;
            for (int regId : pc->regions) {
                if (regId <= 0) continue;
                Vec2 loc = rd.getLocation(regId);
                MapFunc::fillRegionMask(overlay, gs.regionsMapSurf, regId, loc.x, loc.y, {12, 12, 16, 88});
            }
        }

        SDL_LockSurface(overlay);
        int ow = overlay->w, oh = overlay->h;
        int pitch = overlay->pitch;
        uint8_t* px = static_cast<uint8_t*>(overlay->pixels);
        for (int y = 0; y < oh; y++) {
            for (int x = 0; x < ow; x++) {
                if (((x + y) % 7) >= 3) px[y * pitch + x * 4 + 3] = 0;
            }
        }
        SDL_UnlockSurface(overlay);

        SDL_Texture* tex = SDL_CreateTextureFromSurface(r, overlay);
        SDL_FreeSurface(overlay);
        if (!tex) return;
        SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
        if (pupTex_) SDL_DestroyTexture(pupTex_);
        pupTex_ = tex;
        pupKey_ = key;
        pupMask_ = gs.regionsMapSurf;
    }

    if (pupTex_) {
        drawWrappedOverlay(r, pupTex_, cam, screenW, screenH,
                           gs.regionsMapSurf->w, gs.regionsMapSurf->h);
    }
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

    int wx = static_cast<int>(std::floor(world.x));
    int wy = static_cast<int>(std::floor(world.y));


    while (wx < 0) wx += mapW;
    while (wx >= mapW) wx -= mapW;

    if (wy < 0 || wy >= mapH) return -1;

    auto& rd = RegionData::instance();
    SDL_LockSurface(regionsMap);
    int found = rd.getRegion(getPixel(regionsMap, wx, wy));

    if (found <= 0) {
        int maxR = std::clamp(static_cast<int>(std::ceil(2.0f / std::max(0.25f, cam.zoom))) + 1, 2, 6);
        for (int radius = 1; radius <= maxR && found <= 0; radius++) {
            for (int dy = -radius; dy <= radius && found <= 0; dy++) {
                int ny = wy + dy;
                if (ny < 0 || ny >= mapH) continue;
                for (int dx = -radius; dx <= radius; dx++) {
                    if (std::max(std::abs(dx), std::abs(dy)) != radius) continue;
                    int nx = wx + dx;
                    while (nx < 0) nx += mapW;
                    while (nx >= mapW) nx -= mapW;
                    int cand = rd.getRegion(getPixel(regionsMap, nx, ny));
                    if (cand > 0) {
                        found = cand;
                        break;
                    }
                }
            }
        }
    }
    SDL_UnlockSurface(regionsMap);

    return found;
}
