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
    if (currentPhase <= 1) return 1.0f;
    if (currentPhase == 2) return 3.0f;
    return 5.0f;
}

float Storm::getOverlapFraction(float botX, float botY, float botRadius) const {
    if (!active) return 0.0f;
    if (isOutside(botX, botY)) return 1.0f;

    float distToLeft = botX - left;
    float distToRight = right - botX;
    float distToTop = botY - top;
    float distToBottom = bottom - botY;
    float minDist = std::min({distToLeft, distToRight, distToTop, distToBottom});

    if (minDist >= botRadius) return 0.0f;
    return std::max(0.0f, std::min(1.0f, (botRadius - minDist) / botRadius));
}

void StormManager::update(Storm& storm, const StageDef& stage, float matchTimer, float dt) {
    storm.animTimer += dt;
    storm.pulseTimer += dt;

    if (!storm.active && matchTimer >= Storm::INITIAL_DELAY) {
        storm.active = true;
        storm.currentPhase = 1;
        storm.phaseTimer = 0.0f;
    }

    if (!storm.active) return;

    storm.phaseTimer += dt;

    if (storm.phaseTimer >= Storm::PHASE_DURATION && storm.currentPhase < Storm::MAX_PHASES) {
        storm.currentPhase++;
        storm.phaseTimer = 0.0f;

        float shrinkFactor = 0.85f - storm.currentPhase * 0.1f;
        float centerX = stage.width / 2.0f;
        float centerY = stage.height / 2.0f;
        float halfW = std::max((stage.width / 2.0f) * shrinkFactor, Storm::FINAL_RADIUS);
        float halfH = std::max((stage.height / 2.0f) * shrinkFactor, Storm::FINAL_RADIUS);

        storm.targetLeft = centerX - halfW;
        storm.targetRight = centerX + halfW;
        storm.targetTop = centerY - halfH;
        storm.targetBottom = centerY + halfH;
    }

    float speed = storm.getShrinkSpeed() * dt;
    if (storm.left < storm.targetLeft) storm.left = std::min(storm.left + speed, storm.targetLeft);
    if (storm.right > storm.targetRight) storm.right = std::max(storm.right - speed, storm.targetRight);
    if (storm.top < storm.targetTop) storm.top = std::min(storm.top + speed, storm.targetTop);
    if (storm.bottom > storm.targetBottom) storm.bottom = std::max(storm.bottom - speed, storm.targetBottom);
}

