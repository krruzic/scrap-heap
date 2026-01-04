#include "battle_hud.h"
#include "renderer.h"
#include "components.h"
#include "utils.h"
#include <cmath>
#include <algorithm>

namespace ScrapHeap {

namespace {

void drawSpecialIcon(Renderer& r, float cx, float cy, float size, int specialIndex, SDL_Color color) {
    const auto& special = ComponentRegistry::instance().getSpecial(specialIndex);
    const std::string& name = special.name;

    if (name == "Boost") {
        // Arrow pointing right
        r.drawTriangle(cx + size*0.5f, cy, cx - size*0.3f, cy - size*0.4f, cx - size*0.3f, cy + size*0.4f, color, true);
    } else if (name == "Anchor") {
        // Anchor shape
        r.drawRect(cx - size*0.1f, cy - size*0.5f, size*0.2f, size*0.7f, color, true);
        r.drawRect(cx - size*0.4f, cy + size*0.2f, size*0.8f, size*0.15f, color, true);
    } else if (name == "Overdrive") {
        // Lightning bolt
        r.drawTriangle(cx + size*0.1f, cy - size*0.5f, cx - size*0.3f, cy, cx + size*0.1f, cy - size*0.1f, color, true);
        r.drawTriangle(cx - size*0.1f, cy + size*0.5f, cx + size*0.3f, cy, cx - size*0.1f, cy + size*0.1f, color, true);
    } else if (name == "Smoke") {
        // Cloud shape (3 circles)
        r.drawCircle(cx - size*0.25f, cy, size*0.25f, color, true);
        r.drawCircle(cx + size*0.25f, cy, size*0.25f, color, true);
        r.drawCircle(cx, cy - size*0.2f, size*0.3f, color, true);
    } else if (name == "Mine") {
        // Circle with spikes
        r.drawCircle(cx, cy, size*0.3f, color, true);
        for (int i = 0; i < 6; i++) {
            float angle = i * 1.047f;
            float x1 = cx + std::cos(angle) * size*0.3f;
            float y1 = cy + std::sin(angle) * size*0.3f;
            float x2 = cx + std::cos(angle) * size*0.5f;
            float y2 = cy + std::sin(angle) * size*0.5f;
            r.drawLine(x1, y1, x2, y2, color, 2.0f);
        }
    } else if (name == "EMP Pulse") {
        // Concentric circles
        r.drawCircleOutline(cx, cy, size*0.2f, color, 2.0f);
        r.drawCircleOutline(cx, cy, size*0.4f, color, 1.5f);
    } else if (name == "Repair Swarm") {
        // Plus sign
        r.drawRect(cx - size*0.4f, cy - size*0.1f, size*0.8f, size*0.2f, color, true);
        r.drawRect(cx - size*0.1f, cy - size*0.4f, size*0.2f, size*0.8f, color, true);
    } else if (name == "Backlash") {
        // Curved arrow (reflect)
        r.drawCircleOutline(cx, cy, size*0.35f, color, 2.0f);
        r.drawTriangle(cx + size*0.35f, cy - size*0.2f, cx + size*0.35f, cy + size*0.1f, cx + size*0.55f, cy - size*0.05f, color, true);
    } else if (name == "Berserk") {
        // Skull-like
        r.drawCircle(cx, cy - size*0.1f, size*0.35f, color, true);
        r.drawRect(cx - size*0.25f, cy + size*0.15f, size*0.5f, size*0.2f, color, true);
    } else {
        // Default: simple diamond
        r.drawTriangle(cx, cy - size*0.4f, cx - size*0.4f, cy, cx + size*0.4f, cy, color, true);
        r.drawTriangle(cx, cy + size*0.4f, cx - size*0.4f, cy, cx + size*0.4f, cy, color, true);
    }
}

void drawPowerupIcon(Renderer& r, float cx, float cy, float size, const Bot& bot) {
    SDL_Color color = {255, 255, 255, 255};

    if (bot.shieldActive) {
        color = {100, 180, 255, 255};
        r.drawCircleOutline(cx, cy, size*0.4f, color, 3.0f);
        r.drawCircleOutline(cx, cy, size*0.25f, color, 2.0f);
    } else if (bot.speedBoostTimer > 0) {
        color = {255, 255, 80, 255};
        r.drawTriangle(cx + size*0.4f, cy, cx - size*0.2f, cy - size*0.35f, cx - size*0.2f, cy + size*0.35f, color, true);
        r.drawTriangle(cx + size*0.1f, cy, cx - size*0.4f, cy - size*0.2f, cx - size*0.4f, cy + size*0.2f, color, true);
    } else if (bot.damageBoostTimer > 0) {
        color = {255, 80, 80, 255};
        r.drawCircle(cx, cy - size*0.15f, size*0.3f, color, true);
        r.drawRect(cx - size*0.25f, cy + size*0.1f, size*0.5f, size*0.25f, color, true);
    } else if (bot.boostActive) {
        color = {80, 200, 255, 255};
        r.drawTriangle(cx + size*0.5f, cy, cx - size*0.3f, cy - size*0.4f, cx - size*0.3f, cy + size*0.4f, color, true);
    } else if (bot.berserkActive) {
        color = {255, 50, 50, 255};
        r.drawCircle(cx, cy - size*0.1f, size*0.35f, color, true);
        r.drawRect(cx - size*0.25f, cy + size*0.15f, size*0.5f, size*0.2f, color, true);
    } else if (bot.heldPowerup >= 0) {
        color = {200, 200, 50, 255};
        r.drawRect(cx - size*0.35f, cy - size*0.35f, size*0.7f, size*0.7f, color, true);
        r.drawText("?", cx, cy - size*0.25f, r.getFontSmall(), {0,0,0,255}, TextAlign::Center);
    }
}

}

void BattleHUD::render(const std::vector<Bot>& bots,
                       const std::map<int, BotBattleStats>& stats,
                       float matchTimer, float maxMatchTime) {
    renderTimer(matchTimer, maxMatchTime);
    for (size_t i = 0; i < bots.size() && i < 4; ++i) {
        auto it = stats.find(bots[i].playerIndex);
        int kills = (it != stats.end()) ? it->second.kills : 0;
        renderCornerHUD(bots[i], static_cast<int>(i), kills, matchTimer);
    }
}

void BattleHUD::renderTimer(float matchTimer, float maxMatchTime) {
    auto& r = Renderer::instance();
    float timeLeft = std::max(0.0f, maxMatchTime - matchTimer);
    int minutes = static_cast<int>(timeLeft) / 60;
    int seconds = static_cast<int>(timeLeft) % 60;

    char timerStr[16];
    snprintf(timerStr, sizeof(timerStr), "%d:%02d", minutes, seconds);

    float boxW = 80, boxH = 28;
    float boxX = WINDOW_WIDTH / 2.0f - boxW / 2.0f, boxY = 6;

    r.drawRect(boxX, boxY, boxW, boxH, {10, 10, 20, 255}, true);
    r.drawRectOutline(boxX, boxY, boxW, boxH, {100, 100, 120, 255}, 3.0f);

    SDL_Color timerColor = (timeLeft <= 30) ? SDL_Color{255, 80, 80, 255} : SDL_Color{80, 255, 80, 255};
    r.drawText(timerStr, WINDOW_WIDTH / 2.0f, boxY + 7, r.getFontMedium(), timerColor, TextAlign::Center);
}

void BattleHUD::renderCornerHUD(const Bot& bot, int position, int kills, float matchTimer) {
    auto& r = Renderer::instance();

    float pad = 8, boxW = 130, boxH = 80;
    bool isRight = (position == 1 || position == 3);
    bool isBottom = (position == 2 || position == 3);

    float bx = isRight ? (WINDOW_WIDTH - boxW - pad) : pad;
    float by = isBottom ? (WINDOW_HEIGHT - boxH - pad) : pad;

    SDL_Color playerColor = Renderer::getPlayerColor(bot.colorIndex);
    if (!bot.isAlive || bot.isRespawning) playerColor = {80, 80, 80, 255};

    r.drawRect(bx, by, boxW, boxH, {15, 15, 25, 220}, true);
    r.drawRect(bx, by, boxW, 5, playerColor, true);

    char pNum[8];
    snprintf(pNum, sizeof(pNum), "P%d", position + 1);
    r.drawText(pNum, bx + 8, by + 8, r.getFontSmall(), playerColor, TextAlign::Left);

    SDL_Color koColor = kills > 0 ? SDL_Color{100, 255, 100, 255} : SDL_Color{150, 150, 150, 255};
    char killStr[16];
    snprintf(killStr, sizeof(killStr), "%d KO", kills);
    r.drawText(killStr, bx + boxW - 8, by + 8, r.getFontSmall(), koColor, TextAlign::Right);

    float barX = bx + 6, barY = by + 24, barW = boxW - 12, barH = 14;
    r.drawRect(barX, barY, barW, barH, {30, 30, 40, 255}, true);

    float healthRatio = clamp(bot.health / bot.maxHealth, 0.0f, 1.0f);
    float healthW = barW * healthRatio;

    if (healthW > 0 && bot.isAlive) {
        SDL_Color bright = {
            static_cast<Uint8>(std::min(255, playerColor.r + 40)),
            static_cast<Uint8>(std::min(255, playerColor.g + 40)),
            static_cast<Uint8>(std::min(255, playerColor.b + 40)), 255
        };
        r.drawRect(barX, barY, healthW, barH / 2, bright, true);
        r.drawRect(barX, barY + barH / 2, healthW, barH / 2, playerColor, true);
    }
    r.drawRectOutline(barX, barY, barW, barH, {80, 80, 100, 255}, 2.0f);

    char hpStr[16];
    if (bot.isRespawning) snprintf(hpStr, sizeof(hpStr), "%.1fs", bot.respawnTimer);
    else snprintf(hpStr, sizeof(hpStr), "%d%%", static_cast<int>(healthRatio * 100));
    r.drawText(hpStr, bx + boxW / 2, barY + 2, r.getFontSmall(), {255, 255, 255, 200}, TextAlign::Center);

    float circleY = barY + barH + 18;
    float iconSize = 20.0f;
    float spacing = 50.0f;
    float startX = bx + boxW / 2 - spacing / 2;

    r.drawCircle(startX, circleY, iconSize * 0.8f, {30, 30, 40, 200}, true);
    r.drawCircleOutline(startX, circleY, iconSize * 0.8f, {80, 80, 100, 255}, 2.0f);

    if (bot.isAlive && !bot.isRespawning && bot.specialCooldown <= 0) {
        drawSpecialIcon(r, startX, circleY, iconSize * 0.7f, bot.specialIndex, {80, 255, 80, 255});
    }

    float pwrX = startX + spacing;
    r.drawCircle(pwrX, circleY, iconSize * 0.8f, {30, 30, 40, 200}, true);
    r.drawCircleOutline(pwrX, circleY, iconSize * 0.8f, {80, 80, 100, 255}, 2.0f);

    bool hasPowerup = bot.shieldActive || bot.speedBoostTimer > 0 || bot.damageBoostTimer > 0 ||
                      bot.boostActive || bot.berserkActive || bot.heldPowerup >= 0;
    if (hasPowerup && bot.isAlive) {
        drawPowerupIcon(r, pwrX, circleY, iconSize * 0.7f, bot);
    }
}

void BattleHUD::renderPlayerHUD(const Bot&, int, float) {}
void BattleHUD::renderScoreBoxes(const std::vector<Bot>&, const std::map<int, BotBattleStats>&) {}

void BattleHUD::renderKillPopups(const std::vector<KillPopup>& popups, const std::vector<Bot>& bots) {
    auto& r = Renderer::instance();
    float pad = 8, boxW = 130, boxH = 80;

    for (const auto& popup : popups) {
        for (size_t i = 0; i < bots.size() && i < 4; ++i) {
            if (bots[i].playerIndex == popup.playerIndex) {
                bool isRight = (i == 1 || i == 3);
                bool isBottom = (i == 2 || i == 3);
                float bx = isRight ? (WINDOW_WIDTH - boxW - pad) : pad;
                float by = isBottom ? (WINDOW_HEIGHT - boxH - pad) : pad;

                uint8_t alpha = static_cast<uint8_t>(popup.getAlpha() * 255);
                float popupY = by + 30 - (1.0f - popup.timer) * 25;
                float popupX = isRight ? (bx - 30) : (bx + boxW + 10);
                r.drawText("+1", popupX, popupY, r.getFontMedium(), {100, 255, 100, alpha},
                          isRight ? TextAlign::Right : TextAlign::Left);
                break;
            }
        }
    }
}

void BattleHUD::updateKillPopups(std::vector<KillPopup>& popups, float dt) {
    for (auto& p : popups) p.timer -= dt;
    popups.erase(std::remove_if(popups.begin(), popups.end(),
                 [](const KillPopup& p) { return p.timer <= 0; }), popups.end());
}

}
