#pragma once

#include "bot.h"
#include "stage.h"
#include "powerup.h"
#include "combat.h"
#include <vector>
#include <string>

namespace ScrapHeap {

// Forward declarations
struct GameContext;
struct PlayerSlot;

// Battle result
enum class BattleResult {
    InProgress,
    Winner,     // Single winner
    Draw        // Multiple survivors or timeout
};

// Battle state
struct BattleState {
    // Bots in the arena
    std::vector<Bot> bots;

    // Stage
    int stageIndex = 0;
    StageDef stage;

    // Powerups
    std::vector<Powerup> powerups;

    // Special ability objects
    std::vector<Mine> mines;
    std::vector<SmokeCloud> smokeClouds;

    // Combat events for visual feedback
    std::vector<CombatEvent> combatEvents;

    // Timer
    float matchTimer = 0.0f;
    float maxMatchTime = 180.0f;  // 3 minutes

    // Game over state
    BattleResult result = BattleResult::InProgress;
    int winnerIndex = -1;
    float gameOverTimer = 0.0f;
    static constexpr float GAME_OVER_DELAY = 3.0f;

    // Pause state
    bool paused = false;

    // Camera offset for rendering (centers the stage)
    float cameraOffsetX = 0.0f;
    float cameraOffsetY = 0.0f;

    // Check if battle is over
    bool isGameOver() const { return result != BattleResult::InProgress; }

    // Count alive bots
    int countAliveBots() const;

    // Get winner name
    std::string getWinnerName() const;
};

// Battle manager
class BattleManager {
public:
    // Initialize battle with player configurations
    static BattleState createBattle(const PlayerSlot* slots, int stageIndex);

    // Update battle state
    static void update(BattleState& state, GameContext& ctx, float dt);

    // Handle input for battle
    static void handleInput(BattleState& state, GameContext& ctx);

    // Render battle
    static void render(const BattleState& state);

    // Check for game over conditions
    static void checkGameOver(BattleState& state);

    // Record match results to stats
    static void recordResults(const BattleState& state);

private:
    // Update all bots
    static void updateBots(BattleState& state, float dt);

    // Update combat events
    static void updateCombatEvents(BattleState& state, float dt);

    // Update mines
    static void updateMines(BattleState& state, float dt);

    // Update smoke clouds
    static void updateSmokeClouds(BattleState& state, float dt);

    // Render HUD
    static void renderHUD(const BattleState& state);

    // Render game over overlay
    static void renderGameOver(const BattleState& state);
};

} // namespace ScrapHeap
