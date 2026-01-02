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

    // Skip movement if grabbed (can only rotate slowly)
    if (bot.grabState == GrabState::Grabbed) {
        // Slow rotation only
        float turnRate = bot.turnSpeed * 0.2f;
        if (bot.inputLeft) bot.angularVel -= turnRate * dt * 60.0f;
        if (bot.inputRight) bot.angularVel += turnRate * dt * 60.0f;
        bot.angularVel *= ANGULAR_FRICTION;
        bot.angle += bot.angularVel * dt;
        bot.angle = normalizeAngle(bot.angle);
        return;
    }

    const auto& engine = ComponentRegistry::instance().getEngine(bot.engineIndex);

    // Calculate effective stats with modifiers
    float effectiveMaxSpeed = bot.maxSpeed;
    float effectiveAccel = bot.acceleration;
    float effectiveTurnSpeed = bot.turnSpeed;

    // Speed boost powerup
    if (bot.speedBoostTimer > 0) {
        effectiveMaxSpeed *= 1.3f;
        effectiveAccel *= 1.3f;
    }

    // Berserk effect
    if (bot.berserkActive) {
        effectiveMaxSpeed *= 1.5f;
        effectiveAccel *= 1.5f;
        effectiveTurnSpeed *= 0.5f;
    }

    // Boost special
    if (bot.boostActive) {
        effectiveAccel *= 1.5f;
    }

    // Reduced speed while grabbing
    if (bot.grabState == GrabState::Grabbing) {
        effectiveMaxSpeed *= 0.5f;
        effectiveAccel *= 0.5f;
    }

    // Rotation
    if (bot.inputLeft) {
        bot.angularVel -= effectiveTurnSpeed * dt * 60.0f;
    }
    if (bot.inputRight) {
        bot.angularVel += effectiveTurnSpeed * dt * 60.0f;
    }

    // Apply angular friction
    bot.angularVel *= std::pow(ANGULAR_FRICTION, dt * 60.0f);
    bot.angle += bot.angularVel * dt;
    bot.angle = normalizeAngle(bot.angle);

    // Get facing direction
    Vec2 facing = bot.getFacingVector();

    // Apply thrust
    float thrust = 0.0f;
    if (bot.inputForward) thrust = effectiveAccel;
    if (bot.inputBack) thrust = -effectiveAccel * 0.5f;  // Reverse is slower

    // Omni-drive can strafe
    if (engine.omniDrive) {
        Vec2 strafe(-facing.y, facing.x);
        if (bot.stickX != 0.0f || bot.stickY != 0.0f) {
            // Use analog stick for omni movement
            bot.velX += bot.stickX * effectiveAccel * dt;
            bot.velY += bot.stickY * effectiveAccel * dt;
        }
    }

    // Apply thrust in facing direction
    bot.velX += facing.x * thrust * dt;
    bot.velY += facing.y * thrust * dt;

    // Clamp to max speed
    float speed = std::sqrt(bot.velX * bot.velX + bot.velY * bot.velY);
    if (speed > effectiveMaxSpeed) {
        float scale = effectiveMaxSpeed / speed;
        bot.velX *= scale;
        bot.velY *= scale;
    }

    // Apply position
    bot.x += bot.velX * dt * 60.0f;
    bot.y += bot.velY * dt * 60.0f;
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
