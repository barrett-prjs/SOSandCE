#pragma once

#include "core/common.h"


class UIAssets {
public:
    static UIAssets& instance();


    void init(const std::string& assetsBase);


    SDL_Texture* panelWindowBig()   const;
    SDL_Texture* panelWindowMedium() const;
    SDL_Texture* panelHeader()      const;
    SDL_Texture* panelBodyHeaded()  const;
    SDL_Texture* panelBodyHeadless() const;


    SDL_Texture* btnRectDefault()   const;
    SDL_Texture* btnRectHover()     const;

    SDL_Texture* btnSquare(const std::string& name, bool hover = false) const;


    SDL_Texture* icon(const std::string& name) const;


    SDL_Texture* starActive()   const;
    SDL_Texture* starInactive() const;


    SDL_Texture* progressBg()   const;
    SDL_Texture* progressFill() const;


    static void drawStretched(SDL_Renderer* r, SDL_Texture* tex,
                              float x, float y, float w, float h);


    static void drawFit(SDL_Renderer* r, SDL_Texture* tex,
                        float x, float y, float w, float h);


    static void draw9Slice(SDL_Renderer* r, SDL_Texture* tex,
                           float x, float y, float w, float h, int border);

private:
    UIAssets() = default;
    std::string base_;
};
