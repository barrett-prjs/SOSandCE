#pragma once

#include "core/common.h"
#include <deque>


struct TextCacheKey {
    std::string text;
    int         size;
    Color       color;

    bool operator==(const TextCacheKey& o) const {
        return text == o.text && size == o.size && color == o.color;
    }
};


struct TextCacheKeyHash {
    size_t operator()(const TextCacheKey& k) const {
        size_t h = std::hash<std::string>()(k.text);
        h ^= std::hash<int>()(k.size) << 1;
        h ^= (std::hash<int>()(k.color.r)
            ^ (std::hash<int>()(k.color.g) << 8)
            ^ (std::hash<int>()(k.color.b) << 16));
        return h;
    }
};

class TextCache {
public:

    struct Entry {
        SDL_Texture* texture = nullptr;
        int w = 0;
        int h = 0;
    };


    explicit TextCache(size_t maxEntries = 512);


    ~TextCache();


    TextCache(const TextCache&)            = delete;
    TextCache& operator=(const TextCache&) = delete;


    SDL_Texture* get(const std::string& text, int size,
                     Color color = {210, 200, 172});


    Entry getEntry(const std::string& text, int size,
                   Color color = {210, 200, 172});


    void clear();


    size_t size() const { return cache_.size(); }

private:

    void evict();


    void touch(const TextCacheKey& key);

    std::unordered_map<TextCacheKey, Entry, TextCacheKeyHash> cache_;
    std::deque<TextCacheKey> lru_;
    size_t maxEntries_;
};


TextCache& textCache();
