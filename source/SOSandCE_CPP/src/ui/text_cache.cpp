#include "ui/text_cache.h"
#include "core/engine.h"

static TextCache* s_textCache = nullptr;

TextCache& textCache() {
    if (!s_textCache) s_textCache = new TextCache(512);
    return *s_textCache;
}

TextCache::TextCache(size_t maxEntries) : maxEntries_(maxEntries) {}

TextCache::~TextCache() {
    clear();
}

void TextCache::clear() {
    for (auto& [k, e] : cache_) {
        if (e.texture) SDL_DestroyTexture(e.texture);
    }
    cache_.clear();
    lru_.clear();
}

void TextCache::evict() {
    while (cache_.size() >= maxEntries_ && !lru_.empty()) {
        auto key = lru_.front();
        lru_.pop_front();
        auto it = cache_.find(key);
        if (it != cache_.end()) {
            if (it->second.texture) SDL_DestroyTexture(it->second.texture);
            cache_.erase(it);
        }
    }
}

SDL_Texture* TextCache::get(const std::string& text, int size, Color color) {
    return getEntry(text, size, color).texture;
}

TextCache::Entry TextCache::getEntry(const std::string& text, int size, Color color) {
    TextCacheKey key{text, size, color};
    auto it = cache_.find(key);
    if (it != cache_.end()) {

        for (auto lit = lru_.begin(); lit != lru_.end(); ++lit) {
            if (*lit == key) { lru_.erase(lit); break; }
        }
        lru_.push_back(key);
        return it->second;
    }


    evict();

    auto& engine = Engine::instance();
    TTF_Font* font = engine.getFont(std::max(1, size));
    if (!font || text.empty()) return {nullptr, 0, 0};

    SDL_Color sdlColor = color.toSDL();
    SDL_Surface* surf = TTF_RenderUTF8_Blended(font, text.c_str(), sdlColor);
    if (!surf) return {nullptr, 0, 0};

    SDL_Texture* tex = SDL_CreateTextureFromSurface(engine.renderer, surf);
    int w = surf->w, h = surf->h;
    SDL_FreeSurface(surf);

    if (!tex) return {nullptr, 0, 0};

    Entry entry{tex, w, h};
    cache_[key] = entry;
    lru_.push_back(key);
    return entry;
}