void StormManager::applyDamage(Storm& storm, std::vector<Bot>& bots,
                               std::vector<CombatEvent>& events, float dt) {
    if (!storm.active) return;

    float damagePercent = storm.getDamagePercent();
    static float damageEventTimer = 0.0f;
    damageEventTimer += dt;

    for (auto& bot : bots) {
        if (!bot.isAlive || bot.isRespawning) continue;

        float overlap = storm.getOverlapFraction(bot.x, bot.y, bot.radius);
        if (overlap > 0) {
            float damage = (damagePercent / 100.0f) * bot.maxHealth * dt * overlap;
            bot.health -= damage;

            if (damageEventTimer >= 0.5f) {
                CombatEvent event;
                event.type = CombatEvent::Type::Damage;
                event.x = bot.x;
                event.y = bot.y;
                event.value = damage * 10;
                event.timer = 0.5f;
                events.push_back(event);
            }

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

    if (damageEventTimer >= 0.5f) damageEventTimer = 0.0f;
}

void StormManager::render(const Storm& storm, const StageDef& stage,
                          float cameraOffsetX, float cameraOffsetY) {
    if (!storm.active) return;

    auto& r = Renderer::instance();
    float ox = cameraOffsetX, oy = cameraOffsetY;
    float pulse = 0.7f + 0.3f * std::sin(storm.pulseTimer * 2.0f);
    float fastPulse = 0.5f + 0.5f * std::sin(storm.animTimer * 5.0f);

    uint8_t baseAlpha = static_cast<uint8_t>(100 + 30 * pulse);
    SDL_Color stormBase = {30, 10, 60, baseAlpha};

    if (storm.left > 0)
        r.drawRect(ox, oy, storm.left, stage.height, stormBase, true);
    if (storm.right < stage.width)
        r.drawRect(ox + storm.right, oy, stage.width - storm.right, stage.height, stormBase, true);
    if (storm.top > 0)
        r.drawRect(ox + storm.left, oy, storm.right - storm.left, storm.top, stormBase, true);
    if (storm.bottom < stage.height)
        r.drawRect(ox + storm.left, oy + storm.bottom, storm.right - storm.left, stage.height - storm.bottom, stormBase, true);

    uint8_t energyAlpha = static_cast<uint8_t>(30 + 25 * fastPulse);
    SDL_Color energyColor = {80, 40, 150, energyAlpha};

    if (storm.left > 0)
        r.drawRect(ox, oy, storm.left, stage.height, energyColor, true);
    if (storm.right < stage.width)
        r.drawRect(ox + storm.right, oy, stage.width - storm.right, stage.height, energyColor, true);
    if (storm.top > 0)
        r.drawRect(ox + storm.left, oy, storm.right - storm.left, storm.top, energyColor, true);
    if (storm.bottom < stage.height)
        r.drawRect(ox + storm.left, oy + storm.bottom, storm.right - storm.left, stage.height - storm.bottom, energyColor, true);

    if (storm.left > 0)
        drawStormLightning(ox, oy, storm.left, stage.height, storm.animTimer);
    if (storm.right < stage.width)
        drawStormLightning(ox + storm.right, oy, stage.width - storm.right, stage.height, storm.animTimer);
    if (storm.top > 0)
        drawStormLightning(ox + storm.left, oy, storm.right - storm.left, storm.top, storm.animTimer);
    if (storm.bottom < stage.height)
        drawStormLightning(ox + storm.left, oy + storm.bottom, storm.right - storm.left, stage.height - storm.bottom, storm.animTimer);

    SDL_Color borderColor = {100, 200, 255, static_cast<uint8_t>(180 + 55 * fastPulse)};
    float borderThickness = 3.0f + 2.0f * pulse;
    r.drawRectOutline(ox + storm.left, oy + storm.top, storm.right - storm.left, storm.bottom - storm.top, borderColor, borderThickness);
}

void StormManager::drawStormLightning(float zoneX, float zoneY, float zoneW, float zoneH, float animTimer) {
    if (zoneW < 5 || zoneH < 5) return;

    auto& r = Renderer::instance();
    int numBolts = std::min(static_cast<int>((zoneW * zoneH) / 3000.0f) + 2, 8);

    for (int b = 0; b < numBolts; ++b) {
        float boltSeed = animTimer * 3.0f + b * 7.3f;
        float boltX = zoneX + zoneW * (0.1f + 0.8f * std::abs(std::sin(boltSeed * 1.7f)));
        float boltY = zoneY + zoneH * (0.1f + 0.8f * std::abs(std::cos(boltSeed * 2.3f)));

        float flicker = std::sin(animTimer * 20.0f + b * 4.1f);
        if (flicker < 0.2f) continue;

        uint8_t boltAlpha = static_cast<uint8_t>(120 + 80 * flicker);
        SDL_Color boltColor = (b % 3 == 0) ? SDL_Color{150, 220, 255, boltAlpha} :
                              (b % 3 == 1) ? SDL_Color{220, 180, 255, boltAlpha} :
                                             SDL_Color{255, 255, 255, boltAlpha};

        float segLen = 15.0f + 10.0f * std::sin(boltSeed);
        float x1 = boltX, y1 = boltY;

        for (int seg = 0; seg < 4; ++seg) {
            float angle = -1.57f + std::sin(boltSeed + seg * 2.1f) * 0.8f;
            float x2 = x1 + std::cos(angle) * segLen;
            float y2 = y1 + std::sin(angle) * segLen;
            r.drawLine(x1, y1, x2, y2, boltColor, 2.0f);
            x1 = x2;
            y1 = y2;
        }
    }
}

}
