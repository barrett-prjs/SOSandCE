#include "ui/ui_assets.h"
#include "core/engine.h"

UIAssets& UIAssets::instance() {
    static UIAssets s;
    return s;
}

void UIAssets::init(const std::string& assetsBase) {
    base_ = assetsBase + "ui_pack/";
}


SDL_Texture* UIAssets::panelWindowBig()    const { return Engine::instance().loadTexture(base_ + "panels/Window-Big.png"); }
SDL_Texture* UIAssets::panelWindowMedium() const { return Engine::instance().loadTexture(base_ + "panels/Window-Medium.png"); }
SDL_Texture* UIAssets::panelHeader()       const { return Engine::instance().loadTexture(base_ + "panels/Header.png"); }
SDL_Texture* UIAssets::panelBodyHeaded()   const { return Engine::instance().loadTexture(base_ + "panels/Body-Headed.png"); }
SDL_Texture* UIAssets::panelBodyHeadless() const {
    SDL_Texture* external = Engine::instance().loadTexture(
        "C:/Users/the pc/Downloads/NewLoader/ProjectSOS/Assetpacks/Prinbles_GUI_Asset_Silent (1.0.0)/asset/png/Panel/Body/Headless.png");
    if (external) return external;
    return Engine::instance().loadTexture(base_ + "panels/Body-Headless.png");
}


SDL_Texture* UIAssets::btnRectDefault() const { return Engine::instance().loadTexture(base_ + "buttons/Base-Rect-Default.png"); }
SDL_Texture* UIAssets::btnRectHover()   const { return Engine::instance().loadTexture(base_ + "buttons/Base-Rect-Hover.png"); }
SDL_Texture* UIAssets::btnSquare(const std::string& name, bool hover) const {

    std::string state = hover ? "Hover" : "Default";
    SDL_Texture* tex = Engine::instance().loadTexture(base_ + "buttons/Square-" + name + "-" + state + ".png");
    if (!tex) tex = Engine::instance().loadTexture(base_ + "buttons/Base-Square-" + state + ".png");
    return tex;
}


SDL_Texture* UIAssets::icon(const std::string& name) const {
    return Engine::instance().loadTexture(base_ + "icons/" + name + ".png");
}


SDL_Texture* UIAssets::starActive()   const { return Engine::instance().loadTexture(base_ + "stars/Active.png"); }
SDL_Texture* UIAssets::starInactive() const { return Engine::instance().loadTexture(base_ + "stars/Unactive.png"); }


SDL_Texture* UIAssets::progressBg()   const { return Engine::instance().loadTexture(base_ + "progress/Background.png"); }
SDL_Texture* UIAssets::progressFill() const { return Engine::instance().loadTexture(base_ + "progress/Line.png"); }


void UIAssets::drawStretched(SDL_Renderer* r, SDL_Texture* tex,
                              float x, float y, float w, float h) {
    if (!tex) return;
    SDL_Rect dst = {(int)x, (int)y, (int)w, (int)h};
    SDL_RenderCopy(r, tex, nullptr, &dst);
}

void UIAssets::drawFit(SDL_Renderer* r, SDL_Texture* tex,
                        float x, float y, float w, float h) {
    if (!tex) return;
    int tw, th;
    SDL_QueryTexture(tex, nullptr, nullptr, &tw, &th);
    float scale = std::min(w / tw, h / th);
    float dw = tw * scale, dh = th * scale;
    SDL_Rect dst = {(int)(x + (w - dw) / 2), (int)(y + (h - dh) / 2), (int)dw, (int)dh};
    SDL_RenderCopy(r, tex, nullptr, &dst);
}

void UIAssets::draw9Slice(SDL_Renderer* r, SDL_Texture* tex,
                           float x, float y, float w, float h, int border) {
    if (!tex) return;
    int tw, th;
    SDL_QueryTexture(tex, nullptr, nullptr, &tw, &th);

    int b = border;
    int dstX = (int)x, dstY = (int)y, dstW = (int)w, dstH = (int)h;


    b = std::min(b, std::min(tw / 2, th / 2));
    int db = std::min(b, std::min(dstW / 2, dstH / 2));


    struct Slice { SDL_Rect src, dst; };
    Slice slices[9] = {

        {{0, 0, b, b},                      {dstX, dstY, db, db}},
        {{b, 0, tw - 2*b, b},               {dstX + db, dstY, dstW - 2*db, db}},
        {{tw - b, 0, b, b},                 {dstX + dstW - db, dstY, db, db}},

        {{0, b, b, th - 2*b},               {dstX, dstY + db, db, dstH - 2*db}},
        {{b, b, tw - 2*b, th - 2*b},        {dstX + db, dstY + db, dstW - 2*db, dstH - 2*db}},
        {{tw - b, b, b, th - 2*b},          {dstX + dstW - db, dstY + db, db, dstH - 2*db}},

        {{0, th - b, b, b},                 {dstX, dstY + dstH - db, db, db}},
        {{b, th - b, tw - 2*b, b},          {dstX + db, dstY + dstH - db, dstW - 2*db, db}},
        {{tw - b, th - b, b, b},            {dstX + dstW - db, dstY + dstH - db, db, db}},
    };

    for (auto& s : slices) {
        if (s.dst.w > 0 && s.dst.h > 0 && s.src.w > 0 && s.src.h > 0)
            SDL_RenderCopy(r, tex, &s.src, &s.dst);
    }
}
