#pragma once

#include "input.h"
#include <SDL3/SDL.h>
#include <string>
#include <vector>
#include <functional>
#include <array>

namespace ScrapHeap {

// Forward declarations
struct GameContext;

// Menu item
struct MenuItem {
    std::string label;
    std::function<void()> action;
    bool enabled = true;
};

// Floating particle for menu backgrounds
struct MenuParticle {
    float x, y;
    float vx, vy;
    float size;
    float alpha;
    float hue;  // For color cycling

    void reset(float screenWidth, float screenHeight);
    void update(float dt, float screenWidth, float screenHeight);
};

// Background effects manager
class MenuBackground {
public:
    static constexpr int PARTICLE_COUNT = 30;

    void init(float screenWidth, float screenHeight);
    void update(float dt);
    void render();

private:
    std::array<MenuParticle, PARTICLE_COUNT> particles;
    float screenWidth = 0;
    float screenHeight = 0;
    float time = 0.0f;
};

// Virtual keyboard for tag entry
class VirtualKeyboard {
public:
    VirtualKeyboard();

    // Reset keyboard state
    void reset();

    // Handle input, returns true if still active
    bool handleInput(const ControllerState* controller, const KeyboardState* keyboard);

    // Get current input string
    const std::string& getInput() const { return input; }

    // Check if entry was confirmed
    bool isConfirmed() const { return confirmed; }

    // Check if entry was cancelled
    bool isCancelled() const { return cancelled; }

    // Render the keyboard
    void render(float centerX, float centerY);

    // Set initial input
    void setInput(const std::string& text) { input = text; }

private:
    std::string input;
    int cursorRow = 0;
    int cursorCol = 0;
    bool confirmed = false;
    bool cancelled = false;

    // Keyboard layout
    static constexpr int ROWS = 4;
    static constexpr int COLS = 10;
    static const char* const LAYOUT[ROWS];
    static const int ROW_LENGTHS[ROWS];

    char getCharAt(int row, int col) const;
    bool isSpecialKey(int row, int col) const;
    std::string getSpecialKeyLabel(int row, int col) const;
};

// Main menu screen
class MainMenuScreen {
public:
    MainMenuScreen();

    void enter();
    void handleInput(GameContext& ctx);
    void update(float dt);
    void render();

private:
    int selection = 0;
    float animTime = 0.0f;  // For animations
    float selectionBounce = 0.0f;  // Selection animation
    MenuBackground background;

    static constexpr int ITEM_COUNT = 4;
    const char* ITEMS[ITEM_COUNT] = {"PLAY", "STATS", "TAGS", "QUIT"};
};

// Stats screen
class StatsScreen {
public:
    void enter();
    void handleInput(GameContext& ctx);
    void update(float dt);
    void render();

private:
    int scrollOffset = 0;
};

// Tags screen
class TagsScreen {
public:
    TagsScreen();

    void enter();
    void handleInput(GameContext& ctx);
    void update(float dt);
    void render();

private:
    int selection = 0;
    bool enteringNewTag = false;
    VirtualKeyboard keyboard;
};

} // namespace ScrapHeap
