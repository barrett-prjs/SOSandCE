#include "core/app.h"
#include "core/audio.h"
#include "data/data_loader.h"
#include "data/region_data.h"
#include "data/country_data.h"
#include "game/game_state.h"
#include "screens/screen.h"
#include "screens/main_menu.h"
#include "screens/game_screen.h"
#include "screens/settings_screen.h"
#include "screens/country_select.h"
#include "screens/map_select.h"
#include "screens/peace_screen.h"
#include "screens/save_load.h"
#include "screens/decision_tree.h"
#include "ui/theme.h"
#include "ui/imgui_integration.h"
#include "ui/rmlui_backend.h"
#include "ui/ui_assets.h"

static App* s_instance = nullptr;

static const char* screenTypeName(ScreenType type) {
    switch (type) {
        case ScreenType::NONE: return "NONE";
        case ScreenType::MAIN_MENU: return "MAIN_MENU";
        case ScreenType::SETTINGS: return "SETTINGS";
        case ScreenType::COUNTRY_SELECT: return "COUNTRY_SELECT";
        case ScreenType::MAP_SELECT: return "MAP_SELECT";
        case ScreenType::GAME: return "GAME";
        case ScreenType::PEACE_CONFERENCE: return "PEACE_CONFERENCE";
        case ScreenType::SAVE_LOAD: return "SAVE_LOAD";
        case ScreenType::DECISION_TREE: return "DECISION_TREE";
        case ScreenType::QUIT: return "QUIT";
        default: return "UNKNOWN";
    }
}

App& App::instance() {
    return *s_instance;
}

App::App() {
    s_instance = this;
}

App::~App() {
    screens_.clear(*this);


    if (gameState_) gameState_->clear();
    gameState_.reset();

    RmlBackend::shutdown();
    ImGuiLayer::shutdown();
    Audio::instance().shutdown();
    Engine::instance().shutdown();
    s_instance = nullptr;
}

int App::run() {
    printf("[App] Loading settings...\n"); fflush(stdout);
    settings_.load("settings.json");

    printf("[App] Initializing SDL...\n"); fflush(stdout);
    initSDL();
    if (!running_) return 1;

    printf("[App] Loading assets...\n"); fflush(stdout);
    loadAssets();

    printf("[App] Initializing UI assets...\n"); fflush(stdout);
    UIAssets::instance().init(Engine::instance().assetsPath);

    printf("[App] Creating game state...\n"); fflush(stdout);
    gameState_ = std::make_unique<GameState>();

    printf("[App] Registering screens...\n"); fflush(stdout);

    screens_.registerScreen(ScreenType::MAIN_MENU, std::make_unique<MainMenuScreen>());
    screens_.registerScreen(ScreenType::GAME, std::make_unique<GameScreen>());
    screens_.registerScreen(ScreenType::SETTINGS, std::make_unique<SettingsScreen>());
    screens_.registerScreen(ScreenType::COUNTRY_SELECT, std::make_unique<CountrySelectScreen>());
    screens_.registerScreen(ScreenType::MAP_SELECT, std::make_unique<MapSelectScreen>());
    screens_.registerScreen(ScreenType::PEACE_CONFERENCE, std::make_unique<PeaceScreen>());
    screens_.registerScreen(ScreenType::SAVE_LOAD, std::make_unique<SaveLoadScreen>());
    screens_.registerScreen(ScreenType::DECISION_TREE, std::make_unique<DecisionTreeScreen>());


    bool quickStart = false;

    const char* qs = std::getenv("SOS_QUICKSTART");
    if (qs && std::string(qs) == "1") quickStart = true;

    if (quickStart) {
        const char* qsCountry = std::getenv("SOS_QUICKSTART_COUNTRY");
        const char* qsMap = std::getenv("SOS_QUICKSTART_MAP");
        gameState_->controlledCountry = (qsCountry && *qsCountry) ? qsCountry : "United_States";
        gameState_->mapName = (qsMap && *qsMap) ? qsMap : "Modern Day";
        printf("[App] QUICKSTART: Auto-starting %s as %s\n",
               gameState_->mapName.c_str(),
               gameState_->controlledCountry.c_str());
        fflush(stdout);
        switchScreen(ScreenType::GAME);
    } else {
        printf("[App] Switching to main menu...\n"); fflush(stdout);
        switchScreen(ScreenType::MAIN_MENU);
    }

    printf("[App] Entering main loop...\n"); fflush(stdout);
    mainLoop();


    settings_.save("settings.json");

    return 0;
}

