#pragma once

#include "utils.h"
#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <string>
#include <vector>

namespace ScrapHeap {

// Forward declarations
struct Bot;
struct BattleState;
struct StageDef;
struct Powerup;
struct Hazard;
struct CombatEvent;
struct Mine;
struct SmokeCloud;

// Text alignment
enum class TextAlign {
    Left,
    Center,
    Right
};

// Renderer utilities
class Renderer {
public:
    static Renderer& instance();

    // Initialize with SDL renderer and load fonts
    bool initialize(SDL_Renderer* renderer, const std::string& fontPath);
    void shutdown();

    // Get renderer
    SDL_Renderer* getRenderer() { return sdlRenderer; }

    // Get fonts
    TTF_Font* getFontLarge() { return fontLarge; }
    TTF_Font* getFontMedium() { return fontMedium; }
    TTF_Font* getFontSmall() { return fontSmall; }

    // Clear screen
    void clear(SDL_Color color = {20, 20, 30, 255});

    // Present
    void present();

    // Draw text
    void drawText(const std::string& text, float x, float y,
                  TTF_Font* font, SDL_Color color,
                  TextAlign align = TextAlign::Left);

    // Draw text with shadow
    void drawTextShadow(const std::string& text, float x, float y,
                        TTF_Font* font, SDL_Color color,
                        TextAlign align = TextAlign::Left);

    // Get text dimensions
    Vec2 getTextSize(const std::string& text, TTF_Font* font);

    // Draw shapes
    void drawRect(float x, float y, float w, float h, SDL_Color color, bool filled = true);
    void drawRectOutline(float x, float y, float w, float h, SDL_Color color, float thickness = 2.0f);
    void drawCircle(float cx, float cy, float radius, SDL_Color color, bool filled = true);
    void drawCircleOutline(float cx, float cy, float radius, SDL_Color color, float thickness = 2.0f);
    void drawLine(float x1, float y1, float x2, float y2, SDL_Color color, float thickness = 1.0f);
    void drawTriangle(float x1, float y1, float x2, float y2, float x3, float y3,
                      SDL_Color color, bool filled = true);

    // Draw rotated rectangle (for bots)
    void drawRotatedRect(float cx, float cy, float width, float height,
                         float angle, SDL_Color color);

    // Draw vertical gradient rectangle
    void drawGradientRect(float x, float y, float w, float h,
                          SDL_Color topColor, SDL_Color bottomColor);

    // Draw horizontal gradient rectangle
    void drawGradientRectH(float x, float y, float w, float h,
                           SDL_Color leftColor, SDL_Color rightColor);

    // Draw rounded rectangle
    void drawRoundedRect(float x, float y, float w, float h, float radius,
                         SDL_Color color, bool filled = true);

    // Draw panel with border and optional glow
    void drawPanel(float x, float y, float w, float h, SDL_Color bgColor,
                   SDL_Color borderColor, float borderThickness = 2.0f);

    // Draw glowing panel with gradient background
    void drawGlowPanel(float x, float y, float w, float h,
                       SDL_Color baseColor, float glowIntensity = 0.5f);

    // Draw a bot
    void drawBot(const Bot& bot, SDL_Color color);

    // Draw bot's weapon visuals
    void drawBotWeapon(const Bot& bot, SDL_Color color);

    // Draw health bar
    void drawHealthBar(float x, float y, float width, float height,
                       float current, float max, SDL_Color fgColor, SDL_Color bgColor);

    // Draw progress bar (for grab escape, cooldowns, etc.)
    void drawProgressBar(float x, float y, float width, float height,
                         float progress, SDL_Color fgColor, SDL_Color bgColor);

    // Draw stage
    void drawStage(const StageDef& stage, float offsetX, float offsetY);

    // Draw hazard
    void drawHazard(const Hazard& hazard, float offsetX, float offsetY);

    // Draw powerup
    void drawPowerup(const Powerup& powerup, float offsetX, float offsetY);

    // Draw combat event effects
    void drawCombatEvent(const CombatEvent& event, float offsetX, float offsetY);

    // Draw mine
    void drawMine(const Mine& mine, float offsetX, float offsetY);

    // Draw smoke cloud
    void drawSmokeCloud(const SmokeCloud& cloud, float offsetX, float offsetY);

    // Draw grab tether between two bots
    void drawGrabTether(const Bot& grabber, const Bot& grabbed, float offsetX, float offsetY);

    // Player colors
    static const std::vector<SDL_Color>& getPlayerColors();
    static SDL_Color getPlayerColor(int index);

private:
    Renderer() = default;

    SDL_Renderer* sdlRenderer = nullptr;
    TTF_Font* fontLarge = nullptr;   // 48pt
    TTF_Font* fontMedium = nullptr;  // 32pt
    TTF_Font* fontSmall = nullptr;   // 20pt

    // Helper for drawing filled circles using triangles
    void drawFilledCircle(float cx, float cy, float radius, SDL_Color color);
};

} // namespace ScrapHeap
