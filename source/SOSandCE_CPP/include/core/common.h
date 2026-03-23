#pragma once


#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <set>
#include <queue>
#include <functional>
#include <memory>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <random>
#include <optional>
#include <array>
#include <numeric>
#include <sstream>
#include <fstream>
#include <iostream>
#include <chrono>


#include <SDL.h>
#include <SDL_image.h>
#include <SDL_mixer.h>
#include <SDL_ttf.h>

namespace fs = std::filesystem;


struct Color {
    uint8_t r = 0, g = 0, b = 0, a = 255;

    bool operator==(const Color& o) const { return r == o.r && g == o.g && b == o.b && a == o.a; }
    bool operator!=(const Color& o) const { return !(*this == o); }


    bool operator<(const Color& o) const {
        if (r != o.r) return r < o.r;
        if (g != o.g) return g < o.g;
        if (b != o.b) return b < o.b;
        return a < o.a;
    }

    SDL_Color toSDL() const { return SDL_Color{r, g, b, a}; }

    uint32_t toUint32() const {
        return (static_cast<uint32_t>(r) << 24) |
               (static_cast<uint32_t>(g) << 16) |
               (static_cast<uint32_t>(b) << 8)  |
               static_cast<uint32_t>(a);
    }
};


struct ColorHash {
    std::size_t operator()(const Color& c) const noexcept {

        uint32_t v = (static_cast<uint32_t>(c.r) << 24) |
                     (static_cast<uint32_t>(c.g) << 16) |
                     (static_cast<uint32_t>(c.b) << 8)  |
                     static_cast<uint32_t>(c.a);
        return std::hash<uint32_t>{}(v);
    }
};


struct Vec2 {
    float x = 0.0f, y = 0.0f;

    Vec2 operator+(const Vec2& o) const { return {x + o.x, y + o.y}; }
    Vec2 operator-(const Vec2& o) const { return {x - o.x, y - o.y}; }
    Vec2 operator*(float s) const { return {x * s, y * s}; }
    float length() const { return std::sqrt(x * x + y * y); }
    float dot(const Vec2& o) const { return x * o.x + y * o.y; }
};

struct Vec2i {
    int x = 0, y = 0;

    Vec2i operator+(const Vec2i& o) const { return {x + o.x, y + o.y}; }
    Vec2i operator-(const Vec2i& o) const { return {x - o.x, y - o.y}; }
    bool operator==(const Vec2i& o) const { return x == o.x && y == o.y; }
    bool operator!=(const Vec2i& o) const { return !(*this == o); }
};

struct Rect {
    float x = 0.0f, y = 0.0f, w = 0.0f, h = 0.0f;

    bool contains(float px, float py) const {
        return px >= x && px < x + w && py >= y && py < y + h;
    }

    SDL_Rect toSDL() const {
        return SDL_Rect{
            static_cast<int>(x),
            static_cast<int>(y),
            static_cast<int>(w),
            static_cast<int>(h)
        };
    }
};


inline Color getPixel(SDL_Surface* surface, int x, int y) {
    if (!surface || x < 0 || y < 0 || x >= surface->w || y >= surface->h)
        return {0, 0, 0, 0};

    int bpp = surface->format->BytesPerPixel;
    const uint8_t* p = static_cast<const uint8_t*>(surface->pixels)
                       + y * surface->pitch + x * bpp;

    uint32_t pixel = 0;
    switch (bpp) {
        case 1: pixel = *p; break;
        case 2: pixel = *reinterpret_cast<const uint16_t*>(p); break;
        case 3:
            if constexpr (SDL_BYTEORDER == SDL_BIG_ENDIAN)
                pixel = (p[0] << 16) | (p[1] << 8) | p[2];
            else
                pixel = p[0] | (p[1] << 8) | (p[2] << 16);
            break;
        case 4: pixel = *reinterpret_cast<const uint32_t*>(p); break;
        default: break;
    }

    Color c;
    SDL_GetRGBA(pixel, surface->format, &c.r, &c.g, &c.b, &c.a);
    return c;
}

inline void setPixel(SDL_Surface* surface, int x, int y, Color c) {
    if (!surface || x < 0 || y < 0 || x >= surface->w || y >= surface->h)
        return;

    uint32_t pixel = SDL_MapRGBA(surface->format, c.r, c.g, c.b, c.a);
    int bpp = surface->format->BytesPerPixel;
    uint8_t* p = static_cast<uint8_t*>(surface->pixels)
                 + y * surface->pitch + x * bpp;

    switch (bpp) {
        case 1: *p = static_cast<uint8_t>(pixel); break;
        case 2: *reinterpret_cast<uint16_t*>(p) = static_cast<uint16_t>(pixel); break;
        case 3:
            if constexpr (SDL_BYTEORDER == SDL_BIG_ENDIAN) {
                p[0] = static_cast<uint8_t>((pixel >> 16) & 0xFF);
                p[1] = static_cast<uint8_t>((pixel >> 8)  & 0xFF);
                p[2] = static_cast<uint8_t>(pixel & 0xFF);
            } else {
                p[0] = static_cast<uint8_t>(pixel & 0xFF);
                p[1] = static_cast<uint8_t>((pixel >> 8)  & 0xFF);
                p[2] = static_cast<uint8_t>((pixel >> 16) & 0xFF);
            }
            break;
        case 4: *reinterpret_cast<uint32_t*>(p) = pixel; break;
        default: break;
    }
}


inline std::mt19937& rng() {
    static std::mt19937 gen{std::random_device{}()};
    return gen;
}

inline int randInt(int lo, int hi) {
    std::uniform_int_distribution<int> dist(lo, hi);
    return dist(rng());
}

inline float randFloat(float lo, float hi) {
    std::uniform_real_distribution<float> dist(lo, hi);
    return dist(rng());
}


inline std::string replaceAll(std::string s, const std::string& from, const std::string& to) {
    if (from.empty()) return s;
    std::string::size_type pos = 0;
    while ((pos = s.find(from, pos)) != std::string::npos) {
        s.replace(pos, from.size(), to);
        pos += to.size();
    }
    return s;
}

inline std::string titleCase(std::string s) {
    bool capitalise = true;
    for (auto& ch : s) {
        if (std::isspace(static_cast<unsigned char>(ch)) || ch == '_' || ch == '-') {
            capitalise = true;
            if (ch == '_') ch = ' ';
        } else if (capitalise) {
            ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
            capitalise = false;
        } else {
            ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        }
    }
    return s;
}
