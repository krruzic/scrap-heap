#include "game_context.h"
#include "renderer.h"
#include "input.h"
#include "data.h"
#include "components.h"
#include "stage.h"
#include "powerup.h"

namespace ScrapHeap {

void GameContext::changeState(GameState newState) {
    nextState = newState;
    stateChanged = true;
}

void GameContext::processStateChange() {
    if (!stateChanged) return;
    stateChanged = false;

    state = nextState;

    // Enter new state
    switch (state) {
        case GameState::MainMenu:
            mainMenu.enter();
            break;
        case GameState::Stats:
            statsScreen.enter();
            break;
        case GameState::Tags:
            tagsScreen.enter();
            break;
        case GameState::GameSetup:
            gameSetup.enter();
            break;
        case GameState::StageSelect:
            // Already entered via gameSetup
            break;
        case GameState::Battle:
            // Already created via stageSelect
            break;
    }
}

bool initializeContext(GameContext& ctx) {
    // Initialize SDL
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) {
        SDL_Log("Failed to initialize SDL: %s", SDL_GetError());
        return false;
    }

    // Create window
    ctx.window = SDL_CreateWindow(
        "SCRAP HEAP",
        WINDOW_WIDTH, WINDOW_HEIGHT,
        SDL_WINDOW_RESIZABLE
    );
    if (!ctx.window) {
        SDL_Log("Failed to create window: %s", SDL_GetError());
        return false;
    }

    // Create renderer
    ctx.renderer = SDL_CreateRenderer(ctx.window, nullptr);
    if (!ctx.renderer) {
        SDL_Log("Failed to create renderer: %s", SDL_GetError());
        return false;
    }

    SDL_SetRenderVSync(ctx.renderer, 1);

    // Initialize renderer (loads fonts)
    if (!Renderer::instance().initialize(ctx.renderer, "assets/font.ttf")) {
        SDL_Log("Failed to initialize renderer");
        return false;
    }

    // Initialize input
    InputManager::instance().initialize();

    // Load data
    DataManager::instance().loadAll();

    // Initialize components
    ComponentRegistry::instance().initialize();
    StageRegistry::instance().initialize();
    PowerupManager::instance().initialize();

    // Set initial state
    ctx.state = GameState::MainMenu;
    ctx.running = true;
    ctx.mainMenu.enter();

    return true;
}

void shutdownContext(GameContext& ctx) {
    // Save data
    DataManager::instance().saveAll();

    // Shutdown systems
    InputManager::instance().shutdown();
    Renderer::instance().shutdown();

    // Destroy SDL objects
    if (ctx.renderer) {
        SDL_DestroyRenderer(ctx.renderer);
        ctx.renderer = nullptr;
    }
    if (ctx.window) {
        SDL_DestroyWindow(ctx.window);
        ctx.window = nullptr;
    }

    SDL_Quit();
}

void handleEvents(GameContext& ctx) {
    auto& input = InputManager::instance();
    input.beginFrame();

    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_EVENT_QUIT) {
            ctx.running = false;
            return;
        }

        input.processEvent(event);
    }
}

void update(GameContext& ctx, float dt) {
    ctx.processStateChange();

    switch (ctx.state) {
        case GameState::MainMenu:
            ctx.mainMenu.handleInput(ctx);
            ctx.mainMenu.update(dt);
            break;
        case GameState::Stats:
            ctx.statsScreen.handleInput(ctx);
            ctx.statsScreen.update(dt);
            break;
        case GameState::Tags:
            ctx.tagsScreen.handleInput(ctx);
            ctx.tagsScreen.update(dt);
            break;
        case GameState::GameSetup:
            ctx.gameSetup.handleInput(ctx);
            ctx.gameSetup.update(dt);
            break;
        case GameState::StageSelect:
            ctx.stageSelect.handleInput(ctx);
            ctx.stageSelect.update(dt);
            break;
        case GameState::Battle:
            if (ctx.battle) {
                BattleManager::handleInput(*ctx.battle, ctx);
                BattleManager::update(*ctx.battle, ctx, dt);
            }
            break;
    }
}

void render(GameContext& ctx) {
    auto& renderer = Renderer::instance();
    renderer.clear();

    switch (ctx.state) {
        case GameState::MainMenu:
            ctx.mainMenu.render();
            break;
        case GameState::Stats:
            ctx.statsScreen.render();
            break;
        case GameState::Tags:
            ctx.tagsScreen.render();
            break;
        case GameState::GameSetup:
            ctx.gameSetup.render();
            break;
        case GameState::StageSelect:
            ctx.stageSelect.render();
            break;
        case GameState::Battle:
            if (ctx.battle) {
                BattleManager::render(*ctx.battle);
            }
            break;
    }

    renderer.present();
}

void gameLoop(GameContext& ctx, float dt) {
    handleEvents(ctx);
    if (!ctx.running) return;

    update(ctx, dt);
    render(ctx);
}

} // namespace ScrapHeap
