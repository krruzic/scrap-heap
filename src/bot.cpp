#include "bot.h"
#include "components.h"

namespace ScrapHeap {

void Bot::calculateDerivedStats() {
    const auto& frame = ComponentRegistry::instance().getFrame(frameIndex);
    const auto& engine = ComponentRegistry::instance().getEngine(engineIndex);
    const auto& weapon = ComponentRegistry::instance().getWeapon(weaponIndex);
    const auto& special = ComponentRegistry::instance().getSpecial(specialIndex);

    // Calculate total weight
    totalWeight = frame.weight + engine.weight + weapon.weight;

    // Calculate derived movement stats
    acceleration = (engine.power / totalWeight) * 50.0f;
    maxSpeed = (engine.power / totalWeight) * 3.0f;
    turnSpeed = (engine.torque / totalWeight) * 0.15f;

    // Frame stats
    armor = frame.armor;
    radius = frame.radius;

    // Calculate max health
    maxHealth = BASE_HEALTH * (0.8f + totalWeight / 200.0f);
    health = maxHealth;
}

void Bot::reset(float spawnX, float spawnY, float spawnAngle) {
    x = spawnX;
    y = spawnY;
    angle = spawnAngle;
    velX = 0.0f;
    velY = 0.0f;
    angularVel = 0.0f;

    calculateDerivedStats();

    // Reset combat state
    weaponCooldown = 0.0f;
    spinnerSpeed = 0.0f;
    spinnerStunTimer = 0.0f;
    specialCooldown = 0.0f;
    specialActiveTimer = 0.0f;

    // Reset grab state
    grabState = GrabState::None;
    grabbedBy = -1;
    grabbingBot = -1;
    grabTimer = 0.0f;
    grabEscapeProgress = 0.0f;

    // Reset effects
    boostActive = false;
    anchorActive = false;
    overdriveActive = false;
    shieldActive = false;
    backlashActive = false;
    berserkActive = false;
    empDisabled = false;
    empDisabledTimer = 0.0f;
    healTimer = 0.0f;

    // Reset weapon state
    hammerWindingUp = false;
    hammerWindupTimer = 0.0f;
    whipExtended = false;
    whipExtendTimer = 0.0f;
    whipAngle = 0.0f;

    // Reset powerup effects
    speedBoostTimer = 0.0f;
    damageBoostTimer = 0.0f;
    heldPowerup = -1;

    // Reset input
    inputForward = false;
    inputBack = false;
    inputLeft = false;
    inputRight = false;
    inputWeapon = false;
    inputWeaponPressed = false;
    inputSpecial = false;
    inputSpecialPressed = false;
    inputPowerup = false;
    inputPowerupPressed = false;
    stickX = 0.0f;
    stickY = 0.0f;

    isAlive = true;
}

Vec2 Bot::getFacingVector() const {
    return Vec2::fromAngle(angle);
}

Bot createBot(int playerIndex, const std::string& name,
              int frameIdx, int engineIdx, int weaponIdx, int specialIdx, int colorIdx) {
    Bot bot;
    bot.playerIndex = playerIndex;
    bot.displayName = name;
    bot.frameIndex = frameIdx;
    bot.engineIndex = engineIdx;
    bot.weaponIndex = weaponIdx;
    bot.specialIndex = specialIdx;
    bot.colorIndex = colorIdx;
    bot.calculateDerivedStats();
    return bot;
}

} // namespace ScrapHeap
