#include "ui/text_cache.h"
#include "core/engine.h"

static TextCache* s_textCache = nullptr;

TextCache& textCache() {
    if (!s_textCache) s_textCache = new TextCache(2048);
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
    if (cache_.size() < maxEntries_) return;
    uint64_t cutoff = useCounter_ > maxEntries_ / 2 ? useCounter_ - maxEntries_ / 2 : 0;
    for (auto it = cache_.begin(); it != cache_.end();) {
        if (it->second.lastUsed < cutoff) {
            if (it->second.texture) SDL_DestroyTexture(it->second.texture);
            it = cache_.erase(it);
        } else {
            ++it;
        }
    }
    while (cache_.size() >= maxEntries_) {
        auto oldest = cache_.begin();
        for (auto it = cache_.begin(); it != cache_.end(); ++it) {
            if (it->second.lastUsed < oldest->second.lastUsed) oldest = it;
        }
        if (oldest->second.texture) SDL_DestroyTexture(oldest->second.texture);
        cache_.erase(oldest);
    }
}

SDL_Texture* TextCache::get(const std::string& text, int size, Color color) {
    return getEntry(text, size, color).texture;
}

TextCache::Entry TextCache::getEntry(const std::string& text, int size, Color color) {
    TextCacheKey key{text, size, color};
    useCounter_++;
    auto it = cache_.find(key);
    if (it != cache_.end()) {
        it->second.lastUsed = useCounter_;
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

    Entry entry{tex, w, h, useCounter_};
    cache_[key] = entry;
    return entry;
}
