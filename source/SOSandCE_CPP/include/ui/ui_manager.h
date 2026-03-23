#pragma once
#include "core/common.h"
#include "ui/top_bar.h"
#include "ui/tooltip.h"
#include "ui/popup.h"
#include "ui/toast.h"

class Panel;
struct InputState;

class UIManager {
public:
    UIManager();
    ~UIManager();

    TopBar topBar;
    Tooltip tooltip;
    std::vector<Panel*> panels;
    std::vector<std::unique_ptr<Popup>> popupList;

    void addPanel(Panel* p);
    void removePanel(Panel* p);

    void handleInput(const InputState& input);
    void update(float dt);
    void render(SDL_Renderer* r, int screenW, int screenH, float uiSize);

    bool anyPanelOpen() const;
    bool hitUI(int mx, int my) const;

private:
    int nextZ_ = 0;
};
