#pragma once

#include "utils.h"
#include "components.h"
#include <SDL3/SDL.h>
#include <string>

namespace ScrapHeap {

// Forward declarations
struct BattleState;

// Grab state
enum class GrabState {
    None,
    Grabbing,   // This bot is grabbing another
    Grabbed     // This bot is being grabbed
};

// Bot structure
struct Bot {
    // Transform
    float x = 0.0f;
    float y = 0.0f;
    float angle = 0.0f;         // Facing direction in radians
    float velX = 0.0f;
    float velY = 0.0f;
    float angularVel = 0.0f;

    // Derived stats (calculated from components)
    float totalWeight = 0.0f;
    float maxSpeed = 0.0f;
    float acceleration = 0.0f;
    float turnSpeed = 0.0f;
    float armor = 1.0f;
    float radius = 30.0f;

    // Combat
    float health = 100.0f;
    float maxHealth = 100.0f;
    float weaponCooldown = 0.0f;
    float spinnerSpeed = 0.0f;      // 0.0 to 1.0
    float spinnerStunTimer = 0.0f;
    float specialCooldown = 0.0f;
    float specialActiveTimer = 0.0f;

    // Grab state
    GrabState grabState = GrabState::None;
    int grabbedBy = -1;         // Index of bot grabbing this one
    int grabbingBot = -1;       // Index of bot this one is grabbing
    float grabTimer = 0.0f;
    float grabEscapeProgress = 0.0f;

    // Active effects
    bool boostActive = false;
    bool anchorActive = false;
    bool overdriveActive = false;
    bool shieldActive = false;
    bool backlashActive = false;
    bool berserkActive = false;
    bool empDisabled = false;
    float empDisabledTimer = 0.0f;
    float healTimer = 0.0f;

    // Weapon state
    bool hammerWindingUp = false;
    float hammerWindupTimer = 0.0f;
    bool whipExtended = false;
    float whipExtendTimer = 0.0f;
    float whipAngle = 0.0f;

    // Powerup effects
    float speedBoostTimer = 0.0f;
    float damageBoostTimer = 0.0f;
    int heldPowerup = -1;  // Index of held powerup type, -1 if none

    // Build indices
    int frameIndex = 0;
    int engineIndex = 0;
    int weaponIndex = 0;
    int specialIndex = 0;
    int colorIndex = 0;

    // Input state
    bool inputForward = false;
    bool inputBack = false;
    bool inputLeft = false;
    bool inputRight = false;
    bool inputWeapon = false;
    bool inputWeaponPressed = false;  // Just pressed this frame
    bool inputSpecial = false;
    bool inputSpecialPressed = false;
    bool inputPowerup = false;
    bool inputPowerupPressed = false;
    float stickX = 0.0f;  // For analog stick (steering)
    float stickY = 0.0f;
    float throttle = 0.0f;  // 0-1 for forward
    float reverse = 0.0f;   // 0-1 for backward

    // Identity
    int playerIndex = 0;
    std::string displayName = "Player";
    bool isAlive = true;
    bool isBot = false;  // AI controlled

    // Methods
    void calculateDerivedStats();
    void reset(float spawnX, float spawnY, float spawnAngle);
    Vec2 getFacingVector() const;
    Vec2 getPosition() const { return Vec2(x, y); }
    void setPosition(const Vec2& pos) { x = pos.x; y = pos.y; }
};

// Create a bot with the given build
Bot createBot(int playerIndex, const std::string& name,
              int frameIdx, int engineIdx, int weaponIdx, int specialIdx, int colorIdx);

} // namespace ScrapHeap
