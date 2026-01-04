#include "physics.h"
#include "components.h"
#include <cmath>
#include <algorithm>

namespace ScrapHeap {

void Physics::updateBotMovement(Bot& bot, float dt) {
    if (!bot.isAlive) return;

    // Skip movement if anchored
    if (bot.anchorActive) {
        bot.velX = 0;
        bot.velY = 0;
        bot.angularVel = 0;
        return;
    }

    // Skip movement if grabbed (can wiggle slightly)
    if (bot.grabState == GrabState::Grabbed) {
        float wiggleSpeed = bot.acceleration * 0.1f;
        if (bot.inputLeft) bot.velX -= wiggleSpeed * dt;
        if (bot.inputRight) bot.velX += wiggleSpeed * dt;
        bot.velX *= 0.8f;
        bot.velY *= 0.8f;
        return;
    }

    const auto& engine = ComponentRegistry::instance().getEngine(bot.engineIndex);

    // Calculate effective stats with modifiers - MUCH SLOWER base speed
    float effectiveMaxSpeed = bot.maxSpeed * 0.4f;  // Slower tank-like speed
    float effectiveAccel = bot.acceleration * 0.5f;

    // Speed boost powerup
    if (bot.speedBoostTimer > 0) {
        effectiveMaxSpeed *= 1.3f;
        effectiveAccel *= 1.3f;
    }

    // Berserk effect
    if (bot.berserkActive) {
        effectiveMaxSpeed *= 1.5f;
        effectiveAccel *= 1.5f;
    }

    // Boost special
    if (bot.boostActive) {
        effectiveAccel *= 2.0f;
        effectiveMaxSpeed *= 1.5f;
    }

    // Reduced speed while grabbing
    if (bot.grabState == GrabState::Grabbing) {
        effectiveMaxSpeed *= 0.5f;
        effectiveAccel *= 0.5f;
    }

    // === TANK CONTROLS ===
    // Steering: left stick X or d-pad left/right
    float steerInput = bot.stickX;
    if (bot.inputLeft) steerInput = -1.0f;
    if (bot.inputRight) steerInput = 1.0f;

    // Throttle and reverse from shoulder buttons/triggers
    float driveInput = bot.throttle - bot.reverse;

    // Turn speed based on engine torque - slower for tank feel
    float turnRate = (engine.torque / 150.0f) * 2.5f;

    // Apply turning (only when moving or with significant input)
    float currentSpeed = std::sqrt(bot.velX * bot.velX + bot.velY * bot.velY);
    if (std::abs(steerInput) > 0.1f) {
        // Can turn in place at reduced rate, or while moving
        float turnMultiplier = 0.4f + (currentSpeed / effectiveMaxSpeed) * 0.6f;
        turnMultiplier = std::min(1.0f, turnMultiplier);
        bot.angle += steerInput * turnRate * turnMultiplier * dt;
        bot.angle = normalizeAngle(bot.angle);
    }

    // Apply acceleration in facing direction
    if (std::abs(driveInput) > 0.1f) {
        Vec2 facing = bot.getFacingVector();

        // Forward or backward
        float accel = effectiveAccel * driveInput;
        bot.velX += facing.x * accel * dt;
        bot.velY += facing.y * accel * dt;
    }

    // Clamp to max speed
    float speed = std::sqrt(bot.velX * bot.velX + bot.velY * bot.velY);
    if (speed > effectiveMaxSpeed) {
        float scale = effectiveMaxSpeed / speed;
        bot.velX *= scale;
        bot.velY *= scale;
    }

    // Apply position (slower multiplier for tank feel)
    bot.x += bot.velX * dt * 40.0f;
    bot.y += bot.velY * dt * 40.0f;
}

void Physics::applyFriction(Bot& bot, float dt) {
    float friction = std::pow(FRICTION, dt * 60.0f);
    bot.velX *= friction;
    bot.velY *= friction;
}

void Physics::resolveWallCollisions(Bot& bot, const StageDef& stage) {
    // Left wall
    if (bot.x - bot.radius < 0) {
        bot.x = bot.radius;
        bot.velX = -bot.velX * WALL_BOUNCE_DAMPING;
    }
    // Right wall
    if (bot.x + bot.radius > stage.width) {
        bot.x = stage.width - bot.radius;
        bot.velX = -bot.velX * WALL_BOUNCE_DAMPING;
    }
    // Top wall
    if (bot.y - bot.radius < 0) {
        bot.y = bot.radius;
        bot.velY = -bot.velY * WALL_BOUNCE_DAMPING;
    }
    // Bottom wall
    if (bot.y + bot.radius > stage.height) {
        bot.y = stage.height - bot.radius;
        bot.velY = -bot.velY * WALL_BOUNCE_DAMPING;
    }
}

CollisionResult Physics::checkBotCollision(const Bot& a, const Bot& b) {
    CollisionResult result;

    float dx = b.x - a.x;
    float dy = b.y - a.y;
    float dist = std::sqrt(dx * dx + dy * dy);
    float minDist = a.radius + b.radius;

    if (dist < minDist && dist > 0.001f) {
        result.collided = true;
        result.normal = Vec2(dx / dist, dy / dist);
        result.penetration = minDist - dist;
    }

    return result;
}

void Physics::resolveBotCollision(Bot& a, Bot& b, const CollisionResult& collision) {
    if (!collision.collided) return;

    // Separate bots
    float totalWeight = a.totalWeight + b.totalWeight;
    float aRatio = b.totalWeight / totalWeight;
    float bRatio = a.totalWeight / totalWeight;

    // If anchored, treat as infinite weight
    if (a.anchorActive) {
        aRatio = 0.0f;
        bRatio = 1.0f;
    }
    if (b.anchorActive) {
        aRatio = 1.0f;
        bRatio = 0.0f;
    }

    float separation = collision.penetration + SEPARATION_FORCE;
    a.x -= collision.normal.x * separation * aRatio;
    a.y -= collision.normal.y * separation * aRatio;
    b.x += collision.normal.x * separation * bRatio;
    b.y += collision.normal.y * separation * bRatio;

    // Calculate relative velocity
    float relVelX = a.velX - b.velX;
    float relVelY = a.velY - b.velY;
    float relVelNormal = relVelX * collision.normal.x + relVelY * collision.normal.y;

    // Don't resolve if moving apart
    if (relVelNormal > 0) return;

    // Calculate impulse
    float restitution = COLLISION_ELASTICITY;
    float impulse = -(1.0f + restitution) * relVelNormal / (1.0f / a.totalWeight + 1.0f / b.totalWeight);

    // Apply impulse (unless anchored)
    if (!a.anchorActive) {
        a.velX += impulse / a.totalWeight * collision.normal.x;
        a.velY += impulse / a.totalWeight * collision.normal.y;
    }
    if (!b.anchorActive) {
        b.velX -= impulse / b.totalWeight * collision.normal.x;
        b.velY -= impulse / b.totalWeight * collision.normal.y;
    }
}

void Physics::applyKnockback(Bot& bot, const Vec2& direction, float force) {
    if (bot.anchorActive) return;

    // Gyro-stabilized engines resist rotational knockback
    const auto& engine = ComponentRegistry::instance().getEngine(bot.engineIndex);

    float knockbackForce = force / bot.totalWeight * 50.0f;
    Vec2 normalized = direction.normalized();

    bot.velX += normalized.x * knockbackForce;
    bot.velY += normalized.y * knockbackForce;

    // Add some angular knockback unless gyro-stabilized
    if (!engine.gyroStabilized) {
        bot.angularVel += (std::rand() % 100 - 50) / 100.0f * knockbackForce * 0.1f;
    }
}

void Physics::applyImpulse(Bot& bot, const Vec2& impulse) {
    if (bot.anchorActive) return;

    bot.velX += impulse.x / bot.totalWeight;
    bot.velY += impulse.y / bot.totalWeight;
}

bool Physics::isInWall(const Bot& bot, const StageDef& stage) {
    return bot.x - bot.radius < 0 ||
           bot.x + bot.radius > stage.width ||
           bot.y - bot.radius < 0 ||
           bot.y + bot.radius > stage.height;
}

Vec2 Physics::getWallNormal(const Bot& bot, const StageDef& stage) {
    Vec2 normal(0, 0);
    if (bot.x - bot.radius < 0) normal.x = 1;
    if (bot.x + bot.radius > stage.width) normal.x = -1;
    if (bot.y - bot.radius < 0) normal.y = 1;
    if (bot.y + bot.radius > stage.height) normal.y = -1;
    return normal.normalized();
}

} // namespace ScrapHeap
