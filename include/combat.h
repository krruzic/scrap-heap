#pragma once

#include "bot.h"
#include "stage.h"
#include "battle_hud.h"
#include <vector>
#include <map>

namespace ScrapHeap {

// Forward declarations
struct BattleState;

// Combat event for visual feedback
struct CombatEvent {
    enum class Type {
        Damage,
        Impact,      // Spark effect at collision point
        Grab,
        GrabEscape,
        SpecialActivate,
        PowerupCollect,
        Death
    };

    Type type;
    float x, y;
    float value;  // Damage amount, etc.
    int sourceBot = -1;
    int targetBot = -1;
    float timer;  // For visual duration
    bool statsProcessed = false;  // Has this event been counted in stats?
};

// Combat system
class Combat {
public:
    // Process weapon interactions between bots
    static void processWeapons(std::vector<Bot>& bots, const StageDef& stage,
                              std::vector<CombatEvent>& events, float dt);

    // Process special abilities
    static void processSpecials(std::vector<Bot>& bots, std::vector<CombatEvent>& events, float dt);

    // Apply damage to a bot (basic version)
    static void applyDamage(Bot& target, float damage, float knockbackForce,
                           const Vec2& knockbackDir, Bot* source,
                           std::vector<CombatEvent>& events);

    // Apply damage with stats tracking (for use by arena_mode)
    static void applyDamage(Bot& target, float damage, int sourceIndex,
                           std::vector<Bot>& bots, std::vector<CombatEvent>& events,
                           std::map<int, BotBattleStats>& stats,
                           std::vector<KillPopup>& killPopups);

    // Process grab mechanics
    static void processGrabs(std::vector<Bot>& bots, std::vector<CombatEvent>& events, float dt);

    // Process hazard damage
    static void processHazards(std::vector<Bot>& bots, StageDef& stage,
                              std::vector<CombatEvent>& events, float dt);

    // Activate special ability
    static void activateSpecial(Bot& bot, std::vector<Bot>& allBots,
                               std::vector<CombatEvent>& events);

    // Check if bots are in contact
    static bool areInContact(const Bot& a, const Bot& b);

    // Get contact point between bots
    static Vec2 getContactPoint(const Bot& a, const Bot& b);

};

// Mine object for Mine special
struct Mine {
    float x, y;
    float armTimer;  // Time until armed
    bool armed;
    bool exploded;
    int ownerIndex;
    static constexpr float ARM_TIME = 1.0f;
    static constexpr float RADIUS = 25.0f;
    static constexpr float DAMAGE = 40.0f;
    static constexpr float EXPLOSION_RADIUS = 60.0f;
};

// Smoke cloud for Smoke special
struct SmokeCloud {
    float x, y;
    float timer;
    float radius;
    int ownerIndex;
    static constexpr float DURATION = 4.0f;
    static constexpr float MAX_RADIUS = 80.0f;
};

} // namespace ScrapHeap
