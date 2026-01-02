#pragma once

#include "utils.h"
#include "bot.h"
#include "stage.h"
#include <vector>

namespace ScrapHeap {

// Collision result
struct CollisionResult {
    bool collided = false;
    Vec2 normal;
    float penetration = 0.0f;
    int otherBotIndex = -1;
};

// Physics system
class Physics {
public:
    // Update bot movement based on input
    static void updateBotMovement(Bot& bot, float dt);

    // Apply friction to velocity
    static void applyFriction(Bot& bot, float dt);

    // Check and resolve wall collisions
    static void resolveWallCollisions(Bot& bot, const StageDef& stage);

    // Check collision between two bots
    static CollisionResult checkBotCollision(const Bot& a, const Bot& b);

    // Resolve collision between two bots
    static void resolveBotCollision(Bot& a, Bot& b, const CollisionResult& collision);

    // Apply knockback to a bot
    static void applyKnockback(Bot& bot, const Vec2& direction, float force);

    // Apply impulse to a bot
    static void applyImpulse(Bot& bot, const Vec2& impulse);

    // Check if bot is in wall
    static bool isInWall(const Bot& bot, const StageDef& stage);

    // Get wall bounce normal
    static Vec2 getWallNormal(const Bot& bot, const StageDef& stage);

private:
    static constexpr float WALL_BOUNCE_DAMPING = 0.5f;
    static constexpr float SEPARATION_FORCE = 5.0f;
};

} // namespace ScrapHeap
