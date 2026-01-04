#pragma once

#include "bot.h"
#include "utils.h"
#include <vector>

namespace ScrapHeap {

struct CombatEvent;

// Common weapon helper functions
namespace WeaponHelpers {
    // Check if attacker can use weapon (not EMP'd, not on cooldown)
    bool canUseWeapon(const Bot& attacker);

    // Check if target is in front arc (dot product threshold)
    bool isInFrontArc(const Bot& attacker, const Bot& target, float threshold = 0.5f);

    // Check if target is in side arc
    bool isInSideArc(const Bot& attacker, const Bot& target, float threshold = 0.7f);

    // Check if target is behind
    bool isBehind(const Bot& attacker, const Bot& target);

    // Get direction from attacker to target (normalized)
    Vec2 getDirectionTo(const Bot& from, const Bot& to);

    // Apply damage multipliers (damage boost, overdrive)
    float applyDamageMultipliers(const Bot& attacker, float baseDamage);

    // Get attacker's current speed
    float getSpeed(const Bot& bot);

    // Get attacker's angular speed (absolute)
    float getAngularSpeed(const Bot& bot);
}

// Weapon processor functions - each handles one weapon type
namespace Weapons {
    void processSpinner(Bot& attacker, Bot& target, std::vector<CombatEvent>& events, float dt);
    void processClamp(Bot& attacker, Bot& target, std::vector<CombatEvent>& events, float dt);
    void processHammer(Bot& attacker, std::vector<Bot>& bots, std::vector<CombatEvent>& events, float dt);
    void processBatteringRam(Bot& attacker, Bot& target, std::vector<CombatEvent>& events);
    void processFlail(Bot& attacker, Bot& target, std::vector<CombatEvent>& events, float dt);
    void processWhip(Bot& attacker, std::vector<Bot>& bots, std::vector<CombatEvent>& events, float dt);
    void processSawBlade(Bot& attacker, Bot& target, std::vector<CombatEvent>& events, float dt);
    void processThwackBar(Bot& attacker, Bot& target, std::vector<CombatEvent>& events, float dt);
    void processPistonPunch(Bot& attacker, std::vector<Bot>& bots, std::vector<CombatEvent>& events, float dt);
    void processDualSpinners(Bot& attacker, Bot& target, std::vector<CombatEvent>& events, float dt);

    // Constants
    constexpr float SPINNER_TICK_TIME = 0.3f;
}

} // namespace ScrapHeap
