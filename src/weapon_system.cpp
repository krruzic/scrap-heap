#include "weapon_system.h"
#include "combat.h"
#include "components.h"
#include "physics.h"
#include <cmath>
#include <algorithm>

namespace ScrapHeap {

// ============================================================================
// Helper Functions
// ============================================================================

bool WeaponHelpers::canUseWeapon(const Bot& attacker) {
    return !attacker.empDisabled && attacker.weaponCooldown <= 0;
}

bool WeaponHelpers::isInFrontArc(const Bot& attacker, const Bot& target, float threshold) {
    Vec2 facing = attacker.getFacingVector();
    Vec2 toTarget = getDirectionTo(attacker, target);
    return facing.dot(toTarget) >= threshold;
}

bool WeaponHelpers::isInSideArc(const Bot& attacker, const Bot& target, float threshold) {
    Vec2 facing = attacker.getFacingVector();
    Vec2 toTarget = getDirectionTo(attacker, target);
    return std::abs(facing.dot(toTarget)) < threshold;
}

bool WeaponHelpers::isBehind(const Bot& attacker, const Bot& target) {
    Vec2 facing = attacker.getFacingVector();
    Vec2 toTarget = getDirectionTo(attacker, target);
    return facing.dot(toTarget) < 0.0f;
}

Vec2 WeaponHelpers::getDirectionTo(const Bot& from, const Bot& to) {
    Vec2 dir(to.x - from.x, to.y - from.y);
    return dir.normalized();
}

float WeaponHelpers::applyDamageMultipliers(const Bot& attacker, float baseDamage) {
    float damage = baseDamage;
    if (attacker.damageBoostTimer > 0) damage *= 1.5f;
    if (attacker.overdriveActive) damage *= 1.5f;
    return damage;
}

float WeaponHelpers::getSpeed(const Bot& bot) {
    return std::sqrt(bot.velX * bot.velX + bot.velY * bot.velY);
}

float WeaponHelpers::getAngularSpeed(const Bot& bot) {
    return std::abs(bot.angularVel);
}

// ============================================================================
// Spinner - Front-mounted spinning disc
// ============================================================================

void Weapons::processSpinner(Bot& attacker, Bot& target,
                             std::vector<CombatEvent>& events, float dt) {
    if (attacker.empDisabled || attacker.weaponCooldown > 0) return;

    const auto& weapon = ComponentRegistry::instance().getWeapon(attacker.weaponIndex);
    const auto& targetWeapon = ComponentRegistry::instance().getWeapon(target.weaponIndex);

    Vec2 toTarget = WeaponHelpers::getDirectionTo(attacker, target);
    if (!WeaponHelpers::isInFrontArc(attacker, target, 0.3f)) return;

    float damage, knockback;
    if (attacker.inputWeapon && attacker.spinnerSpeed > 0.1f) {
        damage = weapon.damage * attacker.spinnerSpeed;
        knockback = weapon.knockback * attacker.spinnerSpeed;
        attacker.spinnerSpeed = 0.0f;
    } else {
        damage = weapon.damage * 0.5f;
        knockback = weapon.knockback * 0.5f;
    }

    damage = WeaponHelpers::applyDamageMultipliers(attacker, damage);

    bool targetHasSpinner = (targetWeapon.name == "Spinner" || targetWeapon.name == "Dual Spinners");

    if (targetHasSpinner && WeaponHelpers::isInFrontArc(target, attacker, 0.3f)) {
        float targetDamage = WeaponHelpers::applyDamageMultipliers(target, targetWeapon.damage * 0.5f);
        Vec2 reverseDir(-toTarget.x, -toTarget.y);

        Combat::applyDamage(target, damage * 0.5f, knockback * 0.5f, toTarget, &attacker, events);
        Combat::applyDamage(attacker, targetDamage * 0.5f, targetWeapon.knockback * 0.25f, reverseDir, &target, events);

        attacker.spinnerSpeed *= 0.6f;
        target.spinnerSpeed *= 0.6f;
    } else {
        Combat::applyDamage(target, damage, knockback, toTarget, &attacker, events);
    }

    attacker.weaponCooldown = SPINNER_TICK_TIME;
}

// ============================================================================
// Clamp - Grabbing weapon
// ============================================================================

void Weapons::processClamp(Bot& attacker, Bot& target,
                           std::vector<CombatEvent>& events, float dt) {
    if (attacker.empDisabled) return;
    if (!attacker.inputWeaponPressed) return;
    if (attacker.grabState != GrabState::None || target.grabState != GrabState::None) return;
    if (!WeaponHelpers::isInFrontArc(attacker, target, 0.5f)) return;

    const auto& weapon = ComponentRegistry::instance().getWeapon(attacker.weaponIndex);

    attacker.grabState = GrabState::Grabbing;
    attacker.grabbingBot = target.playerIndex;
    attacker.grabTimer = weapon.grabDuration;

    target.grabState = GrabState::Grabbed;
    target.grabbedBy = attacker.playerIndex;
    target.grabEscapeProgress = 0.0f;

    CombatEvent event;
    event.type = CombatEvent::Type::Grab;
    event.x = target.x;
    event.y = target.y;
    event.timer = 0.5f;
    events.push_back(event);
}

// ============================================================================
// Hammer - Windup overhead strike
// ============================================================================

void Weapons::processHammer(Bot& attacker, std::vector<Bot>& bots,
                            std::vector<CombatEvent>& events, float dt) {
    if (attacker.empDisabled) return;

    const auto& weapon = ComponentRegistry::instance().getWeapon(attacker.weaponIndex);

    if (attacker.inputWeaponPressed && !attacker.hammerWindingUp && attacker.weaponCooldown <= 0) {
        attacker.hammerWindingUp = true;
        attacker.hammerWindupTimer = 0.8f;
    }

    if (attacker.hammerWindingUp) {
        attacker.hammerWindupTimer -= dt;

        if (attacker.hammerWindupTimer <= 0) {
            attacker.hammerWindingUp = false;
            attacker.weaponCooldown = weapon.cooldown;

            Vec2 facing = attacker.getFacingVector();
            float strikeX = attacker.x + facing.x * attacker.radius * 1.5f;
            float strikeY = attacker.y + facing.y * attacker.radius * 1.5f;

            for (auto& target : bots) {
                if (&target == &attacker || !target.isAlive) continue;

                float dist = distance(strikeX, strikeY, target.x, target.y);
                if (dist < target.radius + 20.0f) {
                    float damage = WeaponHelpers::applyDamageMultipliers(attacker, weapon.damage);

                    float originalArmor = target.armor;
                    target.armor = 1.0f + (target.armor - 1.0f) * (1.0f - weapon.armorPierce);

                    Vec2 knockDir = WeaponHelpers::getDirectionTo(attacker, target);
                    Combat::applyDamage(target, damage, weapon.knockback, knockDir, &attacker, events);

                    target.armor = originalArmor;
                }
            }
        }
    }
}

// ============================================================================
// Battering Ram - Momentum-based damage
// ============================================================================

void Weapons::processBatteringRam(Bot& attacker, Bot& target,
                                  std::vector<CombatEvent>& events) {
    float speed = WeaponHelpers::getSpeed(attacker);
    if (speed < 50.0f) return;
    if (!WeaponHelpers::isInFrontArc(attacker, target, 0.5f)) return;

    const auto& weapon = ComponentRegistry::instance().getWeapon(attacker.weaponIndex);

    float damage = WeaponHelpers::applyDamageMultipliers(attacker, speed * 0.3f);
    Vec2 toTarget = WeaponHelpers::getDirectionTo(attacker, target);

    Combat::applyDamage(target, damage, weapon.knockback * 1.5f, toTarget, &attacker, events);
}

// ============================================================================
// Flail - Spin-based damage
// ============================================================================

void Weapons::processFlail(Bot& attacker, Bot& target,
                           std::vector<CombatEvent>& events, float dt) {
    if (!WeaponHelpers::canUseWeapon(attacker)) return;

    float angularSpeed = WeaponHelpers::getAngularSpeed(attacker);
    float linearSpeed = WeaponHelpers::getSpeed(attacker);
    float combinedSpeed = angularSpeed * 3.0f + linearSpeed * 0.01f;
    if (combinedSpeed < 0.3f) return;

    const auto& weapon = ComponentRegistry::instance().getWeapon(attacker.weaponIndex);

    float speedMult = std::min(2.0f, combinedSpeed);
    float damage = WeaponHelpers::applyDamageMultipliers(attacker, weapon.damage * speedMult);
    float knockback = weapon.knockback * speedMult;

    Vec2 knockDir = WeaponHelpers::getDirectionTo(attacker, target);
    Combat::applyDamage(target, damage, knockback, knockDir, &attacker, events);

    attacker.weaponCooldown = 0.25f;
}

// ============================================================================
// Whip - Long range extending attack
// ============================================================================

void Weapons::processWhip(Bot& attacker, std::vector<Bot>& bots,
                          std::vector<CombatEvent>& events, float dt) {
    if (attacker.empDisabled) return;

    const auto& weapon = ComponentRegistry::instance().getWeapon(attacker.weaponIndex);

    if (attacker.inputWeaponPressed && !attacker.whipExtended && attacker.weaponCooldown <= 0) {
        attacker.whipExtended = true;
        attacker.whipExtendTimer = 0.3f;
        attacker.whipAngle = attacker.angle;
    }

    if (attacker.whipExtended) {
        attacker.whipExtendTimer -= dt;

        Vec2 whipDir = Vec2::fromAngle(attacker.whipAngle);
        float whipLength = attacker.radius * 3.0f;

        for (auto& target : bots) {
            if (&target == &attacker || !target.isAlive) continue;

            Vec2 toTarget(target.x - attacker.x, target.y - attacker.y);
            float proj = toTarget.dot(whipDir);

            if (proj > 0 && proj < whipLength) {
                Vec2 closest = Vec2(attacker.x, attacker.y) + whipDir * proj;
                float dist = distance(closest.x, closest.y, target.x, target.y);

                if (dist < target.radius + 5.0f) {
                    float damage = WeaponHelpers::applyDamageMultipliers(attacker, weapon.damage);
                    Combat::applyDamage(target, damage, weapon.knockback, whipDir, &attacker, events);
                }
            }
        }

        if (attacker.whipExtendTimer <= 0) {
            attacker.whipExtended = false;
            attacker.weaponCooldown = weapon.cooldown;
        }
    }
}

// ============================================================================
// Saw Blade - Side-mounted continuous damage
// ============================================================================

void Weapons::processSawBlade(Bot& attacker, Bot& target,
                              std::vector<CombatEvent>& events, float dt) {
    if (!WeaponHelpers::canUseWeapon(attacker)) return;
    if (!WeaponHelpers::isInSideArc(attacker, target, 0.7f)) return;

    const auto& weapon = ComponentRegistry::instance().getWeapon(attacker.weaponIndex);

    float damage = WeaponHelpers::applyDamageMultipliers(attacker, weapon.damage);
    Vec2 knockDir = WeaponHelpers::getDirectionTo(attacker, target);

    Combat::applyDamage(target, damage, weapon.knockback, knockDir, &attacker, events);
    attacker.weaponCooldown = weapon.cooldown;
}

// ============================================================================
// Thwack Bar - Rear-mounted spin attack
// ============================================================================

void Weapons::processThwackBar(Bot& attacker, Bot& target,
                               std::vector<CombatEvent>& events, float dt) {
    if (!WeaponHelpers::canUseWeapon(attacker)) return;
    if (!WeaponHelpers::isBehind(attacker, target)) return;

    float angularSpeed = WeaponHelpers::getAngularSpeed(attacker);
    float speedMult = 0.5f + std::min(1.5f, angularSpeed * 2.0f);

    const auto& weapon = ComponentRegistry::instance().getWeapon(attacker.weaponIndex);

    float damage = WeaponHelpers::applyDamageMultipliers(attacker, weapon.damage * speedMult);
    float knockback = weapon.knockback * speedMult;
    Vec2 knockDir = WeaponHelpers::getDirectionTo(attacker, target);

    Combat::applyDamage(target, damage, knockback, knockDir, &attacker, events);
    attacker.weaponCooldown = weapon.cooldown;
}

// ============================================================================
// Piston Punch - Short range high knockback
// ============================================================================

void Weapons::processPistonPunch(Bot& attacker, std::vector<Bot>& bots,
                                 std::vector<CombatEvent>& events, float dt) {
    if (attacker.empDisabled) return;
    if (!attacker.inputWeaponPressed) return;
    if (attacker.weaponCooldown > 0) return;

    const auto& weapon = ComponentRegistry::instance().getWeapon(attacker.weaponIndex);

    Vec2 facing = attacker.getFacingVector();
    float punchRange = attacker.radius * 1.8f;

    for (auto& target : bots) {
        if (&target == &attacker || !target.isAlive) continue;

        float dist = distance(attacker.x, attacker.y, target.x, target.y);
        if (dist < punchRange + target.radius) {
            if (WeaponHelpers::isInFrontArc(attacker, target, 0.4f)) {
                float damage = WeaponHelpers::applyDamageMultipliers(attacker, weapon.damage);
                Combat::applyDamage(target, damage, weapon.knockback, facing, &attacker, events);
            }
        }
    }

    attacker.weaponCooldown = weapon.cooldown;
}

// ============================================================================
// Dual Spinners - Side-mounted spinning discs
// ============================================================================

void Weapons::processDualSpinners(Bot& attacker, Bot& target,
                                  std::vector<CombatEvent>& events, float dt) {
    if (!WeaponHelpers::canUseWeapon(attacker)) return;
    if (!WeaponHelpers::isInSideArc(attacker, target, 0.8f)) return;

    const auto& weapon = ComponentRegistry::instance().getWeapon(attacker.weaponIndex);

    float damage, knockback;
    if (attacker.inputWeapon && attacker.spinnerSpeed > 0.1f) {
        damage = weapon.damage * attacker.spinnerSpeed;
        knockback = weapon.knockback * attacker.spinnerSpeed;
        attacker.spinnerSpeed = 0.0f;
    } else {
        damage = weapon.damage * 0.5f;
        knockback = weapon.knockback * 0.5f;
    }

    damage = WeaponHelpers::applyDamageMultipliers(attacker, damage);
    Vec2 knockDir = WeaponHelpers::getDirectionTo(attacker, target);

    Combat::applyDamage(target, damage, knockback, knockDir, &attacker, events);
    attacker.weaponCooldown = weapon.cooldown;
}

} // namespace ScrapHeap
