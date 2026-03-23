#pragma once
#include "ui/widget.h"
#include <functional>

class Button : public Widget {
public:
    std::string label;
    std::string value;
    std::function<void()> onClick;
    bool enabled = true;
    int fontSize = 0;

    Button() = default;
    Button(float x, float y, float w, float h, const std::string& label,
           std::function<void()> onClick = nullptr);

    void handleInput(const InputState& input) override;
    void render(SDL_Renderer* r) override;
};
