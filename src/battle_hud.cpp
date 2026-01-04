#include "battle_hud.h"
#include "renderer.h"
#include "components.h"
#include "utils.h"
#include <cmath>
#include <algorithm>

namespace ScrapHeap {

void BattleHUD::render(const std::vector<Bot>& bots,
                       const std::map<int, BotBattleStats>& stats,
                       float matchTimer,
                       float maxMatchTime) {
    renderTimer(matchTimer, maxMatchTime);

    // Render corner HUD for each player (combines score, HP, and abilities)
    for (size_t i = 0; i < bots.size() && i < 4; ++i) {
        auto it = stats.find(bots[i].playerIndex);
        int kills = (it != stats.end()) ? it->second.kills : 0;
        renderCornerHUD(bots[i], static_cast<int>(i), kills, matchTimer);
    }
}

void BattleHUD::renderTimer(float matchTimer, float maxMatchTime) {
    auto& renderer = Renderer::instance();

    float timeLeft = std::max(0.0f, maxMatchTime - matchTimer);
    int minutes = static_cast<int>(timeLeft) / 60;
    int seconds = static_cast<int>(timeLeft) % 60;

    char timerStr[16];
    snprintf(timerStr, sizeof(timerStr), "%d:%02d", minutes, seconds);

    // Timer box
    float timerBoxW = 80;
    float timerBoxH = 28;
    float timerBoxX = WINDOW_WIDTH / 2.0f - timerBoxW / 2.0f;
    float timerBoxY = 6;

    SDL_Color timerBoxBg = {10, 10, 20, 255};
    SDL_Color timerBoxBorder = {100, 100, 120, 255};
    SDL_Color timerBoxHighlight = {60, 60, 80, 255};

    renderer.drawRect(timerBoxX, timerBoxY, timerBoxW, timerBoxH, timerBoxBg, true);
    renderer.drawRectOutline(timerBoxX, timerBoxY, timerBoxW, timerBoxH, timerBoxBorder, 3.0f);
    renderer.drawRect(timerBoxX + 3, timerBoxY + 3, timerBoxW - 6, 2, timerBoxHighlight, true);

    SDL_Color timerColor = (timeLeft <= 30) ?
        SDL_Color{255, 80, 80, 255} : SDL_Color{80, 255, 80, 255};
    renderer.drawText(timerStr, WINDOW_WIDTH / 2.0f, timerBoxY + 7,
                     renderer.getFontMedium(), timerColor, TextAlign::Center);
}

void BattleHUD::renderCornerHUD(const Bot& bot, int position, int kills, float matchTimer) {
    auto& renderer = Renderer::instance();

    // Corner positions for each player
    // P1: top-left, P2: top-right, P3: bottom-left, P4: bottom-right
    float cornerPad = 8;
    float boxWidth = 120;
    float boxHeight = 70;

    bool isRight = (position == 1 || position == 3);
    bool isBottom = (position == 2 || position == 3);

    float bx = isRight ? (WINDOW_WIDTH - boxWidth - cornerPad) : cornerPad;
    float by = isBottom ? (WINDOW_HEIGHT - boxHeight - cornerPad) : cornerPad;

    SDL_Color playerColor = Renderer::getPlayerColor(bot.colorIndex);
    SDL_Color darkPlayerColor = {
        static_cast<Uint8>(playerColor.r * 0.3f),
        static_cast<Uint8>(playerColor.g * 0.3f),
        static_cast<Uint8>(playerColor.b * 0.3f),
        255
    };

    bool dimmed = !bot.isAlive || bot.isRespawning;
    if (dimmed) {
        playerColor = {80, 80, 80, 255};
        darkPlayerColor = {40, 40, 40, 255};
    }

    // Background box
    SDL_Color boxBg = {15, 15, 25, 220};
    renderer.drawRect(bx, by, boxWidth, boxHeight, boxBg, true);

    // Player color stripe at top
    renderer.drawRect(bx, by, boxWidth, 5, playerColor, true);

    // Player name and kills
    float textY = by + 8;
    char pNumStr[16];
    snprintf(pNumStr, sizeof(pNumStr), "P%d", position + 1);
    renderer.drawText(pNumStr, bx + 8, textY,
                     renderer.getFontSmall(), playerColor, TextAlign::Left);

    // Kills count
    SDL_Color koColor = kills > 0 ? SDL_Color{100, 255, 100, 255} : SDL_Color{150, 150, 150, 255};
    char killStr[16];
    snprintf(killStr, sizeof(killStr), "%d KO", kills);
    renderer.drawText(killStr, bx + boxWidth - 8, textY,
                     renderer.getFontSmall(), koColor, TextAlign::Right);

    // Health bar
    float barX = bx + 6;
    float barY = by + 24;
    float barW = boxWidth - 12;
    float barH = 14;

    // Bar background
    renderer.drawRect(barX, barY, barW, barH, {30, 30, 40, 255}, true);

    // Health fill
    float healthRatio = clamp(bot.health / bot.maxHealth, 0.0f, 1.0f);
    float healthW = barW * healthRatio;

    if (healthW > 0 && bot.isAlive) {
        SDL_Color brightColor = {
            static_cast<Uint8>(std::min(255, playerColor.r + 40)),
            static_cast<Uint8>(std::min(255, playerColor.g + 40)),
            static_cast<Uint8>(std::min(255, playerColor.b + 40)),
            255
        };
        renderer.drawRect(barX, barY, healthW, barH / 2, brightColor, true);
        renderer.drawRect(barX, barY + barH / 2, healthW, barH / 2, playerColor, true);
    }

    renderer.drawRectOutline(barX, barY, barW, barH, {80, 80, 100, 255}, 2.0f);

    // HP percentage text
    char hpStr[16];
    if (bot.isRespawning) {
        float respawnLeft = bot.respawnTimer;
        snprintf(hpStr, sizeof(hpStr), "%.1fs", respawnLeft);
    } else {
        snprintf(hpStr, sizeof(hpStr), "%d%%", static_cast<int>(healthRatio * 100));
    }
    renderer.drawText(hpStr, bx + boxWidth / 2, barY + 2,
                     renderer.getFontSmall(), {255, 255, 255, 200}, TextAlign::Center);

    // Ability indicators - row of circles below health bar
    float abilityY = barY + barH + 10;
    float iconRadius = 8.0f;
    float iconSpacing = 22.0f;
    float iconsStartX = bx + 14;

    SDL_Color emptyCircle = {40, 40, 50, 180};
    SDL_Color abilityReady = {100, 255, 100, 255};
    SDL_Color abilityOnCooldown = {60, 60, 70, 200};

    // Special ability (leftmost)
    float specialX = iconsStartX;
    renderer.drawCircle(specialX, abilityY, iconRadius, emptyCircle, true);

    if (bot.isAlive && !bot.isRespawning) {
        if (bot.specialActiveTimer > 0) {
            float pulse = 0.5f + 0.5f * std::sin(matchTimer * 10.0f);
            SDL_Color activeColor = {255, static_cast<Uint8>(180 + 75 * pulse), 50, 255};
            renderer.drawCircle(specialX, abilityY, iconRadius - 1, activeColor, true);
        } else if (bot.specialCooldown <= 0) {
            renderer.drawCircle(specialX, abilityY, iconRadius - 1, abilityReady, true);
        } else {
            renderer.drawCircle(specialX, abilityY, iconRadius - 1, abilityOnCooldown, true);
        }
    }
    renderer.drawCircleOutline(specialX, abilityY, iconRadius, {80, 80, 100, 255}, 1.5f);

    // Speed boost
    float speedX = iconsStartX + iconSpacing;
    renderer.drawCircle(speedX, abilityY, iconRadius, emptyCircle, true);
    if (bot.speedBoostTimer > 0) {
        renderer.drawCircle(speedX, abilityY, iconRadius - 1, {255, 255, 80, 255}, true);
    }
    renderer.drawCircleOutline(speedX, abilityY, iconRadius, {80, 80, 100, 255}, 1.5f);

    // Damage boost
    float dmgX = iconsStartX + iconSpacing * 2;
    renderer.drawCircle(dmgX, abilityY, iconRadius, emptyCircle, true);
    if (bot.damageBoostTimer > 0) {
        renderer.drawCircle(dmgX, abilityY, iconRadius - 1, {255, 80, 80, 255}, true);
    }
    renderer.drawCircleOutline(dmgX, abilityY, iconRadius, {80, 80, 100, 255}, 1.5f);

    // Active ability (shield/boost/berserk)
    float activeX = iconsStartX + iconSpacing * 3;
    renderer.drawCircle(activeX, abilityY, iconRadius, emptyCircle, true);
    if (bot.shieldActive) {
        renderer.drawCircle(activeX, abilityY, iconRadius - 1, {100, 180, 255, 255}, true);
    } else if (bot.boostActive) {
        renderer.drawCircle(activeX, abilityY, iconRadius - 1, {80, 200, 255, 255}, true);
    } else if (bot.berserkActive) {
        renderer.drawCircle(activeX, abilityY, iconRadius - 1, {255, 50, 50, 255}, true);
    }
    renderer.drawCircleOutline(activeX, abilityY, iconRadius, {80, 80, 100, 255}, 1.5f);

    // Held powerup indicator (if any)
    if (bot.heldPowerup >= 0) {
        float pwrX = iconsStartX + iconSpacing * 4;
        renderer.drawCircle(pwrX, abilityY, iconRadius, {200, 200, 50, 255}, true);
        renderer.drawCircleOutline(pwrX, abilityY, iconRadius, {255, 255, 100, 255}, 1.5f);
    }
}

void BattleHUD::renderPlayerHUD(const Bot& bot, int position, float matchTimer) {
    // Deprecated - now using renderCornerHUD
}

void BattleHUD::renderScoreBoxes(const std::vector<Bot>& bots,
                                 const std::map<int, BotBattleStats>& stats) {
    // Deprecated - now integrated into renderCornerHUD
}

void BattleHUD::renderKillPopups(const std::vector<KillPopup>& popups,
                                 const std::vector<Bot>& bots) {
    auto& renderer = Renderer::instance();

    float cornerPad = 8;
    float boxWidth = 120;
    float boxHeight = 70;

    for (const auto& popup : popups) {
        for (size_t i = 0; i < bots.size() && i < 4; ++i) {
            if (bots[i].playerIndex == popup.playerIndex) {
                bool isRight = (i == 1 || i == 3);
                bool isBottom = (i == 2 || i == 3);

                float bx = isRight ? (WINDOW_WIDTH - boxWidth - cornerPad) : cornerPad;
                float by = isBottom ? (WINDOW_HEIGHT - boxHeight - cornerPad) : cornerPad;

                uint8_t alpha = static_cast<uint8_t>(popup.getAlpha() * 255);
                SDL_Color popupColor = {100, 255, 100, alpha};
                float popupY = by + 30 - (1.0f - popup.timer) * 25;
                float popupX = isRight ? (bx - 30) : (bx + boxWidth + 10);

                renderer.drawText("+1", popupX, popupY,
                                 renderer.getFontMedium(), popupColor,
                                 isRight ? TextAlign::Right : TextAlign::Left);
                break;
            }
        }
    }
}

void BattleHUD::updateKillPopups(std::vector<KillPopup>& popups, float dt) {
    for (auto& popup : popups) {
        popup.timer -= dt;
    }
    popups.erase(
        std::remove_if(popups.begin(), popups.end(),
                      [](const KillPopup& p) { return p.timer <= 0; }),
        popups.end());
}

} // namespace ScrapHeap
