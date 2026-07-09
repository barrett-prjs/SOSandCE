#include "core/audio.h"

Audio& Audio::instance() {
    static Audio a;
    return a;
}

void Audio::init() {
    int flags = MIX_INIT_MP3 | MIX_INIT_OGG;
    int initted = Mix_Init(flags);
    if ((initted & MIX_INIT_MP3) == 0) {
        printf("[Audio] Warning: MP3 support not available\n");
    }
    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0) {
        printf("[Audio] Mix_OpenAudio failed: %s\n", Mix_GetError());
    }
    Mix_AllocateChannels(16);
    printf("[Audio] Initialized (formats: %d)\n", initted);
    fflush(stdout);
}

void Audio::shutdown() {
    Mix_HaltMusic();
    if (currentTrack_) {
        Mix_FreeMusic(currentTrack_);
        currentTrack_ = nullptr;
    }
    currentMusic.clear();
    for (auto& [k, c] : sounds_) Mix_FreeChunk(c);
    sounds_.clear();
    Mix_CloseAudio();
}

Mix_Chunk* Audio::loadSound(const std::string& path) {
    auto it = sounds_.find(path);
    if (it != sounds_.end()) return it->second;
    Mix_Chunk* c = Mix_LoadWAV(path.c_str());
    if (c) {
        sounds_[path] = c;
        Mix_VolumeChunk(c, static_cast<int>(soundVolume_ * MIX_MAX_VOLUME));
    }
    return c;
}

void Audio::playSound(const std::string& name) {
    auto it = sounds_.find(name);
    if (it != sounds_.end()) {
        Mix_PlayChannel(-1, it->second, 0);
    }
}

void Audio::setSoundVolume(float vol) {
    soundVolume_ = std::clamp(vol, 0.0f, 1.0f);
    for (auto& [k, c] : sounds_) {
        Mix_VolumeChunk(c, static_cast<int>(soundVolume_ * MIX_MAX_VOLUME));
    }
}

void Audio::playMusic(const std::string& path, bool loop, int fadeMs) {
    if (currentMusic == path && Mix_PlayingMusic()) {
        Mix_VolumeMusic(static_cast<int>(musicVolume_ * MIX_MAX_VOLUME));
        return;
    }

    Mix_HaltMusic();
    if (currentTrack_) {
        Mix_FreeMusic(currentTrack_);
        currentTrack_ = nullptr;
    }
    currentMusic.clear();

    Mix_Music* loadedTrack = Mix_LoadMUS(path.c_str());
    if (!loadedTrack) {
        printf("[Audio] Can't load music %s: %s (non-fatal)\n", path.c_str(), Mix_GetError());
        fflush(stdout);
        return;
    }

    currentTrack_ = loadedTrack;
    if (fadeMs > 0) {
        Mix_FadeInMusic(currentTrack_, loop ? -1 : 0, fadeMs);
    } else {
        Mix_PlayMusic(currentTrack_, loop ? -1 : 0);
    }
    Mix_VolumeMusic(static_cast<int>(musicVolume_ * MIX_MAX_VOLUME));
    currentMusic = path;
}

void Audio::stopMusic(int fadeMs) {
    Mix_HaltMusic();
    if (currentTrack_) {
        Mix_FreeMusic(currentTrack_);
        currentTrack_ = nullptr;
    }
    currentMusic.clear();
}

void Audio::setMusicVolume(float vol) {
    musicVolume_ = std::clamp(vol, 0.0f, 1.0f);
    Mix_VolumeMusic(static_cast<int>(musicVolume_ * MIX_MAX_VOLUME));
}

bool Audio::isMusicPlaying() const {
    return Mix_PlayingMusic() != 0;
}

void Audio::loadAllSounds(const std::string& dir, float volume) {
    if (!fs::exists(dir)) return;
    for (auto& entry : fs::directory_iterator(dir)) {
        if (entry.path().extension() == ".wav") {
            std::string name = entry.path().stem().string();
            Mix_Chunk* c = Mix_LoadWAV(entry.path().string().c_str());
            if (c) {
                Mix_VolumeChunk(c, static_cast<int>(volume * MIX_MAX_VOLUME));
                sounds_[name] = c;
            }
        }
    }
}
