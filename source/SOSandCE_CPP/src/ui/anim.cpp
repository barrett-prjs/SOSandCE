#include "ui/anim.h"

namespace UIAnim {

namespace {

struct SpringState {
    float pos = 0.0f;
    float vel = 0.0f;
    uint32_t lastTouched = 0;
};

struct CountState {
    float value = 0.0f;
    bool initialized = false;
};

float s_dt = 1.0f / 60.0f;
uint32_t s_frameTicks = 0;
bool s_transitionActive = false;

std::unordered_map<std::string, uint32_t> s_transitions;
std::unordered_map<std::string, SpringState> s_springs;
std::unordered_map<std::string, CountState> s_counters;

float easeOutCubic(float t) {
    float inv = 1.0f - t;
    return 1.0f - inv * inv * inv;
}

void sweepIfBloated() {
    if (s_springs.size() > 2048) s_springs.clear();
    if (s_counters.size() > 2048) s_counters.clear();
    if (s_transitions.size() > 512) s_transitions.clear();
}

}

void beginFrame(float dt) {
    s_dt = std::clamp(dt, 0.0f, 0.05f);
    s_frameTicks = SDL_GetTicks();
    sweepIfBloated();
}

float frameDt() {
    return s_dt;
}

void trigger(const std::string& id) {
    s_transitions[id] = SDL_GetTicks();
}

void forget(const std::string& id) {
    s_transitions.erase(id);
}

float progress(const std::string& id) {
    auto it = s_transitions.find(id);
    if (it == s_transitions.end()) {
        s_transitions[id] = s_frameTicks;
        it = s_transitions.find(id);
    }
    float elapsed = (s_frameTicks - it->second) / 1000.0f;
    float t = std::clamp(elapsed / MENU_OPEN_SECONDS, 0.0f, 1.0f);
    return easeOutCubic(t);
}

void pushMenuTransition(SDL_Renderer* r, const std::string& id, float centerX, float centerY) {
    float t = progress(id);
    if (t >= 0.999f) {
        s_transitionActive = false;
        return;
    }
    float scale = 0.8f + 0.2f * t;
    SDL_Rect vp;
    vp.x = static_cast<int>(std::lround(centerX * (1.0f - scale)));
    vp.y = static_cast<int>(std::lround(centerY * (1.0f - scale)));
    vp.w = 16384;
    vp.h = 16384;
    SDL_RenderSetViewport(r, &vp);
    SDL_RenderSetScale(r, scale, scale);
    s_transitionActive = true;
}

void popMenuTransition(SDL_Renderer* r) {
    if (!s_transitionActive) return;
    SDL_RenderSetScale(r, 1.0f, 1.0f);
    SDL_RenderSetViewport(r, nullptr);
    s_transitionActive = false;
}

void drawTransitionFade(SDL_Renderer* r, const std::string& id, int screenW, int screenH) {
    float t = progress(id);
    if (t >= 0.999f) return;
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, 0, 0, 0, static_cast<uint8_t>((1.0f - t) * 170.0f));
    SDL_Rect full = {0, 0, screenW, screenH};
    SDL_RenderFillRect(r, &full);
}

float springOffset(const std::string& id, bool active, float target) {
    SpringState& s = s_springs[id];
    s.lastTouched = s_frameTicks;
    float goal = active ? target : 0.0f;

    constexpr float stiffness = 480.0f;
    constexpr float damping = 26.0f;
    float dt = std::min(s_dt, 1.0f / 30.0f);
    s.vel += (stiffness * (goal - s.pos) - damping * s.vel) * dt;
    s.pos += s.vel * dt;

    if (std::fabs(s.pos - goal) < 0.02f && std::fabs(s.vel) < 0.5f) {
        s.pos = goal;
        s.vel = 0.0f;
    }
    return s.pos;
}

float countTo(const std::string& id, float target) {
    CountState& c = s_counters[id];
    if (!c.initialized) {
        c.initialized = true;
        c.value = target;
        return c.value;
    }

    float diff = target - c.value;
    float snapEpsilon = std::max(0.5f, std::fabs(target) * 0.0005f);
    if (std::fabs(diff) <= snapEpsilon) {
        c.value = target;
        return c.value;
    }

    float rate = 1.0f - std::exp(-7.0f * s_dt);
    c.value += diff * rate;
    return c.value;
}

Color brighten(Color c, float factor) {
    auto scale = [factor](uint8_t v) {
        return static_cast<uint8_t>(std::min(255.0f, v * factor));
    };
    return {scale(c.r), scale(c.g), scale(c.b), c.a};
}

void clearAll() {
    s_transitions.clear();
    s_springs.clear();
    s_counters.clear();
    s_transitionActive = false;
}

}