void App::initSDL() {
    auto& engine = Engine::instance();

    int w = settings_.windowWidth;
    int h = settings_.windowHeight;

    if (!engine.init(w, h)) {
        SDL_Log("FATAL: Engine::init failed");
        running_ = false;
        return;
    }

    if (!settings_.fullscreen && engine.window) {
        SDL_SetWindowFullscreen(engine.window, 0);
        SDL_SetWindowSize(engine.window, settings_.windowWidth, settings_.windowHeight);
        SDL_SetWindowPosition(engine.window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
        SDL_GetWindowSize(engine.window, &engine.WIDTH, &engine.HEIGHT);
    }

    engine.FPS = settings_.fps;
    engine.recalcUI(settings_.uiSize);
    Theme::setScale(engine.uiScale);


    ImGuiLayer::init(engine.window, engine.renderer);


    RmlBackend::init(engine.window, engine.renderer);

    Audio::instance().init();
    Audio::instance().setMusicVolume(settings_.musicVolume);
    Audio::instance().setSoundVolume(settings_.soundVolume);
}

void App::loadAssets() {
    auto& engine = Engine::instance();
    std::string base = engine.assetsPath;


    DataLoader::setBasePath(base + "base_data/");


    RegionData::instance().load();
    CountryData::instance().load();


    Audio::instance().loadAllSounds(base + "snd/", settings_.soundVolume);
}

void App::switchScreen(ScreenType type) {
    pendingScreen_ = type;
}

void App::requestGameResetOnTransition() {
    resetGameOnTransition_ = true;
}

Screen* App::currentScreen() const {
    return screens_.current();
}

Screen* App::screen(ScreenType type) const {
    return screens_.get(type);
}

GameState& App::gameState() {
    return *gameState_;
}

void App::mainLoop() {
    auto& engine = Engine::instance();
    lastFrameTime_ = SDL_GetTicks();

    while (running_) {
        uint32_t now = SDL_GetTicks();
        float dt = (now - lastFrameTime_) / 1000.0f;
        dt = std::min(dt, 0.1f);
        lastFrameTime_ = now;


        if (pendingScreen_ != ScreenType::NONE) {
            if (pendingScreen_ == ScreenType::QUIT) {
                running_ = false;
                break;
            }
            if (resetGameOnTransition_) {
                resetGameSession();
                resetGameOnTransition_ = false;
            }
            screens_.switchTo(pendingScreen_, *this);
            pendingScreen_ = ScreenType::NONE;
        }


        input_.beginFrame();
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            ImGuiLayer::processEvent(e);
            RmlBackend::processEvent(e);
            input_.processEvent(e);
        }
        input_.endFrame();

        if (input_.quit) {
            running_ = false;
            break;
        }

        if (input_.windowResized) {
            handleResize(input_.newWidth, input_.newHeight);
        }


        ImGuiLayer::newFrame();


        if (screens_.current()) {
            screens_.handleInput(*this, input_);
            screens_.update(*this, dt);

            engine.clear({15, 17, 20});
            screens_.render(*this);


            RmlBackend::endFrame();


            ImGuiLayer::render();
            engine.present();
        }


        if (screens_.current() && screens_.current()->nextScreen != ScreenType::NONE) {
            pendingScreen_ = screens_.current()->nextScreen;
            screens_.current()->nextScreen = ScreenType::NONE;
        }


        uint32_t frameTime = SDL_GetTicks() - now;
        uint32_t targetFrame = 1000 / engine.FPS;
        if (frameTime < targetFrame) {
            SDL_Delay(targetFrame - frameTime);
        }
    }
}

void App::handleResize(int w, int h) {
    auto& engine = Engine::instance();
    engine.WIDTH = w;
    engine.HEIGHT = h;
    engine.recalcUI(settings_.uiSize);
    Theme::setScale(engine.uiScale);
    settings_.windowWidth = w;
    settings_.windowHeight = h;
}

void App::resetGameSession() {
    if (auto* gameScreen = dynamic_cast<GameScreen*>(screen(ScreenType::GAME))) {
        gameScreen->resetSessionState();
    }
    if (gameState_) {
        gameState_->clear();
    }
}


void ScreenManager::registerScreen(ScreenType type, std::unique_ptr<Screen> screen) {
    screens_[static_cast<int>(type)] = std::move(screen);
}

void ScreenManager::switchTo(ScreenType type, App& app) {
    if (current_) {
        current_->nextScreen = ScreenType::NONE;
        current_->exit(app);
    }
    auto it = screens_.find(static_cast<int>(type));
    if (it != screens_.end()) {
        printf("[ScreenManager] switchTo %s\n", screenTypeName(type));
        fflush(stdout);
        current_ = it->second.get();
        currentType_ = type;
        current_->nextScreen = ScreenType::NONE;
        current_->enter(app);
    } else {
        current_ = nullptr;
        currentType_ = ScreenType::NONE;
    }
}

void ScreenManager::update(App& app, float dt) {
    if (current_) current_->update(app, dt);
}

void ScreenManager::render(App& app) {
    if (current_) current_->render(app);
}

void ScreenManager::handleInput(App& app, const InputState& input) {
    if (current_) current_->handleInput(app, input);
}

void ScreenManager::clear(App& app) {
    if (current_) {
        current_->exit(app);
    }
    current_ = nullptr;
    currentType_ = ScreenType::NONE;
    screens_.clear();
}

Screen* ScreenManager::get(ScreenType type) const {
    auto it = screens_.find(static_cast<int>(type));
    if (it != screens_.end()) {
        return it->second.get();
    }
    return nullptr;
}
