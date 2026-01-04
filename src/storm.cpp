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

float Storm::getDamagePercent() const {
    // 1% per second in phase 1, 3% in phase 2, 5% in phase 3+
    if (currentPhase <= 1) return 1.0f;
    if (currentPhase == 2) return 3.0f;
    return 5.0f;
}

float Storm::getOverlapFraction(float botX, float botY, float botRadius) const {
    if (!active) return 0.0f;

    // Check if any part of the bot (circle) overlaps with storm
    // Simple check: if center is outside, full overlap
    // If center is inside but edge touches storm, partial overlap

    bool centerOutside = isOutside(botX, botY);
    if (centerOutside) return 1.0f;

    // Check distance to each wall
    float distToLeft = botX - left;
    float distToRight = right - botX;
    float distToTop = botY - top;
    float distToBottom = bottom - botY;

    float minDist = std::min({distToLeft, distToRight, distToTop, distToBottom});

    // If closest wall is farther than bot radius, no overlap
    if (minDist >= botRadius) return 0.0f;

    // Partial overlap: fraction of radius that's in the storm
    float overlap = (botRadius - minDist) / botRadius;
    return std::max(0.0f, std::min(1.0f, overlap));
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

    float damagePercent = storm.getDamagePercent();

    for (auto& bot : bots) {
        if (!bot.isAlive || bot.isRespawning) continue;

        // Get overlap fraction (0 = fully inside safe zone, 1 = fully in storm)
        float overlap = storm.getOverlapFraction(bot.x, bot.y, bot.radius);

        if (overlap > 0) {
            // Damage = percentage of max health per second * overlap fraction
            float damage = (damagePercent / 100.0f) * bot.maxHealth * dt * overlap;
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
                event.value = damage * 10;  // Show accumulated damage
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

    // === Layer 1: Semi-translucent storm background (~50% opacity) ===
    // Much more translucent so bots are visible underneath
    uint8_t baseAlpha = static_cast<uint8_t>(100 + 30 * pulse);
    SDL_Color stormBase = {30, 10, 60, baseAlpha};

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

    // === Layer 2: Subtle pulsing energy overlay ===
    uint8_t energyAlpha = static_cast<uint8_t>(30 + 25 * fastPulse);
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
    SDL_Color borderColor = {100, 200, 255, static_cast<uint8_t>(180 + 55 * fastPulse)};
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

        uint8_t boltAlpha = static_cast<uint8_t>(120 + 80 * flicker);

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
