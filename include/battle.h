#pragma once

#include "bot.h"
#include "stage.h"
#include "powerup.h"
#include "combat.h"
#include <vector>
#include <string>
#include <map>
#include <algorithm>

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

// Shrinking wall (storm) state
struct ShrinkingWall {
    // Wall boundaries (safe zone is inside these)
    float left = 0.0f;
    float right = 0.0f;
    float top = 0.0f;
    float bottom = 0.0f;

    // Target boundaries (what we're shrinking towards)
    float targetLeft = 0.0f;
    float targetRight = 0.0f;
    float targetTop = 0.0f;
    float targetBottom = 0.0f;

    // Shrink phases
    int currentPhase = 0;
    float phaseTimer = 0.0f;

    // Wall properties
    bool active = false;
    float damagePerSecond = 5.0f;

    // Animation state for electrical effect
    float animTimer = 0.0f;
    float pulseTimer = 0.0f;
    float arcOffsets[16] = {0};  // Random offsets for lightning arcs

    // Timing constants
    static constexpr float INITIAL_DELAY = 30.0f;       // Wait 30s before wall appears
    static constexpr float PHASE_DURATION = 20.0f;      // Each phase lasts 20s
    static constexpr float FINAL_RADIUS = 60.0f;        // Minimum safe zone radius
    static constexpr int MAX_PHASES = 5;

    // Get current shrink speed (faster in later phases)
    float getShrinkSpeed() const {
        // Starts slow, gets faster each phase - 3x faster than before
        return (0.5f + currentPhase * 0.4f) * 3.0f;
    }

    // Reset storm to stage boundaries
    void reset(float stageWidth, float stageHeight) {
        left = 0.0f;
        right = stageWidth;
        top = 0.0f;
        bottom = stageHeight;
        currentPhase = 0;
        phaseTimer = 0.0f;
        active = false;
        damagePerSecond = 5.0f;

        // Set new targets
        float shrinkAmount = stageWidth * 0.15f;
        targetLeft = shrinkAmount;
        targetRight = stageWidth - shrinkAmount;
        targetTop = shrinkAmount;
        targetBottom = stageHeight - shrinkAmount;
    }
};

// BotBattleStats and KillPopup are defined in battle_hud.h (included via combat.h)

// Battle state
struct BattleState {
    // Bots in the arena
    std::vector<Bot> bots;

    // Per-bot stats tracking
    std::map<int, BotBattleStats> botStats;

    // Stage
    int stageIndex = 0;
    StageDef stage;

    // Powerups
    std::vector<Powerup> powerups;

    // Special ability objects
    std::vector<Mine> mines;
    std::vector<SmokeCloud> smokeClouds;

    // Shrinking wall
    ShrinkingWall wall;

    // Combat events for visual feedback
    std::vector<CombatEvent> combatEvents;

    // Kill popups (+1 animations)
    std::vector<KillPopup> killPopups;

    // Timer
    float matchTimer = 0.0f;
    float maxMatchTime = 180.0f;  // 3 minutes

    // Game over state
    BattleResult result = BattleResult::InProgress;
    int winnerIndex = -1;
    float gameOverTimer = 0.0f;
    bool playersConfirmed[4] = {false, false, false, false};  // Track A button presses
    bool resultsConfirmed = false;  // All players confirmed or Start pressed

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

    // Check if a position is outside the safe zone
    bool isOutsideSafeZone(float x, float y) const;
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
    static void recordResults(const BattleState& state, GameContext& ctx);

private:
    // Update all bots
    static void updateBots(BattleState& state, float dt);

    // Update combat events
    static void updateCombatEvents(BattleState& state, float dt);

    // Update mines
    static void updateMines(BattleState& state, float dt);

    // Update smoke clouds
    static void updateSmokeClouds(BattleState& state, float dt);

    // Update kill popups
    static void updateKillPopups(BattleState& state, float dt);

    // Update shrinking wall
    static void updateShrinkingWall(BattleState& state, float dt);

    // Apply wall damage to bots outside safe zone
    static void applyWallDamage(BattleState& state, float dt);

    // Render shrinking wall
    static void renderShrinkingWall(const BattleState& state);

    // Render game over overlay
    static void renderGameOver(const BattleState& state);

    // Handle win screen input
    static void handleWinScreenInput(BattleState& state, GameContext& ctx);
};

} // namespace ScrapHeap
