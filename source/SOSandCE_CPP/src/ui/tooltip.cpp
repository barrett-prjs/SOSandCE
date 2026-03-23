#include "ui/tooltip.h"
#include "ui/primitives.h"
#include "ui/theme.h"
#include "ui/ui_assets.h"


void Tooltip::show(const std::vector<std::string>& lines, float x, float y) {
    lines_ = lines;
    x_ = x;
    y_ = y;
    visible_ = true;
}

void Tooltip::hide() {
    visible_ = false;
}

void Tooltip::render(SDL_Renderer* r, int screenW, int screenH) {
    if (!visible_ || lines_.empty()) return;

    float padding = Theme::s(0.5f);
    float lineH = Theme::s(1.2f);
    int textSize = Theme::si(0.8f);


    float maxW = 0;
    for (auto& line : lines_) {
        float tw = static_cast<float>(UIPrim::textWidth(line, textSize));
        if (tw > maxW) maxW = tw;
    }

    float tipW = maxW + padding * 2;
    float tipH = lines_.size() * lineH + padding * 2;


    float tipX = x_ + Theme::s(1.0f);
    float tipY = y_ + Theme::s(1.0f);


    if (tipX + tipW > screenW) tipX = screenW - tipW - 2;
    if (tipY + tipH > screenH) tipY = screenH - tipH - 2;
    if (tipX < 0) tipX = 2;
    if (tipY < 0) tipY = 2;


    UIPrim::drawShadow(r, tipX, tipY, tipW, tipH, 60, 6);


    auto& assets = UIAssets::instance();
    SDL_Texture* tipBg = assets.panelBodyHeadless();
    if (tipBg) {
        UIAssets::draw9Slice(r, tipBg, tipX, tipY, tipW, tipH, 10);
    } else {
        UIPrim::drawRectFilled(r, Theme::bg_dark, tipX, tipY, tipW, tipH);
        UIPrim::drawRect(r, Theme::border, tipX, tipY, tipW, tipH, Theme::border, 0, 1);
    }


    UIPrim::drawHLine(r, Theme::gold_dim, tipX + 4, tipX + tipW - 4, tipY + 2, 1);


    float drawY = tipY + padding;
    for (size_t i = 0; i < lines_.size(); i++) {
        Color lineColor = (i == 0) ? Theme::gold_bright : Theme::grey;
        UIPrim::drawText(r, lines_[i], textSize,
                         tipX + padding, drawY,
                         "topleft", lineColor, i == 0);
        drawY += lineH;
    }
}


void RegionTooltip::showAt(float x, float y, const std::string& header,
                            const std::vector<std::pair<std::string, Color>>& lines,
                            SDL_Texture* flagTex, int screenW, int screenH) {
    header_ = header;
    lines_ = lines;
    flagTex_ = flagTex;
    visible_ = true;

    float padding = Theme::s(0.6f);
    float lineH = Theme::s(1.3f);
    int textSize = Theme::si(0.85f);
    int headerSize = Theme::si(1.0f);


    float maxW = static_cast<float>(UIPrim::textWidth(header, headerSize));
    for (auto& [text, color] : lines) {
        float tw = static_cast<float>(UIPrim::textWidth(text, textSize));
        if (tw > maxW) maxW = tw;
    }


    float flagSpace = flagTex ? Theme::s(2.5f) : 0;

    w_ = maxW + padding * 2 + flagSpace;
    h_ = padding * 2 + Theme::s(1.8f) + lines.size() * lineH;


    x_ = x + Theme::s(1.0f);
    y_ = y + Theme::s(0.5f);


    if (x_ + w_ > screenW) x_ = screenW - w_ - 2;
    if (y_ + h_ > screenH) y_ = screenH - h_ - 2;
    if (x_ < 0) x_ = 2;
    if (y_ < 0) y_ = 2;
}

void RegionTooltip::render(SDL_Renderer* r) {
    if (!visible_) return;

    float padding = Theme::s(0.6f);
    float lineH = Theme::s(1.3f);
    int textSize = Theme::si(0.85f);
    int headerSize = Theme::si(1.0f);


    UIPrim::drawShadow(r, x_, y_, w_, h_, 60, 8);


    auto& assets = UIAssets::instance();
    SDL_Texture* tipBg = assets.panelBodyHeadless();
    if (tipBg) {
        UIAssets::draw9Slice(r, tipBg, x_, y_, w_, h_, 12);
    } else {
        UIPrim::drawRectFilled(r, Theme::bg_dark, x_, y_, w_, h_);
        UIPrim::drawRect(r, Theme::border, x_, y_, w_, h_, Theme::border, 0, 1);
    }


    UIPrim::drawHLine(r, Theme::gold, x_ + 4, x_ + w_ - 4, y_ + 2, 2);

    float drawX = x_ + padding;
    float drawY = y_ + padding;


    if (flagTex_) {
        float flagW = Theme::s(2.0f);
        float flagH = Theme::s(1.4f);
        SDL_Rect flagRect = {
            static_cast<int>(drawX),
            static_cast<int>(drawY),
            static_cast<int>(flagW),
            static_cast<int>(flagH)
        };
        SDL_RenderCopy(r, flagTex_, nullptr, &flagRect);
        drawX += flagW + Theme::s(0.4f);
    }


    UIPrim::drawText(r, header_, headerSize, drawX, drawY, "topleft",
                     Theme::gold_bright, true);
    drawY += Theme::s(1.8f);


    drawX = x_ + padding;


    for (auto& [text, color] : lines_) {
        Color lineColor = (color.a == 0) ? Theme::grey : color;
        UIPrim::drawText(r, text, textSize, drawX, drawY, "topleft", lineColor);
        drawY += lineH;
    }
}

bool RegionTooltip::containsPoint(float px, float py) const {
    return visible_ && px >= x_ && px <= x_ + w_ && py >= y_ && py <= y_ + h_;
}
