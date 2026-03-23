#pragma once
#include "core/common.h"
#include <functional>

struct InputState;

struct PopupButton {
    std::string label;
    std::string action;
    std::function<void()> callback;
    float xOffset = 0;
    float yOffset = 5.25f;
    float halfWidth = 4.0f;
};

class Popup {
public:
    Popup() = default;
    Popup(const std::string& title, const std::vector<std::string>& text,
          const std::vector<PopupButton>& buttons, float xSize = 22, float ySize = 6,
          float x = -1, float y = -1, const std::string& flag1 = "",
          const std::string& flag2 = "", bool dismissOnClick = false);

    std::string title;
    std::vector<std::string> text;
    std::vector<PopupButton> buttons;
    float xSize, ySize;
    float xBase, yBase;
    float x, y;
    std::string flag1, flag2;
    std::string type = "popup";
    bool pressed = false;
    bool dismissOnClick = false;

    void update(const InputState& input, float uiSize, std::vector<std::unique_ptr<Popup>>& popupList);
    void draw(SDL_Renderer* r, float uiSize);

private:
    SDL_Texture* image_ = nullptr;
    int imageW_ = 0, imageH_ = 0;
    float xOffset_ = 0, yOffset_ = 0;
    bool holdingPopup_ = false;

    void rebuildImage(float uiSize);
};

class TextBox : public Popup {
public:
    TextBox(const std::string& title, const std::vector<PopupButton>& buttons,
            float xSize, float ySize, float x, float y);

    std::string inputText;
    int maxChars = 16;

    void update(const InputState& input, float uiSize, std::vector<std::unique_ptr<Popup>>& popupList);
};
