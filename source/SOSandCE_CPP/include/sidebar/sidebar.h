#pragma once
#include "core/common.h"

class App;
class GameState;
struct InputState;

class Sidebar {
public:
    Sidebar();

    void handleInput(App& app, const InputState& input);
    void update(App& app, float dt);
    void render(SDL_Renderer* r, App& app);


    std::string activeTab = "political";
    std::string selectedCountry;
    std::string overlayTitle;
    std::vector<std::string> overlayLines;
    Color overlayAccent = {220, 196, 130};
    std::string currentlyBuilding = "civilian_factory";
    bool leftSide = false;
    float scrollY = 0;
    float maxScroll = 0;
    float animation = 0;
    bool isOpen() const { return animation > 0.01f; }
    bool isLeft() const { return leftSide; }
    void toggle();
    void open();
    void close();
    void clearOverlayInfo();
    void setOverlayInfo(const std::string& title, const std::vector<std::string>& lines, Color accent = {220, 196, 130});
    bool showsPlayerTabs(const GameState& gs) const;


    float getWidth(int screenW) const;
    float getX(int screenW) const;

private:
    float targetAnimation_ = 0;
    float sideBarSize_ = 0.27f;


    float renderPoliticalTab(SDL_Renderer* r, App& app, float x, float y, float w);
    float renderMilitaryTab(SDL_Renderer* r, App& app, float x, float y, float w);
    float renderIndustryTab(SDL_Renderer* r, App& app, float x, float y, float w);
    float renderSelfInfo(SDL_Renderer* r, App& app, float x, float y, float w);
    float renderCountryInfo(SDL_Renderer* r, App& app, float x, float y, float w);
    float renderForeignTab(SDL_Renderer* r, App& app, float x, float y, float w);
    float renderOverlayInfo(SDL_Renderer* r, App& app, float x, float y, float w);
};
