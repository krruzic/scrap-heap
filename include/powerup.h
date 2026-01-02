#pragma once

#include "utils.h"
#include <SDL3/SDL.h>
#include <string>
#include <vector>

namespace ScrapHeap {

// Forward declarations
struct Bot;
struct BattleState;

// Powerup types
enum class PowerupType {
    HealthPack,     // Restore 30 HP
    SpeedBoost,     // 1.3x speed for 8s
    DamageBoost,    // 1.5x damage for 8s
    Shield,         // Ignore next hit
    CooldownReset   // Reset special cooldown
};

// Powerup definition
struct PowerupDef {
    std::string name;
    PowerupType type;
    SDL_Color color;
    float duration;  // 0 for instant effects
    std::string description;
};

// Active powerup in the arena
struct Powerup {
    PowerupType type;
    float x = 0.0f;
    float y = 0.0f;
    float radius = 20.0f;
    bool active = false;
    float respawnTimer = 0.0f;
    float spawnCountdown = 0.0f;  // Shows countdown before spawn
    int spawnPointIndex = 0;
};

// Powerup manager
class PowerupManager {
public:
    static PowerupManager& instance();

    void initialize();

    const std::vector<PowerupDef>& getDefinitions() const { return definitions; }
    const PowerupDef& getDefinition(PowerupType type) const;

    // Create powerups for a stage
    std::vector<Powerup> createPowerupsForStage(int stageIndex) const;

    // Update powerups (timers, respawns)
    void update(std::vector<Powerup>& powerups, float dt);

    // Check if bot collects a powerup
    void checkCollection(std::vector<Powerup>& powerups, Bot& bot);

    // Apply powerup effect to bot
    void applyPowerup(PowerupType type, Bot& bot);

    // Apply held powerup when X/Y pressed
    void useHeldPowerup(Bot& bot);

private:
    PowerupManager() = default;

    std::vector<PowerupDef> definitions;

    // Spawn timing
    static constexpr float FIRST_SPAWN_DELAY = 10.0f;
    static constexpr float RESPAWN_DELAY = 15.0f;
    static constexpr float SPAWN_COUNTDOWN_VISIBLE = 3.0f;
};

} // namespace ScrapHeap
