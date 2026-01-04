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

    // Render health bars for each bot
    for (size_t i = 0; i < bots.size(); ++i) {
        renderPlayerHUD(bots[i], static_cast<int>(i), matchTimer);
    }

    renderScoreBoxes(bots, stats);
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

void BattleHUD::renderPlayerHUD(const Bot& bot, int position, float matchTimer) {
    auto& renderer = Renderer::instance();

    float hudY = 45;
    float barWidth = 140;
    float barHeight = 16;
    float barSpacing = 24;
    float nameHeight = 14;

    // Calculate position based on number of bots (assume 4 max)
    float totalWidth = 4 * (barWidth + barSpacing) - barSpacing;
    float startX = (WINDOW_WIDTH - totalWidth) / 2.0f;
    float x = startX + position * (barWidth + barSpacing);
    float y = hudY;

    SDL_Color playerColor = Renderer::getPlayerColor(bot.colorIndex);
    SDL_Color darkColor = {
        static_cast<Uint8>(playerColor.r * 0.3f),
        static_cast<Uint8>(playerColor.g * 0.3f),
        static_cast<Uint8>(playerColor.b * 0.3f),
        255
    };

    if (!bot.isAlive) {
        playerColor = {60, 60, 60, 255};
        darkColor = {30, 30, 30, 255};
    }

    // Player name with colored background
    renderer.drawRect(x, y, barWidth, nameHeight, playerColor, true);
    renderer.drawText(bot.displayName, x + barWidth / 2, y + 2,
                     renderer.getFontSmall(), {0, 0, 0, 255}, TextAlign::Center);

    // Health bar frame
    float barY = y + nameHeight + 2;
    SDL_Color frameBg = {20, 20, 30, 255};
    SDL_Color frameBorder = {80, 80, 100, 255};
    renderer.drawRect(x - 2, barY - 2, barWidth + 4, barHeight + 4, frameBg, true);
    renderer.drawRectOutline(x - 2, barY - 2, barWidth + 4, barHeight + 4, frameBorder, 2.0f);

    // Health bar
    float healthRatio = clamp(bot.health / bot.maxHealth, 0.0f, 1.0f);
    float healthW = barWidth * healthRatio;

    renderer.drawRect(x, barY, barWidth, barHeight, darkColor, true);

    if (healthW > 0) {
        SDL_Color brightColor = {
            static_cast<Uint8>(std::min(255, playerColor.r + 30)),
            static_cast<Uint8>(std::min(255, playerColor.g + 30)),
            static_cast<Uint8>(std::min(255, playerColor.b + 30)),
            255
        };
        renderer.drawRect(x, barY, healthW, barHeight / 2, brightColor, true);
        renderer.drawRect(x, barY + barHeight / 2, healthW, barHeight / 2, playerColor, true);
    }

    // HP percentage
    char hpStr[16];
    snprintf(hpStr, sizeof(hpStr), "%d%%", static_cast<int>(healthRatio * 100));
    renderer.drawText(hpStr, x + barWidth / 2, barY + 3,
                     renderer.getFontSmall(), {255, 255, 255, 200}, TextAlign::Center);

    // Ability indicators
    float abilityY = barY + barHeight + 14;
    float iconRadius = 10.0f;
    float iconSpacing = 26.0f;

    const auto& special = ComponentRegistry::instance().getSpecial(bot.specialIndex);
    SDL_Color emptyCircle = {50, 50, 60, 180};
    SDL_Color abilityReady = {100, 255, 100, 255};
    SDL_Color abilityOnCooldown = {80, 80, 90, 200};

    // Special ability circle
    float specialX = x + iconRadius;
    renderer.drawCircle(specialX, abilityY, iconRadius, emptyCircle, true);

    if (bot.specialActiveTimer > 0) {
        float pulse = 0.5f + 0.5f * std::sin(matchTimer * 10.0f);
        SDL_Color activeColor = {255, static_cast<Uint8>(180 + 75 * pulse), 50, 255};
        renderer.drawCircle(specialX, abilityY, iconRadius - 1, activeColor, true);
    } else if (bot.specialCooldown <= 0) {
        renderer.drawCircle(specialX, abilityY, iconRadius - 1, abilityReady, true);
    } else {
        renderer.drawCircle(specialX, abilityY, iconRadius - 1, abilityOnCooldown, true);
    }
    renderer.drawCircleOutline(specialX, abilityY, iconRadius, {100, 100, 120, 255}, 2.0f);

    // Powerup indicators
    float powerup1X = x + iconSpacing + iconRadius;
    float powerup2X = x + iconSpacing * 2 + iconRadius;
    float powerup3X = x + iconSpacing * 3 + iconRadius;

    // Speed boost
    renderer.drawCircle(powerup1X, abilityY, iconRadius, emptyCircle, true);
    if (bot.speedBoostTimer > 0) {
        renderer.drawCircle(powerup1X, abilityY, iconRadius - 1, {255, 255, 80, 255}, true);
    }
    renderer.drawCircleOutline(powerup1X, abilityY, iconRadius, {100, 100, 120, 255}, 2.0f);

    // Damage boost
    renderer.drawCircle(powerup2X, abilityY, iconRadius, emptyCircle, true);
    if (bot.damageBoostTimer > 0) {
        renderer.drawCircle(powerup2X, abilityY, iconRadius - 1, {255, 80, 80, 255}, true);
    }
    renderer.drawCircleOutline(powerup2X, abilityY, iconRadius, {100, 100, 120, 255}, 2.0f);

    // Active ability
    renderer.drawCircle(powerup3X, abilityY, iconRadius, emptyCircle, true);
    if (bot.shieldActive) {
        renderer.drawCircle(powerup3X, abilityY, iconRadius - 1, {100, 180, 255, 255}, true);
    } else if (bot.boostActive) {
        renderer.drawCircle(powerup3X, abilityY, iconRadius - 1, {80, 200, 255, 255}, true);
    } else if (bot.berserkActive) {
        renderer.drawCircle(powerup3X, abilityY, iconRadius - 1, {255, 50, 50, 255}, true);
    }
    renderer.drawCircleOutline(powerup3X, abilityY, iconRadius, {100, 100, 120, 255}, 2.0f);
}

