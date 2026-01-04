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

    // === ENGINE-BASED STATS ===
    // Max speed: primarily based on power/weight ratio
    // Acceleration: based on power but inversely affected by weight more strongly
    float powerToWeight = engine.power / bot.totalWeight;

    // Base max speed scaled by power-to-weight (higher power = faster)
    float baseMaxSpeed = powerToWeight * 2.5f;

    // Base acceleration - weight has stronger effect (heavier = sluggish)
    // High power engines accelerate faster, but heavy bots are slow to start
    float baseAccelRate = (engine.power * 0.5f) / (bot.totalWeight * 1.2f);

    // Ramjet-style engines (high power, low torque) have better top speed but slower accel
    // Dragster-style engines have both high speed and good accel
    float accelModifier = 1.0f;
    if (engine.power > 300.0f) {
        // Very high power engines trade some accel for top speed
        accelModifier = 0.7f;
        baseMaxSpeed *= 1.2f;
    }

    float effectiveMaxSpeed = baseMaxSpeed;
    float effectiveAccelRate = baseAccelRate * accelModifier;

    // Speed boost powerup
    if (bot.speedBoostTimer > 0) {
        effectiveMaxSpeed *= 1.3f;
        effectiveAccelRate *= 1.2f;
    }

    // Berserk effect
    if (bot.berserkActive) {
        effectiveMaxSpeed *= 1.5f;
        effectiveAccelRate *= 1.3f;
    }

    // Boost special - big acceleration boost
    if (bot.boostActive) {
        effectiveAccelRate *= 3.0f;
        effectiveMaxSpeed *= 1.5f;
    }

    // Reduced speed while grabbing
    if (bot.grabState == GrabState::Grabbing) {
        effectiveMaxSpeed *= 0.5f;
        effectiveAccelRate *= 0.5f;
    }

    // === TANK CONTROLS ===
    // Steering: left stick X or d-pad left/right
    float steerInput = bot.stickX;
    if (bot.inputLeft) steerInput = -1.0f;
    if (bot.inputRight) steerInput = 1.0f;

    // Throttle and reverse from shoulder buttons/triggers
    float driveInput = bot.throttle - bot.reverse;

    // Turn speed based on engine torque
    // Higher torque = faster turning, weight reduces it
    float baseTurnRate = (engine.torque / bot.totalWeight) * 5.0f;

    // Angular velocity smoothing - don't turn instantly
    float targetAngularVel = steerInput * baseTurnRate;
    float angularAccel = 15.0f;  // How fast we reach target turn rate

    // Smoothly interpolate angular velocity
    float angularDiff = targetAngularVel - bot.angularVel;
    float maxAngularChange = angularAccel * dt;
    if (std::abs(angularDiff) < maxAngularChange) {
        bot.angularVel = targetAngularVel;
    } else {
        bot.angularVel += (angularDiff > 0 ? maxAngularChange : -maxAngularChange);
    }

    // Apply angular velocity to angle
    bot.angle += bot.angularVel * dt;
    bot.angle = normalizeAngle(bot.angle);

    // === GRADUAL ACCELERATION ===
    Vec2 facing = bot.getFacingVector();

    // Current speed in facing direction
    float currentForwardSpeed = facing.x * bot.velX + facing.y * bot.velY;

    // Target speed based on input
    float targetSpeed = driveInput * effectiveMaxSpeed;

    // Speed difference
    float speedDiff = targetSpeed - currentForwardSpeed;

    // Acceleration curve: faster from rest, slower near max
    float speedRatio = std::abs(currentForwardSpeed) / std::max(0.1f, effectiveMaxSpeed);
    float accelCurve = 1.0f - speedRatio * 0.4f;  // 100% at rest, 60% at max
    accelCurve = std::max(0.3f, accelCurve);  // Never below 30%

    // Calculate acceleration this frame
    float accelThisFrame = effectiveAccelRate * accelCurve * dt * 80.0f;

    // Clamp to not overshoot target
    if (std::abs(accelThisFrame) > std::abs(speedDiff)) {
        accelThisFrame = speedDiff;
    } else if (speedDiff < 0) {
        accelThisFrame = -accelThisFrame;
    }

    // Apply acceleration in facing direction
    bot.velX += facing.x * accelThisFrame;
    bot.velY += facing.y * accelThisFrame;

    // Clamp to max speed
    float speed = std::sqrt(bot.velX * bot.velX + bot.velY * bot.velY);
    if (speed > effectiveMaxSpeed) {
        float scale = effectiveMaxSpeed / speed;
        bot.velX *= scale;
        bot.velY *= scale;
    }

    // Apply position
    bot.x += bot.velX * dt * 50.0f;
    bot.y += bot.velY * dt * 50.0f;
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
