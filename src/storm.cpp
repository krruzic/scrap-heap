#include "storm.h"
#include "renderer.h"
#include <cmath>
#include <algorithm>

namespace ScrapHeap {

void Storm::reset(float stageWidth, float stageHeight) {
    left = 0.0f;
    right = stageWidth;
    top = 0.0f;
    bottom = stageHeight;
    currentPhase = 0;
    phaseTimer = 0.0f;
    active = false;
    damagePerSecond = 5.0f;

    // Set new targets
    float shrinkAmount = stageWidth * 0.15f;
    targetLeft = shrinkAmount;
    targetRight = stageWidth - shrinkAmount;
    targetTop = shrinkAmount;
    targetBottom = stageHeight - shrinkAmount;
}

bool Storm::isOutside(float x, float y) const {
    if (!active) return false;
    return x < left || x > right || y < top || y > bottom;
}

void StormManager::update(Storm& storm, const StageDef& stage, float matchTimer, float dt) {
    // Update animation timers
    storm.animTimer += dt;
    storm.pulseTimer += dt;

    // Activate storm after initial delay
    if (!storm.active && matchTimer >= Storm::INITIAL_DELAY) {
        storm.active = true;
        storm.currentPhase = 1;
        storm.phaseTimer = 0.0f;
    }

    if (!storm.active) return;

    storm.phaseTimer += dt;

    // Check for phase advancement
    if (storm.phaseTimer >= Storm::PHASE_DURATION && storm.currentPhase < Storm::MAX_PHASES) {
        storm.currentPhase++;
        storm.phaseTimer = 0.0f;
        storm.damagePerSecond += 2.0f;  // Increase damage each phase

        // Calculate new targets (shrink further toward center)
        float shrinkFactor = 0.85f - storm.currentPhase * 0.1f;
        float centerX = stage.width / 2.0f;
        float centerY = stage.height / 2.0f;

        float halfW = (stage.width / 2.0f) * shrinkFactor;
        float halfH = (stage.height / 2.0f) * shrinkFactor;

        // Ensure minimum size
        halfW = std::max(halfW, Storm::FINAL_RADIUS);
        halfH = std::max(halfH, Storm::FINAL_RADIUS);

        storm.targetLeft = centerX - halfW;
        storm.targetRight = centerX + halfW;
        storm.targetTop = centerY - halfH;
        storm.targetBottom = centerY + halfH;
    }

    // Smoothly shrink toward targets
    float speed = storm.getShrinkSpeed() * dt;
    if (storm.left < storm.targetLeft) {
        storm.left = std::min(storm.left + speed, storm.targetLeft);
    }
    if (storm.right > storm.targetRight) {
        storm.right = std::max(storm.right - speed, storm.targetRight);
    }
    if (storm.top < storm.targetTop) {
        storm.top = std::min(storm.top + speed, storm.targetTop);
    }
    if (storm.bottom > storm.targetBottom) {
        storm.bottom = std::max(storm.bottom - speed, storm.targetBottom);
    }
}

void StormManager::applyDamage(Storm& storm, std::vector<Bot>& bots,
                               std::vector<CombatEvent>& events, float dt) {
    if (!storm.active) return;

    for (auto& bot : bots) {
        if (!bot.isAlive) continue;

        if (storm.isOutside(bot.x, bot.y)) {
            float damage = storm.damagePerSecond * dt;
            bot.health -= damage;

            // Create damage event periodically
            static float damageEventTimer = 0.0f;
            damageEventTimer += dt;
            if (damageEventTimer >= 0.5f) {
                damageEventTimer = 0.0f;
                CombatEvent event;
                event.type = CombatEvent::Type::Damage;
                event.x = bot.x;
                event.y = bot.y;
                event.value = damage * 2;  // Show accumulated damage
                event.timer = 0.5f;
                events.push_back(event);
            }

            // Check for death
            if (bot.health <= 0) {
                bot.health = 0;
                bot.isAlive = false;

                CombatEvent deathEvent;
                deathEvent.type = CombatEvent::Type::Death;
                deathEvent.x = bot.x;
                deathEvent.y = bot.y;
                deathEvent.timer = 1.0f;
                events.push_back(deathEvent);
            }
        }
    }
}

void StormManager::render(const Storm& storm, const StageDef& stage,
                          float cameraOffsetX, float cameraOffsetY) {
    if (!storm.active) return;

    auto& renderer = Renderer::instance();

    float ox = cameraOffsetX;
    float oy = cameraOffsetY;

    // Animated pulse effect
    float pulse = 0.7f + 0.3f * std::sin(storm.pulseTimer * 2.0f);
    float fastPulse = 0.5f + 0.5f * std::sin(storm.animTimer * 5.0f);

    // === Layer 1: Dense storm background (~80% opacity) ===
    uint8_t baseAlpha = static_cast<uint8_t>(190 + 30 * pulse);
    SDL_Color stormBase = {25, 5, 50, baseAlpha};

    // Draw storm zone rectangles (the dangerous areas outside safe zone)
    // Left zone
    if (storm.left > 0) {
        renderer.drawRect(ox, oy, storm.left, stage.height, stormBase, true);
    }
    // Right zone
    if (storm.right < stage.width) {
        renderer.drawRect(ox + storm.right, oy,
                         stage.width - storm.right, stage.height, stormBase, true);
    }
    // Top zone (between left and right)
    if (storm.top > 0) {
        renderer.drawRect(ox + storm.left, oy,
                         storm.right - storm.left, storm.top, stormBase, true);
    }
    // Bottom zone (between left and right)
    if (storm.bottom < stage.height) {
        renderer.drawRect(ox + storm.left, oy + storm.bottom,
                         storm.right - storm.left,
                         stage.height - storm.bottom, stormBase, true);
    }

    // === Layer 2: Pulsing energy overlay ===
    uint8_t energyAlpha = static_cast<uint8_t>(60 + 40 * fastPulse);
    SDL_Color energyColor = {80, 40, 150, energyAlpha};

    if (storm.left > 0) {
        renderer.drawRect(ox, oy, storm.left, stage.height, energyColor, true);
    }
    if (storm.right < stage.width) {
        renderer.drawRect(ox + storm.right, oy,
                         stage.width - storm.right, stage.height, energyColor, true);
    }
    if (storm.top > 0) {
        renderer.drawRect(ox + storm.left, oy,
                         storm.right - storm.left, storm.top, energyColor, true);
    }
    if (storm.bottom < stage.height) {
        renderer.drawRect(ox + storm.left, oy + storm.bottom,
                         storm.right - storm.left,
                         stage.height - storm.bottom, energyColor, true);
    }

    // === Layer 3: Lightning bolts throughout the storm zones ===
    if (storm.left > 0) {
        drawStormLightning(ox, oy, storm.left, stage.height, storm.animTimer);
    }
    if (storm.right < stage.width) {
        drawStormLightning(ox + storm.right, oy,
                          stage.width - storm.right, stage.height, storm.animTimer);
    }
    if (storm.top > 0) {
        drawStormLightning(ox + storm.left, oy,
                          storm.right - storm.left, storm.top, storm.animTimer);
    }
    if (storm.bottom < stage.height) {
        drawStormLightning(ox + storm.left, oy + storm.bottom,
                          storm.right - storm.left,
                          stage.height - storm.bottom, storm.animTimer);
    }

    // === Layer 4: Safe zone border (crackling energy line) ===
    SDL_Color borderColor = {100, 200, 255, static_cast<uint8_t>(200 + 55 * fastPulse)};
    float borderThickness = 3.0f + 2.0f * pulse;

    // Draw safe zone outline
    float safeX = ox + storm.left;
    float safeY = oy + storm.top;
    float safeW = storm.right - storm.left;
    float safeH = storm.bottom - storm.top;

    renderer.drawRectOutline(safeX, safeY, safeW, safeH, borderColor, borderThickness);
}

void StormManager::drawStormLightning(float zoneX, float zoneY, float zoneW, float zoneH,
                                      float animTimer) {
    if (zoneW < 5 || zoneH < 5) return;

    auto& renderer = Renderer::instance();

    // Draw multiple lightning bolts in the zone
    int numBolts = static_cast<int>((zoneW * zoneH) / 3000.0f) + 2;
    numBolts = std::min(numBolts, 8);

    for (int b = 0; b < numBolts; ++b) {
        // Pseudo-random position based on animation timer and bolt index
        float boltSeed = animTimer * 3.0f + b * 7.3f;
        float boltX = zoneX + zoneW * (0.1f + 0.8f * std::abs(std::sin(boltSeed * 1.7f)));
        float boltY = zoneY + zoneH * (0.1f + 0.8f * std::abs(std::cos(boltSeed * 2.3f)));

        // Flicker effect - some bolts visible, some not
        float flicker = std::sin(animTimer * 20.0f + b * 4.1f);
        if (flicker < 0.2f) continue;

        uint8_t boltAlpha = static_cast<uint8_t>(150 + 105 * flicker);

        // Alternate colors
        SDL_Color boltColor = (b % 3 == 0) ?
            SDL_Color{150, 220, 255, boltAlpha} :  // Cyan
            (b % 3 == 1) ?
            SDL_Color{220, 180, 255, boltAlpha} :  // Purple
            SDL_Color{255, 255, 255, boltAlpha};   // White

        // Draw a jagged lightning bolt (3-4 segments)
        float segLen = 15.0f + 10.0f * std::sin(boltSeed);
        float x1 = boltX;
        float y1 = boltY;

        for (int seg = 0; seg < 4; ++seg) {
            float angle = -1.57f + std::sin(boltSeed + seg * 2.1f) * 0.8f;
            float x2 = x1 + std::cos(angle) * segLen;
            float y2 = y1 + std::sin(angle) * segLen;

            renderer.drawLine(x1, y1, x2, y2, boltColor, 2.0f);

            x1 = x2;
            y1 = y2;
        }
    }
}

} // namespace ScrapHeap
