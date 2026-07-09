#pragma once

#include <string>
#include <fstream>
#include <sstream>
#include <iostream>

struct Settings {
    float uiSize       = 24.0f;
    float musicVolume  = 1.0f;
    float soundVolume  = 1.0f;
    int   fps          = 60;
    bool  fogOfWar     = true;
    bool  sidebarLeft  = false;
    bool  fullscreen   = true;
    int   windowWidth  = 1200;
    int   windowHeight = 675;


    void load(const std::string& path) {
        std::ifstream file(path);
        if (!file.is_open()) return;

        std::string content((std::istreambuf_iterator<char>(file)),
                             std::istreambuf_iterator<char>());
        file.close();

        auto readFloat = [&](const std::string& key, float& out) {
            auto pos = content.find("\"" + key + "\"");
            if (pos == std::string::npos) return;
            pos = content.find(':', pos);
            if (pos == std::string::npos) return;
            out = std::stof(content.substr(pos + 1));
        };

        auto readInt = [&](const std::string& key, int& out) {
            auto pos = content.find("\"" + key + "\"");
            if (pos == std::string::npos) return;
            pos = content.find(':', pos);
            if (pos == std::string::npos) return;
            out = std::stoi(content.substr(pos + 1));
        };

        auto readBool = [&](const std::string& key, bool& out) {
            auto pos = content.find("\"" + key + "\"");
            if (pos == std::string::npos) return;
            pos = content.find(':', pos);
            if (pos == std::string::npos) return;
            std::string rest = content.substr(pos + 1);

            for (auto& ch : rest) {
                if (ch == 't') { out = true;  return; }
                if (ch == 'f') { out = false; return; }
                if (ch != ' ' && ch != '\t' && ch != '\n' && ch != '\r') return;
            }
        };

        try {
            readFloat("uiSize",       uiSize);
            readFloat("musicVolume",  musicVolume);
            readFloat("soundVolume",  soundVolume);
            readInt  ("fps",          fps);
            readBool ("fogOfWar",     fogOfWar);
            readBool ("sidebarLeft",  sidebarLeft);
            readBool ("fullscreen",   fullscreen);
            readInt  ("windowWidth",  windowWidth);
            readInt  ("windowHeight", windowHeight);
        } catch (...) {
            std::cerr << "[Settings] Warning: failed to parse " << path << "\n";
        }
    }


    void save(const std::string& path) const {
        std::ofstream file(path);
        if (!file.is_open()) {
            std::cerr << "[Settings] Warning: could not write " << path << "\n";
            return;
        }

        file << "{\n";
        file << "  \"uiSize\": "       << uiSize       << ",\n";
        file << "  \"musicVolume\": "  << musicVolume  << ",\n";
        file << "  \"soundVolume\": "  << soundVolume  << ",\n";
        file << "  \"fps\": "          << fps          << ",\n";
        file << "  \"fogOfWar\": "     << (fogOfWar ? "true" : "false") << ",\n";
        file << "  \"sidebarLeft\": "  << (sidebarLeft ? "true" : "false") << ",\n";
        file << "  \"fullscreen\": "   << (fullscreen ? "true" : "false") << ",\n";
        file << "  \"windowWidth\": "  << windowWidth  << ",\n";
        file << "  \"windowHeight\": " << windowHeight << "\n";
        file << "}\n";
    }
};
