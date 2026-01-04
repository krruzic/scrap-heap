#pragma once

#include "game_mode.h"
#include "battle_hud.h"
#include "storm.h"
#include "powerup.h"
#include <map>

namespace ScrapHeap {

// Battle result for arena mode
enum class ArenaResult {
    InProgress,
    Winner,     // Single winner
    Draw        // Multiple survivors or timeout
};

// Arena mode - standard last-bot-standing battle
class ArenaMode : public IGameMode {
public:
    ArenaMode() = default;
    ~ArenaMode() override = default;

    // IGameMode implementation
    void initialize(const PlayerSlot* slots, int stageIndex) override;
    void update(GameContext& ctx, float dt) override;
    void handleInput(GameContext& ctx) override;
    void render() override;

    GameModeResult getResult() const override;
    int getWinnerIndex() const override { return winnerIndex_; }
    std::string getWinnerName() const override;

    const std::vector<Bot>& getBots() const override { return bots_; }
    std::vector<Bot>& getBots() override { return bots_; }

    float getMatchTimer() const override { return matchTimer_; }
    bool isPaused() const override { return paused_; }
    void setPaused(bool paused) override { paused_ = paused; }

    // Arena-specific accessors
    const Storm& getStorm() const { return storm_; }
    const std::map<int, BotBattleStats>& getBotStats() const { return botStats_; }
    ArenaResult getArenaResult() const { return result_; }
    float getGameOverTimer() const { return gameOverTimer_; }
    bool isResultsConfirmed() const { return resultsConfirmed_; }

private:
    // Core state
    std::vector<Bot> bots_;
    std::map<int, BotBattleStats> botStats_;
    int stageIndex_ = 0;
    StageDef stage_;

    // Powerups and objects
    std::vector<Powerup> powerups_;
    std::vector<Mine> mines_;
    std::vector<SmokeCloud> smokeClouds_;

    // Storm
    Storm storm_;

    // Combat events
    std::vector<CombatEvent> combatEvents_;
    std::vector<KillPopup> killPopups_;

    // Timer
    float matchTimer_ = 0.0f;
    float maxMatchTime_ = 180.0f;

    // Result state
    ArenaResult result_ = ArenaResult::InProgress;
    int winnerIndex_ = -1;
    float gameOverTimer_ = 0.0f;
    bool playersConfirmed_[4] = {false, false, false, false};
    bool resultsConfirmed_ = false;

    // Pause state
    bool paused_ = false;

    // Camera
    float cameraOffsetX_ = 0.0f;
    float cameraOffsetY_ = 0.0f;

    // Update methods
    void updateBots(float dt);
    void updateCombatEvents(float dt);
    void updateMines(float dt);
    void updateSmokeClouds(float dt);
    void checkGameOver();
    void handleWinScreenInput(GameContext& ctx);
    void recordResults(GameContext& ctx);

    // Render methods
    void renderArena();
    void renderGameOver();

    // Count alive bots
    int countAliveBots() const;
};

} // namespace ScrapHeap
