#pragma once

#include "bot.h"
#include "stage.h"
#include "combat.h"
#include <vector>
#include <string>
#include <memory>

namespace ScrapHeap {

// Forward declarations
struct GameContext;
struct PlayerSlot;

// Game mode result
enum class GameModeResult {
    InProgress,
    Finished
};

// Base interface for all game modes
class IGameMode {
public:
    virtual ~IGameMode() = default;

    // Initialize the game mode with player configurations
    virtual void initialize(const PlayerSlot* slots, int stageIndex) = 0;

    // Update game state
    virtual void update(GameContext& ctx, float dt) = 0;

    // Handle input
    virtual void handleInput(GameContext& ctx) = 0;

    // Render the game
    virtual void render() = 0;

    // Check if the game mode is finished
    virtual GameModeResult getResult() const = 0;

    // Get winner info (if applicable)
    virtual int getWinnerIndex() const = 0;
    virtual std::string getWinnerName() const = 0;

    // Get bots (for external access)
    virtual const std::vector<Bot>& getBots() const = 0;
    virtual std::vector<Bot>& getBots() = 0;

    // Get match timer
    virtual float getMatchTimer() const = 0;

    // Check if paused
    virtual bool isPaused() const = 0;
    virtual void setPaused(bool paused) = 0;
};

// Factory for creating game modes
class GameModeFactory {
public:
    enum class ModeType {
        Arena,          // Standard last-bot-standing
        TeamBattle,     // 2v2 team battle (future)
        KingOfTheHill,  // Control point mode (future)
        Survival        // Waves of enemies (future)
    };

    static std::unique_ptr<IGameMode> create(ModeType type);
};

} // namespace ScrapHeap
