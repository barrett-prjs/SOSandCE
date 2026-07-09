#pragma once
#include "core/common.h"

class Audio {
public:
    static Audio& instance();

    void init();
    void shutdown();


    Mix_Chunk* loadSound(const std::string& path);
    void playSound(const std::string& name);
    void setSoundVolume(float vol);


    void playMusic(const std::string& path, bool loop = true, int fadeMs = 0);
    void stopMusic(int fadeMs = 0);
    void setMusicVolume(float vol);
    bool isMusicPlaying() const;


    void loadAllSounds(const std::string& dir, float volume = 1.0f);

    std::string currentMusic;

private:
    Audio() = default;
    std::unordered_map<std::string, Mix_Chunk*> sounds_;
    Mix_Music* currentTrack_ = nullptr;
    float soundVolume_ = 1.0f;
    float musicVolume_ = 1.0f;
};
