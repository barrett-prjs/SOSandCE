#pragma once

#include <vector>
#include <string>
#include <cstring>
#include <SDL.h>

struct InputState {

    int mouseX = 0, mouseY = 0;
    int mouseRelX = 0, mouseRelY = 0;


    bool mouseLeft   = false;
    bool mouseRight  = false;
    bool mouseMiddle = false;


    bool mouseLeftDown  = false;
    bool mouseRightDown = false;
    bool mouseLeftUp    = false;
    bool mouseRightUp   = false;


    int scrollY = 0;


    const uint8_t* keys = nullptr;


    bool keyDown[SDL_NUM_SCANCODES] = {};
    bool keyUp[SDL_NUM_SCANCODES]   = {};


    std::vector<SDL_Event> events;

    bool quit = false;


    bool windowResized = false;
    int  newWidth  = 0;
    int  newHeight = 0;


    std::string textInput;
    bool hasTextInput = false;


    void beginFrame() {
        mouseLeftDown  = false;
        mouseRightDown = false;
        mouseLeftUp    = false;
        mouseRightUp   = false;
        scrollY        = 0;
        mouseRelX      = 0;
        mouseRelY      = 0;

        std::memset(keyDown, 0, sizeof(keyDown));
        std::memset(keyUp,   0, sizeof(keyUp));

        events.clear();
        windowResized = false;
        newWidth  = 0;
        newHeight = 0;
        textInput.clear();
        hasTextInput = false;
    }


    void processEvent(const SDL_Event& e) {
        events.push_back(e);

        switch (e.type) {
            case SDL_QUIT:
                quit = true;
                break;

            case SDL_MOUSEMOTION:
                mouseX    = e.motion.x;
                mouseY    = e.motion.y;
                mouseRelX += e.motion.xrel;
                mouseRelY += e.motion.yrel;
                break;

            case SDL_MOUSEBUTTONDOWN:
                if (e.button.button == SDL_BUTTON_LEFT)   { mouseLeft   = true; mouseLeftDown  = true; }
                if (e.button.button == SDL_BUTTON_RIGHT)  { mouseRight  = true; mouseRightDown = true; }
                if (e.button.button == SDL_BUTTON_MIDDLE) { mouseMiddle = true; }
                break;

            case SDL_MOUSEBUTTONUP:
                if (e.button.button == SDL_BUTTON_LEFT)   { mouseLeft   = false; mouseLeftUp  = true; }
                if (e.button.button == SDL_BUTTON_RIGHT)  { mouseRight  = false; mouseRightUp = true; }
                if (e.button.button == SDL_BUTTON_MIDDLE) { mouseMiddle = false; }
                break;

            case SDL_MOUSEWHEEL:
                scrollY += e.wheel.y;
                break;

            case SDL_KEYDOWN:
                if (e.key.keysym.scancode < SDL_NUM_SCANCODES && !e.key.repeat)
                    keyDown[e.key.keysym.scancode] = true;
                break;

            case SDL_KEYUP:
                if (e.key.keysym.scancode < SDL_NUM_SCANCODES)
                    keyUp[e.key.keysym.scancode] = true;
                break;

            case SDL_TEXTINPUT:
                textInput += e.text.text;
                hasTextInput = true;
                break;

            case SDL_WINDOWEVENT:
                if (e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED ||
                    e.window.event == SDL_WINDOWEVENT_RESIZED) {
                    windowResized = true;
                    newWidth  = e.window.data1;
                    newHeight = e.window.data2;
                }
                break;

            default:
                break;
        }
    }


    void endFrame() {
        keys = SDL_GetKeyboardState(nullptr);
    }


    bool isKeyDown(SDL_Scancode sc) const {
        return (sc < SDL_NUM_SCANCODES) && keyDown[sc];
    }


    bool isKeyHeld(SDL_Scancode sc) const {
        return keys && keys[sc];
    }


    bool isKeyUp(SDL_Scancode sc) const {
        return (sc < SDL_NUM_SCANCODES) && keyUp[sc];
    }
};
