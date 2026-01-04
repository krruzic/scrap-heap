#pragma once

#include "bot.h"
#include <vector>
#include <map>

namespace ScrapHeap {

// Per-bot tracking for stats
struct BotBattleStats {
    int kills = 0;
    int deaths = 0;
    float damageDealt = 0.0f;
    float damageTaken = 0.0f;
};

// Kill popup animation (+1 that fades)
struct KillPopup {
    int playerIndex;      // Which player got the kill
    float timer;          // Time remaining (starts at 1.0)
    static constexpr float DURATION = 1.0f;

    float getAlpha() const {
        return std::min(1.0f, timer / 0.3f);  // Fade out in last 0.3s
    }
};

// HUD rendering for battle modes
class BattleHUD {
public:
    // Render main battle HUD (health bars, ability indicators)
    static void render(const std::vector<Bot>& bots,
                      const std::map<int, BotBattleStats>& stats,
                      float matchTimer,
                      float maxMatchTime);

    // Render player health bar and status
    static void renderPlayerHUD(const Bot& bot, int position, float matchTimer);

    // Render corner score boxes
    static void renderScoreBoxes(const std::vector<Bot>& bots,
                                 const std::map<int, BotBattleStats>& stats);

    // Render match timer
    static void renderTimer(float matchTimer, float maxMatchTime);

    // Render kill popups
    static void renderKillPopups(const std::vector<KillPopup>& popups,
                                const std::vector<Bot>& bots);

    // Update kill popup timers
    static void updateKillPopups(std::vector<KillPopup>& popups, float dt);
};

} // namespace ScrapHeap