void BattleHUD::renderScoreBoxes(const std::vector<Bot>& bots,
                                 const std::map<int, BotBattleStats>& stats) {
    auto& renderer = Renderer::instance();

    float boxWidth = 64;
    float boxHeight = 48;
    float cornerPad = 8;

    float cornerX[4] = {cornerPad, WINDOW_WIDTH - boxWidth - cornerPad,
                        cornerPad, WINDOW_WIDTH - boxWidth - cornerPad};
    float cornerY[4] = {90, 90,
                        WINDOW_HEIGHT - boxHeight - cornerPad, WINDOW_HEIGHT - boxHeight - cornerPad};

    for (size_t i = 0; i < bots.size() && i < 4; ++i) {
        const auto& bot = bots[i];
        auto it = stats.find(bot.playerIndex);
        int kills = (it != stats.end()) ? it->second.kills : 0;

        SDL_Color playerColor = Renderer::getPlayerColor(bot.colorIndex);
        if (!bot.isAlive) {
            playerColor.r /= 3;
            playerColor.g /= 3;
            playerColor.b /= 3;
        }

        float bx = cornerX[i];
        float by = cornerY[i];

        SDL_Color boxBg = {15, 15, 25, 240};
        renderer.drawRect(bx, by, boxWidth, boxHeight, boxBg, true);
        renderer.drawRect(bx, by, boxWidth, 6, playerColor, true);

        char pNumStr[8];
        snprintf(pNumStr, sizeof(pNumStr), "P%zu", i + 1);
        renderer.drawText(pNumStr, bx + boxWidth / 2, by + 10,
                         renderer.getFontSmall(), {200, 200, 200, 255}, TextAlign::Center);

        SDL_Color koColor = kills > 0 ? SDL_Color{100, 255, 100, 255} : SDL_Color{150, 150, 150, 255};
        renderer.drawText(std::to_string(kills), bx + boxWidth / 2, by + 22,
                         renderer.getFontMedium(), koColor, TextAlign::Center);

        renderer.drawText("KO", bx + boxWidth / 2, by + 38,
                         renderer.getFontSmall(), {120, 120, 140, 255}, TextAlign::Center);
    }
}

void BattleHUD::renderKillPopups(const std::vector<KillPopup>& popups,
                                 const std::vector<Bot>& bots) {
    auto& renderer = Renderer::instance();

    float boxWidth = 64;
    float cornerPad = 8;
    float cornerX[4] = {cornerPad, WINDOW_WIDTH - boxWidth - cornerPad,
                        cornerPad, WINDOW_WIDTH - boxWidth - cornerPad};
    float cornerY[4] = {90, 90,
                        WINDOW_HEIGHT - 48 - cornerPad, WINDOW_HEIGHT - 48 - cornerPad};

    for (const auto& popup : popups) {
        for (size_t i = 0; i < bots.size() && i < 4; ++i) {
            if (bots[i].playerIndex == popup.playerIndex) {
                uint8_t alpha = static_cast<uint8_t>(popup.getAlpha() * 255);
                SDL_Color popupColor = {100, 255, 100, alpha};
                float popupY = cornerY[i] + 18 - (1.0f - popup.timer) * 30;
                renderer.drawText("+1", cornerX[i] + boxWidth + 8, popupY,
                                 renderer.getFontSmall(), popupColor, TextAlign::Left);
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
