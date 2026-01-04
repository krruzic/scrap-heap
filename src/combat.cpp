#include "combat.h"
#include "components.h"
#include "physics.h"
#include <cmath>
#include <algorithm>

namespace ScrapHeap {

bool Combat::areInContact(const Bot& a, const Bot& b) {
    float dist = distance(a.x, a.y, b.x, b.y);
    return dist < a.radius + b.radius + 5.0f;  // Small buffer for near-contact
}

Vec2 Combat::getContactPoint(const Bot& a, const Bot& b) {
    float dx = b.x - a.x;
    float dy = b.y - a.y;
    float dist = std::sqrt(dx * dx + dy * dy);
    if (dist < 0.001f) return Vec2(a.x, a.y);

    float t = a.radius / dist;
    return Vec2(a.x + dx * t, a.y + dy * t);
}

void Combat::applyDamage(Bot& target, float damage, float knockbackForce,
                        const Vec2& knockbackDir, Bot* source,
                        std::vector<CombatEvent>& events) {
    if (!target.isAlive) return;

    // Shield blocks damage
    if (target.shieldActive) {
        target.shieldActive = false;
        return;
    }

    // Apply armor
    float actualDamage = damage / target.armor;

    // Overdrive increases damage taken
    if (target.overdriveActive) {
        actualDamage *= 1.5f;
    }

    // Apply damage
    target.health -= actualDamage;

    // Reset spinner (stun mechanic) - reduced stun time
    target.spinnerSpeed *= 0.5f;  // Slow down instead of full reset
    target.spinnerStunTimer = 0.15f;

    // Calculate impact point for sparks
    Vec2 impactPoint;
    if (source) {
        float dx = target.x - source->x;
        float dy = target.y - source->y;
        float dist = std::sqrt(dx * dx + dy * dy);
        if (dist > 0.001f) {
            impactPoint.x = source->x + (dx / dist) * source->radius;
            impactPoint.y = source->y + (dy / dist) * source->radius;
        } else {
            impactPoint.x = target.x;
            impactPoint.y = target.y;
        }
    } else {
        impactPoint.x = target.x;
        impactPoint.y = target.y;
    }

    // Create impact spark event
    CombatEvent impactEvent;
    impactEvent.type = CombatEvent::Type::Impact;
    impactEvent.x = impactPoint.x;
    impactEvent.y = impactPoint.y;
    impactEvent.value = knockbackForce;  // Size of sparks based on force
    impactEvent.timer = 0.3f;
    events.push_back(impactEvent);

    // Apply knockback to target
    if (knockbackForce > 0) {
        Physics::applyKnockback(target, knockbackDir, knockbackForce);

        // Apply recoil to attacker (Newton's third law)
        if (source && source->isAlive && !source->anchorActive) {
            Vec2 recoilDir(-knockbackDir.x, -knockbackDir.y);
            float recoilForce = knockbackForce * 0.4f;  // 40% recoil
            Physics::applyKnockback(*source, recoilDir, recoilForce);
        }
    }

    // Backlash reflects damage to attacker
    if (target.backlashActive && source && source != &target) {
        float reflectedDamage = actualDamage * 0.5f;
        source->health -= reflectedDamage;
    }

    // Create damage event
    CombatEvent event;
    event.type = CombatEvent::Type::Damage;
    event.x = target.x;
    event.y = target.y;
    event.value = actualDamage;
    event.targetBot = target.playerIndex;
    event.sourceBot = source ? source->playerIndex : -1;
    event.timer = 0.5f;
    events.push_back(event);

    // Check for death
    if (target.health <= 0) {
        target.health = 0;
        target.isAlive = false;

        CombatEvent deathEvent;
        deathEvent.type = CombatEvent::Type::Death;
        deathEvent.x = target.x;
        deathEvent.y = target.y;
        deathEvent.targetBot = target.playerIndex;
        deathEvent.sourceBot = source ? source->playerIndex : -1;  // Track killer
        deathEvent.timer = 1.0f;
        events.push_back(deathEvent);
    }
}

void Combat::applyDamage(Bot& target, float damage, int sourceIndex,
                         std::vector<Bot>& bots, std::vector<CombatEvent>& events,
                         std::map<int, BotBattleStats>& stats,
                         std::vector<KillPopup>& killPopups) {
    float actualDamage = damage;

    // Apply damage
    target.health -= actualDamage;

    // Track stats
    stats[target.playerIndex].damageTaken += actualDamage;
    if (sourceIndex >= 0) {
        stats[sourceIndex].damageDealt += actualDamage;
    }

    // Create damage event
    CombatEvent event;
    event.type = CombatEvent::Type::Damage;
    event.x = target.x;
    event.y = target.y;
    event.value = actualDamage;
    event.targetBot = target.playerIndex;
    event.sourceBot = sourceIndex;
    event.timer = 0.5f;
    events.push_back(event);

    // Check for death
    if (target.health <= 0) {
        target.health = 0;
        target.isAlive = false;
        stats[target.playerIndex].deaths++;

        // Credit kill
        if (sourceIndex >= 0) {
            stats[sourceIndex].kills++;
            KillPopup popup;
            popup.playerIndex = sourceIndex;
            popup.timer = KillPopup::DURATION;
            killPopups.push_back(popup);
        }

        CombatEvent deathEvent;
        deathEvent.type = CombatEvent::Type::Death;
        deathEvent.x = target.x;
        deathEvent.y = target.y;
        deathEvent.targetBot = target.playerIndex;
        deathEvent.sourceBot = sourceIndex;
        deathEvent.timer = 1.0f;
        events.push_back(deathEvent);
    }
}

void Combat::processWeapons(std::vector<Bot>& bots, const StageDef& stage,
                           std::vector<CombatEvent>& events, float dt) {
    // Update weapon cooldowns and spinner speeds
    for (auto& bot : bots) {
        if (!bot.isAlive) continue;

        // Update cooldown
        if (bot.weaponCooldown > 0) {
            bot.weaponCooldown -= dt;
        }

        // Update spinner stun timer
        if (bot.spinnerStunTimer > 0) {
            bot.spinnerStunTimer -= dt;
        }

        // Update spinner speed
        const auto& weapon = ComponentRegistry::instance().getWeapon(bot.weaponIndex);
        if (weapon.name == "Spinner" || weapon.name == "Dual Spinners") {
            if (bot.spinnerStunTimer <= 0 && !bot.empDisabled) {
                // Spin up
                bot.spinnerSpeed += dt / weapon.spinUpTime;
                if (bot.spinnerSpeed > 1.0f) bot.spinnerSpeed = 1.0f;
            }
        }

        // Update EMP disable timer
        if (bot.empDisabledTimer > 0) {
            bot.empDisabledTimer -= dt;
            if (bot.empDisabledTimer <= 0) {
                bot.empDisabled = false;
            }
        }
    }

    // Process weapon interactions between bots
    for (size_t i = 0; i < bots.size(); ++i) {
        if (!bots[i].isAlive) continue;

        const auto& weapon = ComponentRegistry::instance().getWeapon(bots[i].weaponIndex);

        for (size_t j = i + 1; j < bots.size(); ++j) {
            if (!bots[j].isAlive) continue;
            if (!areInContact(bots[i], bots[j])) continue;

            // Process each weapon type
            if (weapon.name == "Spinner") {
                processSpinner(bots[i], bots[j], events, dt);
            } else if (weapon.name == "Clamp") {
                processClamp(bots[i], bots[j], events, dt);
            } else if (weapon.name == "Battering Ram") {
                processBatteringRam(bots[i], bots[j], events);
            } else if (weapon.name == "Flail") {
                processFlail(bots[i], bots[j], events, dt);
            } else if (weapon.name == "Saw Blade") {
                processSawBlade(bots[i], bots[j], events, dt);
            } else if (weapon.name == "Thwack Bar") {
                processThwackBar(bots[i], bots[j], events, dt);
            } else if (weapon.name == "Dual Spinners") {
                processDualSpinners(bots[i], bots[j], events, dt);
            }

            // Check other bot's weapon too
            const auto& weapon2 = ComponentRegistry::instance().getWeapon(bots[j].weaponIndex);
            if (weapon2.name == "Spinner") {
                processSpinner(bots[j], bots[i], events, dt);
            } else if (weapon2.name == "Clamp") {
                processClamp(bots[j], bots[i], events, dt);
            } else if (weapon2.name == "Battering Ram") {
                processBatteringRam(bots[j], bots[i], events);
            } else if (weapon2.name == "Flail") {
                processFlail(bots[j], bots[i], events, dt);
            } else if (weapon2.name == "Saw Blade") {
                processSawBlade(bots[j], bots[i], events, dt);
            } else if (weapon2.name == "Thwack Bar") {
                processThwackBar(bots[j], bots[i], events, dt);
            } else if (weapon2.name == "Dual Spinners") {
                processDualSpinners(bots[j], bots[i], events, dt);
            }
        }

        // Active weapons that work at range
        if (weapon.name == "Hammer") {
            processHammer(bots[i], bots, events, dt);
        } else if (weapon.name == "Whip") {
            processWhip(bots[i], bots, events, dt);
        } else if (weapon.name == "Piston Punch") {
            processPistonPunch(bots[i], bots, events, dt);
        }
    }
}

void Combat::processSpinner(Bot& attacker, Bot& target,
                           std::vector<CombatEvent>& events, float dt) {
    if (attacker.empDisabled) return;
    if (attacker.weaponCooldown > 0) return;

    const auto& weapon = ComponentRegistry::instance().getWeapon(attacker.weaponIndex);
    const auto& targetWeapon = ComponentRegistry::instance().getWeapon(target.weaponIndex);

    float damage;
    float knockback;

    if (attacker.inputSpecial && attacker.spinnerSpeed > 0.1f) {
        damage = weapon.damage * attacker.spinnerSpeed;
        knockback = weapon.knockback * attacker.spinnerSpeed;
        attacker.spinnerSpeed = 0.0f;
    } else {
        damage = weapon.damage * 0.5f;
        knockback = weapon.knockback * 0.5f;
    }

    if (attacker.damageBoostTimer > 0) damage *= 1.5f;
    if (attacker.overdriveActive) damage *= 1.5f;

    Vec2 knockDir(target.x - attacker.x, target.y - attacker.y);

    bool targetHasSpinner = (targetWeapon.name == "Spinner" || targetWeapon.name == "Dual Spinners")
                            && target.spinnerSpeed >= 0.1f;

    if (targetHasSpinner) {
        float targetDamage = targetWeapon.damage * 0.5f;
        if (target.inputSpecial && target.spinnerSpeed > 0.1f) {
            targetDamage = targetWeapon.damage * target.spinnerSpeed;
            target.spinnerSpeed = 0.0f;
        }
        if (target.damageBoostTimer > 0) targetDamage *= 1.5f;
        if (target.overdriveActive) targetDamage *= 1.5f;

        Vec2 reverseKnockDir(-knockDir.x, -knockDir.y);
        float targetKnockback = targetWeapon.knockback * 0.5f;

        applyDamage(target, damage, knockback, knockDir, &attacker, events);
        applyDamage(attacker, targetDamage * 0.7f, targetKnockback * 0.7f, reverseKnockDir, &target, events);

        attacker.spinnerSpeed *= 0.6f;
        target.spinnerSpeed *= 0.6f;
    } else {
        applyDamage(target, damage, knockback, knockDir, &attacker, events);
    }

    attacker.weaponCooldown = SPINNER_TICK_TIME;
}

void Combat::processClamp(Bot& attacker, Bot& target,
                         std::vector<CombatEvent>& events, float dt) {
    if (attacker.empDisabled) return;
    if (!attacker.inputWeaponPressed) return;
    if (attacker.grabState != GrabState::None) return;
    if (target.grabState != GrabState::None) return;

    const auto& weapon = ComponentRegistry::instance().getWeapon(attacker.weaponIndex);

    // Initiate grab
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

void Combat::processGrabs(std::vector<Bot>& bots, std::vector<CombatEvent>& events, float dt) {
    for (auto& bot : bots) {
        if (bot.grabState != GrabState::Grabbing) continue;

        // Find grabbed bot
        Bot* grabbed = nullptr;
        for (auto& other : bots) {
            if (other.playerIndex == bot.grabbingBot) {
                grabbed = &other;
                break;
            }
        }
        if (!grabbed) {
            bot.grabState = GrabState::None;
            bot.grabbingBot = -1;
            continue;
        }

        const auto& weapon = ComponentRegistry::instance().getWeapon(bot.weaponIndex);

        // Apply continuous damage
        float damage = weapon.damage * dt;
        if (bot.damageBoostTimer > 0) damage *= 1.5f;
        if (bot.overdriveActive) damage *= 1.5f;

        Vec2 knockDir(0, 0);  // No knockback during grab
        applyDamage(*grabbed, damage, 0, knockDir, &bot, events);

        // Drag grabbed bot behind grabber
        Vec2 facing = bot.getFacingVector();
        float dragDist = bot.radius + grabbed->radius + 10.0f;
        grabbed->x = bot.x - facing.x * dragDist;
        grabbed->y = bot.y - facing.y * dragDist;
        grabbed->velX = bot.velX;
        grabbed->velY = bot.velY;

        // Update grab timer
        bot.grabTimer -= dt;

        // Escape progress from time
        grabbed->grabEscapeProgress += dt / weapon.grabDuration;

        // Mashing speeds up escape
        if (grabbed->inputWeaponPressed) {
            grabbed->grabEscapeProgress += 0.15f;
        }

        // Check for escape or timeout
        if (bot.grabTimer <= 0 || grabbed->grabEscapeProgress >= 1.0f) {
            bot.grabState = GrabState::None;
            bot.grabbingBot = -1;
            grabbed->grabState = GrabState::None;
            grabbed->grabbedBy = -1;

            CombatEvent event;
            event.type = CombatEvent::Type::GrabEscape;
            event.x = grabbed->x;
            event.y = grabbed->y;
            event.timer = 0.5f;
            events.push_back(event);
        }
    }
}

void Combat::processHammer(Bot& attacker, std::vector<Bot>& bots,
                          std::vector<CombatEvent>& events, float dt) {
    if (attacker.empDisabled) return;

    const auto& weapon = ComponentRegistry::instance().getWeapon(attacker.weaponIndex);

    // Start windup on button press
    if (attacker.inputWeaponPressed && !attacker.hammerWindingUp && attacker.weaponCooldown <= 0) {
        attacker.hammerWindingUp = true;
        attacker.hammerWindupTimer = 0.8f;
    }

    // Update windup
    if (attacker.hammerWindingUp) {
        attacker.hammerWindupTimer -= dt;

        if (attacker.hammerWindupTimer <= 0) {
            // Strike!
            attacker.hammerWindingUp = false;
            attacker.weaponCooldown = weapon.cooldown;

            Vec2 facing = attacker.getFacingVector();
            float strikeRange = attacker.radius * 1.5f;
            float strikeX = attacker.x + facing.x * strikeRange;
            float strikeY = attacker.y + facing.y * strikeRange;

            for (auto& target : bots) {
                if (&target == &attacker) continue;
                if (!target.isAlive) continue;

                float dist = distance(strikeX, strikeY, target.x, target.y);
                if (dist < target.radius + 20.0f) {
                    float damage = weapon.damage;
                    if (attacker.damageBoostTimer > 0) damage *= 1.5f;
                    if (attacker.overdriveActive) damage *= 1.5f;

                    // Armor piercing is applied by temporarily modifying target armor
                    float originalArmor = target.armor;
                    target.armor = 1.0f + (target.armor - 1.0f) * (1.0f - weapon.armorPierce);

                    Vec2 knockDir(target.x - attacker.x, target.y - attacker.y);
                    applyDamage(target, damage, weapon.knockback, knockDir, &attacker, events);

                    target.armor = originalArmor;
                }
            }
        }
    }
}

void Combat::processBatteringRam(Bot& attacker, Bot& target,
                                std::vector<CombatEvent>& events) {
    // Damage based on speed
    float speed = std::sqrt(attacker.velX * attacker.velX + attacker.velY * attacker.velY);
    if (speed < 0.5f) return;  // Minimum speed threshold

    const auto& weapon = ComponentRegistry::instance().getWeapon(attacker.weaponIndex);

    float damage = speed * 2.0f;
    if (attacker.damageBoostTimer > 0) damage *= 1.5f;
    if (attacker.overdriveActive) damage *= 1.5f;

    Vec2 knockDir(target.x - attacker.x, target.y - attacker.y);
    applyDamage(target, damage, weapon.knockback, knockDir, &attacker, events);
}

void Combat::processFlail(Bot& attacker, Bot& target,
                         std::vector<CombatEvent>& events, float dt) {
    if (attacker.empDisabled) return;
    if (attacker.weaponCooldown > 0) return;

    // Flail damage based on angular velocity
    float angularSpeed = std::abs(attacker.angularVel);
    if (angularSpeed < 0.1f) return;

    const auto& weapon = ComponentRegistry::instance().getWeapon(attacker.weaponIndex);

    float damage = weapon.damage * std::min(1.0f, angularSpeed);
    float knockback = weapon.knockback * std::min(1.0f, angularSpeed);

    if (attacker.damageBoostTimer > 0) damage *= 1.5f;
    if (attacker.overdriveActive) damage *= 1.5f;

    Vec2 knockDir(target.x - attacker.x, target.y - attacker.y);
    applyDamage(target, damage, knockback, knockDir, &attacker, events);

    attacker.weaponCooldown = 0.3f;
}

void Combat::processWhip(Bot& attacker, std::vector<Bot>& bots,
                        std::vector<CombatEvent>& events, float dt) {
    if (attacker.empDisabled) return;

    const auto& weapon = ComponentRegistry::instance().getWeapon(attacker.weaponIndex);

    // Extend whip on button press
    if (attacker.inputWeaponPressed && !attacker.whipExtended && attacker.weaponCooldown <= 0) {
        attacker.whipExtended = true;
        attacker.whipExtendTimer = 0.3f;
        attacker.whipAngle = attacker.angle;
    }

    if (attacker.whipExtended) {
        attacker.whipExtendTimer -= dt;

        // Check for hits during extension
        Vec2 whipDir = Vec2::fromAngle(attacker.whipAngle);
        float whipLength = attacker.radius * 3.0f;

        for (auto& target : bots) {
            if (&target == &attacker) continue;
            if (!target.isAlive) continue;

            // Line-circle intersection for whip
            Vec2 toTarget(target.x - attacker.x, target.y - attacker.y);
            float proj = toTarget.dot(whipDir);

            if (proj > 0 && proj < whipLength) {
                Vec2 closest = Vec2(attacker.x, attacker.y) + whipDir * proj;
                float dist = distance(closest.x, closest.y, target.x, target.y);

                if (dist < target.radius + 5.0f) {
                    float damage = weapon.damage;
                    if (attacker.damageBoostTimer > 0) damage *= 1.5f;
                    if (attacker.overdriveActive) damage *= 1.5f;

                    Vec2 knockDir = whipDir;
                    applyDamage(target, damage, weapon.knockback, knockDir, &attacker, events);
                }
            }
        }

        if (attacker.whipExtendTimer <= 0) {
            attacker.whipExtended = false;
            attacker.weaponCooldown = weapon.cooldown;
        }
    }
}

void Combat::processSawBlade(Bot& attacker, Bot& target,
                            std::vector<CombatEvent>& events, float dt) {
    if (attacker.empDisabled) return;
    if (attacker.weaponCooldown > 0) return;

    // Check if target is to the side
    Vec2 facing = attacker.getFacingVector();
    Vec2 toTarget(target.x - attacker.x, target.y - attacker.y);
    toTarget = toTarget.normalized();

    float dot = facing.dot(toTarget);
    // Side-mounted, so check for targets not directly in front or behind
    if (std::abs(dot) < 0.7f) {
        const auto& weapon = ComponentRegistry::instance().getWeapon(attacker.weaponIndex);

        float damage = weapon.damage;
        if (attacker.damageBoostTimer > 0) damage *= 1.5f;
        if (attacker.overdriveActive) damage *= 1.5f;

        Vec2 knockDir = toTarget;
        applyDamage(target, damage, weapon.knockback, knockDir, &attacker, events);

        attacker.weaponCooldown = weapon.cooldown;
    }
}

void Combat::processThwackBar(Bot& attacker, Bot& target,
                             std::vector<CombatEvent>& events, float dt) {
    if (attacker.empDisabled) return;
    if (attacker.weaponCooldown > 0) return;

    // Check if target is behind (rear-mounted)
    Vec2 facing = attacker.getFacingVector();
    Vec2 toTarget(target.x - attacker.x, target.y - attacker.y);
    toTarget = toTarget.normalized();

    float dot = facing.dot(toTarget);
    // Rear-mounted, effective when target is behind
    if (dot < -0.3f) {
        // Also requires some angular velocity or backing up
        if (std::abs(attacker.angularVel) > 0.1f || attacker.inputBack) {
            const auto& weapon = ComponentRegistry::instance().getWeapon(attacker.weaponIndex);

            float damage = weapon.damage;
            if (attacker.damageBoostTimer > 0) damage *= 1.5f;
            if (attacker.overdriveActive) damage *= 1.5f;

            Vec2 knockDir = toTarget;
            applyDamage(target, damage, weapon.knockback, knockDir, &attacker, events);

            attacker.weaponCooldown = weapon.cooldown;
        }
    }
}

void Combat::processPistonPunch(Bot& attacker, std::vector<Bot>& bots,
                               std::vector<CombatEvent>& events, float dt) {
    if (attacker.empDisabled) return;
    if (!attacker.inputWeaponPressed) return;
    if (attacker.weaponCooldown > 0) return;

    const auto& weapon = ComponentRegistry::instance().getWeapon(attacker.weaponIndex);

    Vec2 facing = attacker.getFacingVector();
    float punchRange = attacker.radius * 1.3f;

    for (auto& target : bots) {
        if (&target == &attacker) continue;
        if (!target.isAlive) continue;

        float dist = distance(attacker.x, attacker.y, target.x, target.y);
        if (dist < punchRange + target.radius) {
            // Check if in front
            Vec2 toTarget(target.x - attacker.x, target.y - attacker.y);
            if (facing.dot(toTarget.normalized()) > 0.5f) {
                float damage = weapon.damage;
                if (attacker.damageBoostTimer > 0) damage *= 1.5f;
                if (attacker.overdriveActive) damage *= 1.5f;

                applyDamage(target, damage, weapon.knockback, facing, &attacker, events);
            }
        }
    }

    attacker.weaponCooldown = weapon.cooldown;
}

void Combat::processDualSpinners(Bot& attacker, Bot& target,
                                std::vector<CombatEvent>& events, float dt) {
    if (attacker.empDisabled) return;
    if (attacker.weaponCooldown > 0) return;

    Vec2 facing = attacker.getFacingVector();
    Vec2 toTarget(target.x - attacker.x, target.y - attacker.y);
    toTarget = toTarget.normalized();

    float dot = facing.dot(toTarget);
    if (std::abs(dot) < 0.8f) {
        const auto& weapon = ComponentRegistry::instance().getWeapon(attacker.weaponIndex);

        float damage;
        float knockback;

        if (attacker.inputSpecial && attacker.spinnerSpeed > 0.1f) {
            damage = weapon.damage * attacker.spinnerSpeed;
            knockback = weapon.knockback * attacker.spinnerSpeed;
            attacker.spinnerSpeed = 0.0f;
        } else {
            damage = weapon.damage * 0.5f;
            knockback = weapon.knockback * 0.5f;
        }

        if (attacker.damageBoostTimer > 0) damage *= 1.5f;
        if (attacker.overdriveActive) damage *= 1.5f;

        Vec2 knockDir = toTarget;
        applyDamage(target, damage, knockback, knockDir, &attacker, events);

        attacker.weaponCooldown = weapon.cooldown;
    }
}

void Combat::processSpecials(std::vector<Bot>& bots, std::vector<CombatEvent>& events, float dt) {
    for (auto& bot : bots) {
        if (!bot.isAlive) continue;

        // Update special cooldown
        if (bot.specialCooldown > 0) {
            bot.specialCooldown -= dt;
        }

        // Update active timers
        if (bot.specialActiveTimer > 0) {
            bot.specialActiveTimer -= dt;
            if (bot.specialActiveTimer <= 0) {
                // Deactivate all timed effects
                bot.boostActive = false;
                bot.anchorActive = false;
                bot.overdriveActive = false;
                bot.backlashActive = false;
                bot.berserkActive = false;
            }
        }

        // Update heal effect
        if (bot.healTimer > 0) {
            bot.healTimer -= dt;
            bot.health = std::min(bot.health + 5.0f * dt, bot.maxHealth);
        }

        // Update powerup timers
        if (bot.speedBoostTimer > 0) bot.speedBoostTimer -= dt;
        if (bot.damageBoostTimer > 0) bot.damageBoostTimer -= dt;
    }
}

void Combat::activateSpecial(Bot& bot, std::vector<Bot>& allBots,
                            std::vector<CombatEvent>& events) {
    if (bot.specialCooldown > 0) return;

    const auto& special = ComponentRegistry::instance().getSpecial(bot.specialIndex);

    bot.specialCooldown = special.cooldown;

    CombatEvent event;
    event.type = CombatEvent::Type::SpecialActivate;
    event.x = bot.x;
    event.y = bot.y;
    event.timer = 0.5f;
    events.push_back(event);

    if (special.name == "Boost") {
        bot.boostActive = true;
        bot.specialActiveTimer = special.duration;

        // Apply strong forward impulse for burst launch effect
        Vec2 facing = bot.getFacingVector();
        float boostImpulse = 600.0f;  // Strong launch forward
        Physics::applyImpulse(bot, Vec2(facing.x * boostImpulse, facing.y * boostImpulse));
    }
    else if (special.name == "Anchor") {
        bot.anchorActive = true;
        bot.specialActiveTimer = special.duration;
    }
    else if (special.name == "Overdrive") {
        bot.overdriveActive = true;
        bot.specialActiveTimer = special.duration;
    }
    else if (special.name == "Smoke") {
        // Smoke cloud is handled externally via mines/clouds vectors
    }
    else if (special.name == "Mine") {
        // Mine is handled externally
    }
    else if (special.name == "EMP Pulse") {
        // Disable nearby enemy weapons
        float empRadius = 100.0f;
        for (auto& other : allBots) {
            if (&other == &bot) continue;
            if (!other.isAlive) continue;

            float dist = distance(bot.x, bot.y, other.x, other.y);
            if (dist < empRadius) {
                other.empDisabled = true;
                other.empDisabledTimer = 2.0f;
                other.spinnerSpeed = 0.0f;
            }
        }
    }
    else if (special.name == "Repair Swarm") {
        bot.healTimer = special.duration;
    }
    else if (special.name == "Backlash") {
        bot.backlashActive = true;
        bot.specialActiveTimer = special.duration;
    }
    else if (special.name == "Berserk") {
        bot.berserkActive = true;
        bot.specialActiveTimer = special.duration;
    }
}

void Combat::processHazards(std::vector<Bot>& bots, StageDef& stage,
                           std::vector<CombatEvent>& events, float dt) {
    // Update hazard timers
    for (auto& hazard : stage.hazards) {
        if (hazard.type == HazardType::Flames) {
            hazard.activationTimer += dt;

            float cycleTime = hazard.activeTime + hazard.inactiveTime;
            float phase = std::fmod(hazard.activationTimer, cycleTime);
            hazard.isActive = phase < hazard.activeTime;
        }
    }

    // Apply hazard damage to bots
    for (auto& bot : bots) {
        if (!bot.isAlive) continue;

        HazardType hazardType;
        if (isInHazard(stage, bot.x, bot.y, bot.radius, &hazardType)) {
            if (hazardType == HazardType::Pit) {
                // Instant death
                if (isInPit(stage, bot.x, bot.y)) {
                    bot.health = 0;
                    bot.isAlive = false;

                    CombatEvent event;
                    event.type = CombatEvent::Type::Death;
                    event.x = bot.x;
                    event.y = bot.y;
                    event.targetBot = bot.playerIndex;
                    event.sourceBot = -1;  // Environmental death
                    event.timer = 1.0f;
                    events.push_back(event);
                }
            } else {
                // Find the hazard and apply its damage
                for (const auto& hazard : stage.hazards) {
                    if (hazard.type == hazardType && hazard.isActive) {
                        float damage = hazard.damage * dt;
                        bot.health -= damage;

                        if (bot.health <= 0) {
                            bot.health = 0;
                            bot.isAlive = false;

                            CombatEvent event;
                            event.type = CombatEvent::Type::Death;
                            event.x = bot.x;
                            event.y = bot.y;
                            event.targetBot = bot.playerIndex;
                            event.sourceBot = -1;  // Environmental death
                            event.timer = 1.0f;
                            events.push_back(event);
                        }
                        break;
                    }
                }
            }
        }
    }
}

} // namespace ScrapHeap
