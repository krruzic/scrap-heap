#pragma once

#include "menu.h"
#include "game_setup.h"
#include "battle.h"
#include <SDL3/SDL.h>
#include <memory>

namespace ScrapHeap {

// Game states
enum class GameState {
    MainMenu,
    Stats,
    Tags,
    GameSetup,
    StageSelect,
    Battle
};

// Main game context
struct GameContext {
    // SDL handles
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;

    // Game state
    GameState state = GameState::MainMenu;
    GameState nextState = GameState::MainMenu;
    bool stateChanged = false;
    bool running = true;

    // Screen objects
    MainMenuScreen mainMenu;
    StatsScreen statsScreen;
    TagsScreen tagsScreen;
    GameSetupScreen gameSetup;
    StageSelectScreen stageSelect;

    // Battle state (only valid during Battle state)
    std::unique_ptr<BattleState> battle;

    // Transition to new state
    void changeState(GameState newState);

    // Process state change
    void processStateChange();
};

// Initialize game context
bool initializeContext(GameContext& ctx);

// Shutdown game context
void shutdownContext(GameContext& ctx);

// Main game loop iteration
void gameLoop(GameContext& ctx, float dt);

// Handle events
void handleEvents(GameContext& ctx);

// Update game state
void update(GameContext& ctx, float dt);

// Render current state
void render(GameContext& ctx);

} // namespace ScrapHeap
