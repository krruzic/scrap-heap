#include "powerup.h"
#include "bot.h"
#include "stage.h"
#include <cstdlib>

namespace ScrapHeap {

PowerupManager& PowerupManager::instance() {
    static PowerupManager manager;
    return manager;
}

void PowerupManager::initialize() {
    definitions = {
        {"Health Pack", PowerupType::HealthPack, {80, 255, 80, 255}, 0.0f, "Restore 30 HP"},
        {"Speed Boost", PowerupType::SpeedBoost, {255, 255, 80, 255}, 8.0f, "1.3x speed for 8s"},
        {"Damage Boost", PowerupType::DamageBoost, {255, 80, 80, 255}, 8.0f, "1.5x damage for 8s"},
        {"Shield", PowerupType::Shield, {80, 150, 255, 255}, 0.0f, "Ignore next hit"},
        {"Cooldown Reset", PowerupType::CooldownReset, {200, 80, 255, 255}, 0.0f, "Reset special cooldown"}
    };
}

const PowerupDef& PowerupManager::getDefinition(PowerupType type) const {
    int index = static_cast<int>(type);
    if (index < 0 || index >= static_cast<int>(definitions.size())) {
        return definitions[0];
    }
    return definitions[index];
}

std::vector<Powerup> PowerupManager::createPowerupsForStage(int stageIndex) const {
    const auto& stage = StageRegistry::instance().getStage(stageIndex);
    std::vector<Powerup> powerups;

    for (int i = 0; i < static_cast<int>(stage.powerupSpawns.size()); ++i) {
        Powerup powerup;
        powerup.type = static_cast<PowerupType>(std::rand() % 5);
        powerup.x = stage.powerupSpawns[i].x;
        powerup.y = stage.powerupSpawns[i].y;
        powerup.radius = 20.0f;
        powerup.active = false;
        powerup.respawnTimer = FIRST_SPAWN_DELAY;
        powerup.spawnCountdown = 0.0f;
        powerup.spawnPointIndex = i;
        powerups.push_back(powerup);
    }

    return powerups;
}

void PowerupManager::update(std::vector<Powerup>& powerups, float dt) {
    for (auto& powerup : powerups) {
        if (!powerup.active) {
            powerup.respawnTimer -= dt;

            // Show countdown for last 3 seconds
            if (powerup.respawnTimer <= SPAWN_COUNTDOWN_VISIBLE) {
                powerup.spawnCountdown = powerup.respawnTimer;
            }

            if (powerup.respawnTimer <= 0.0f) {
                powerup.active = true;
                // Randomize type on respawn
                powerup.type = static_cast<PowerupType>(std::rand() % 5);
            }
        }
    }
}

void PowerupManager::checkCollection(std::vector<Powerup>& powerups, Bot& bot) {
    if (!bot.isAlive) return;

    for (auto& powerup : powerups) {
        if (!powerup.active) continue;

        float dist = distance(bot.x, bot.y, powerup.x, powerup.y);
        if (dist < bot.radius + powerup.radius) {
            // Collected!
            applyPowerup(powerup.type, bot);
            powerup.active = false;
            powerup.respawnTimer = RESPAWN_DELAY;
            powerup.spawnCountdown = 0.0f;
        }
    }
}

void PowerupManager::applyPowerup(PowerupType type, Bot& bot) {
    switch (type) {
        case PowerupType::HealthPack:
            bot.health = std::min(bot.health + 30.0f, bot.maxHealth);
            break;

        case PowerupType::SpeedBoost:
            bot.speedBoostTimer = 8.0f;
            break;

        case PowerupType::DamageBoost:
            bot.damageBoostTimer = 8.0f;
            break;

        case PowerupType::Shield:
            bot.shieldActive = true;
            break;

        case PowerupType::CooldownReset:
            bot.specialCooldown = 0.0f;
            break;
    }
}

void PowerupManager::useHeldPowerup(Bot& bot) {
    if (bot.heldPowerup < 0) return;

    applyPowerup(static_cast<PowerupType>(bot.heldPowerup), bot);
    bot.heldPowerup = -1;
}

} // namespace ScrapHeap
