#include "menu.h"
#include "game_context.h"
#include "renderer.h"
#include "data.h"
#include "utils.h"
#include <cmath>
#include <cstdlib>

namespace ScrapHeap {

// MenuParticle implementation
void MenuParticle::reset(float screenWidth, float screenHeight) {
    x = static_cast<float>(rand() % static_cast<int>(screenWidth));
    y = static_cast<float>(rand() % static_cast<int>(screenHeight));
    vx = (rand() % 100 - 50) / 100.0f * 20.0f;
    vy = (rand() % 100 - 50) / 100.0f * 20.0f - 15.0f;  // Slight upward bias
    size = 2.0f + (rand() % 100) / 20.0f;
    alpha = 0.1f + (rand() % 100) / 400.0f;
    hue = (rand() % 100) / 100.0f;
}

void MenuParticle::update(float dt, float screenWidth, float screenHeight) {
    x += vx * dt;
    y += vy * dt;

    // Wrap around screen
    if (x < -size) x = screenWidth + size;
    if (x > screenWidth + size) x = -size;
    if (y < -size) y = screenHeight + size;
    if (y > screenHeight + size) y = -size;
}

// MenuBackground implementation
void MenuBackground::init(float width, float height) {
    screenWidth = width;
    screenHeight = height;
    for (auto& p : particles) {
        p.reset(screenWidth, screenHeight);
    }
}

void MenuBackground::update(float dt) {
    time += dt;
    for (auto& p : particles) {
        p.update(dt, screenWidth, screenHeight);
    }
}

void MenuBackground::render() {
    auto& renderer = Renderer::instance();

    // Draw gradient background
    SDL_Color topColor = {15, 20, 35, 255};
    SDL_Color bottomColor = {35, 25, 50, 255};
    renderer.drawGradientRect(0, 0, screenWidth, screenHeight, topColor, bottomColor);

    // Draw subtle grid pattern
    SDL_Color gridColor = {255, 255, 255, 8};
    float gridSize = 50.0f;
    for (float gx = 0; gx < screenWidth; gx += gridSize) {
        renderer.drawRect(gx, 0, 1, screenHeight, gridColor, true);
    }
    for (float gy = 0; gy < screenHeight; gy += gridSize) {
        renderer.drawRect(0, gy, screenWidth, 1, gridColor, true);
    }

    // Draw particles
    for (const auto& p : particles) {
        // Color cycles through orange/yellow/cyan based on hue and time
        float hueShift = p.hue + time * 0.1f;
        hueShift = hueShift - std::floor(hueShift);

        Uint8 r, g, b;
        if (hueShift < 0.33f) {
            // Orange to yellow
            r = 255;
            g = static_cast<Uint8>(150 + 105 * (hueShift / 0.33f));
            b = 50;
        } else if (hueShift < 0.66f) {
            // Yellow to cyan
            float t = (hueShift - 0.33f) / 0.33f;
            r = static_cast<Uint8>(255 * (1 - t));
            g = 255;
            b = static_cast<Uint8>(50 + 205 * t);
        } else {
            // Cyan to orange
            float t = (hueShift - 0.66f) / 0.34f;
            r = static_cast<Uint8>(50 + 205 * t);
            g = static_cast<Uint8>(255 * (1 - t * 0.4f));
            b = static_cast<Uint8>(255 * (1 - t));
        }

        SDL_Color particleColor = {r, g, b, static_cast<Uint8>(p.alpha * 255)};
        renderer.drawCircle(p.x, p.y, p.size, particleColor, true);
    }

    // Draw decorative corner accents
    SDL_Color accentColor = {255, 180, 50, 40};
    float cornerSize = 80.0f;
    // Top-left
    renderer.drawRect(0, 0, cornerSize, 3, accentColor, true);
    renderer.drawRect(0, 0, 3, cornerSize, accentColor, true);
    // Top-right
    renderer.drawRect(screenWidth - cornerSize, 0, cornerSize, 3, accentColor, true);
    renderer.drawRect(screenWidth - 3, 0, 3, cornerSize, accentColor, true);
    // Bottom-left
    renderer.drawRect(0, screenHeight - 3, cornerSize, 3, accentColor, true);
    renderer.drawRect(0, screenHeight - cornerSize, 3, cornerSize, accentColor, true);
    // Bottom-right
    renderer.drawRect(screenWidth - cornerSize, screenHeight - 3, cornerSize, 3, accentColor, true);
    renderer.drawRect(screenWidth - 3, screenHeight - cornerSize, 3, cornerSize, accentColor, true);
}

// Virtual keyboard layout
const char* const VirtualKeyboard::LAYOUT[ROWS] = {
    "QWERTYUIOP",
    "ASDFGHJKL ",  // Space at end
    "ZXCVBNM<",    // < is DEL
    "   DONE   "   // DONE button
};

const int VirtualKeyboard::ROW_LENGTHS[ROWS] = {10, 10, 8, 10};

VirtualKeyboard::VirtualKeyboard() {
    reset();
}

void VirtualKeyboard::reset() {
    input.clear();
    cursorRow = 0;
    cursorCol = 0;
    confirmed = false;
    cancelled = false;
}

char VirtualKeyboard::getCharAt(int row, int col) const {
    if (row < 0 || row >= ROWS) return '\0';
    if (col < 0 || col >= ROW_LENGTHS[row]) return '\0';
    return LAYOUT[row][col];
}

bool VirtualKeyboard::isSpecialKey(int row, int col) const {
    if (row == 1 && col == 9) return true;  // Space
    if (row == 2 && col == 7) return true;  // DEL
    if (row == 3) return true;              // DONE row
    return false;
}

std::string VirtualKeyboard::getSpecialKeyLabel(int row, int col) const {
    if (row == 1 && col == 9) return "SPC";
    if (row == 2 && col == 7) return "DEL";
    if (row == 3) return "DONE";
    return "";
}

bool VirtualKeyboard::handleInput(const ControllerState* controller, const KeyboardState* keyboard) {
    // Navigation
    bool up = (controller && controller->dpadUpPressed()) || (keyboard && keyboard->upPressed());
    bool down = (controller && controller->dpadDownPressed()) || (keyboard && keyboard->downPressed());
    bool left = (controller && controller->dpadLeftPressed()) || (keyboard && keyboard->leftPressed());
    bool right = (controller && controller->dpadRightPressed()) || (keyboard && keyboard->rightPressed());
    bool confirm = (controller && controller->buttonAPressed()) || (keyboard && keyboard->enterPressed());
    bool cancel = (controller && controller->buttonBPressed()) || (keyboard && keyboard->escapePressed());

    if (cancel) {
        cancelled = true;
        return false;
    }

    if (up) {
        cursorRow = (cursorRow - 1 + ROWS) % ROWS;
        cursorCol = std::min(cursorCol, ROW_LENGTHS[cursorRow] - 1);
    }
    if (down) {
        cursorRow = (cursorRow + 1) % ROWS;
        cursorCol = std::min(cursorCol, ROW_LENGTHS[cursorRow] - 1);
    }
    if (left) {
        cursorCol = (cursorCol - 1 + ROW_LENGTHS[cursorRow]) % ROW_LENGTHS[cursorRow];
    }
    if (right) {
        cursorCol = (cursorCol + 1) % ROW_LENGTHS[cursorRow];
    }

    if (confirm) {
        if (cursorRow == 3) {
            // DONE
            confirmed = true;
            return false;
        } else if (cursorRow == 2 && cursorCol == 7) {
            // DEL
            if (!input.empty()) {
                input.pop_back();
            }
        } else if (cursorRow == 1 && cursorCol == 9) {
            // Space
            if (input.length() < MAX_TAG_LENGTH) {
                input += ' ';
            }
        } else {
            // Regular character
            char c = getCharAt(cursorRow, cursorCol);
            if (c != '\0' && input.length() < MAX_TAG_LENGTH) {
                input += c;
            }
        }
    }

    return true;  // Still active
}

void VirtualKeyboard::render(float centerX, float centerY) {
    auto& renderer = Renderer::instance();

    float keyWidth = 44.0f;
    float keyHeight = 44.0f;
    float spacing = 6.0f;

    // Calculate total keyboard size
    float kbWidth = 10 * (keyWidth + spacing);
    float kbHeight = ROWS * (keyHeight + spacing) + 60;

    // Draw keyboard background panel
    SDL_Color panelBg = {25, 30, 45, 240};
    SDL_Color panelBorder = {100, 120, 180, 255};
    renderer.drawPanel(centerX - kbWidth / 2 - 20, centerY - 100,
                      kbWidth + 40, kbHeight + 40, panelBg, panelBorder, 3);

    // Draw input field with styling
    SDL_Color inputBg = {15, 18, 28, 255};
    SDL_Color inputBorder = {80, 100, 150, 255};
    float inputFieldY = centerY - 80;
    renderer.drawRect(centerX - 150, inputFieldY - 5, 300, 35, inputBg, true);
    renderer.drawRectOutline(centerX - 150, inputFieldY - 5, 300, 35, inputBorder, 2);

    // Draw current input with cursor
    SDL_Color inputColor = {255, 255, 255, 255};
    std::string displayInput = input + "_";
    renderer.drawText(displayInput, centerX, inputFieldY,
                     renderer.getFontMedium(), inputColor, TextAlign::Center);

    // Draw keyboard rows
    for (int row = 0; row < ROWS; ++row) {
        int cols = ROW_LENGTHS[row];
        float rowWidth = cols * (keyWidth + spacing) - spacing;
        float startX = centerX - rowWidth / 2.0f;
        float y = centerY + row * (keyHeight + spacing);

        for (int col = 0; col < cols; ++col) {
            float x = startX + col * (keyWidth + spacing);

            bool selected = (row == cursorRow && col == cursorCol);

            // Handle DONE button specially (spans multiple cells)
            if (row == 3) {
                if (col == 3) {
                    float doneWidth = 4 * (keyWidth + spacing) - spacing;

                    if (selected) {
                        // Glowing selected state
                        SDL_Color glow = {100, 255, 150, 60};
                        renderer.drawRect(x - 4, y - 4, doneWidth + 8, keyHeight + 8, glow, true);
                    }

                    // Button background with gradient effect
                    SDL_Color doneBgTop = selected ? SDL_Color{60, 180, 100, 255} : SDL_Color{50, 80, 60, 255};
                    SDL_Color doneBgBot = selected ? SDL_Color{40, 140, 70, 255} : SDL_Color{35, 55, 40, 255};
                    renderer.drawGradientRect(x, y, doneWidth, keyHeight, doneBgTop, doneBgBot);

                    SDL_Color doneBorder = selected ? SDL_Color{100, 255, 150, 255} : SDL_Color{70, 100, 80, 255};
                    renderer.drawRectOutline(x, y, doneWidth, keyHeight, doneBorder, 2);

                    SDL_Color textColor = selected ? SDL_Color{255, 255, 255, 255} : SDL_Color{180, 200, 180, 255};
                    renderer.drawText("DONE", x + doneWidth / 2, y + 10,
                                      renderer.getFontSmall(), textColor, TextAlign::Center);
                }
                continue;
            }

            // Draw key with styling
            if (selected) {
                // Glowing selected state
                SDL_Color glow = {255, 200, 100, 60};
                renderer.drawRect(x - 3, y - 3, keyWidth + 6, keyHeight + 6, glow, true);
            }

            // Key background gradient
            SDL_Color keyBgTop = selected ? SDL_Color{120, 100, 180, 255} : SDL_Color{55, 55, 70, 255};
            SDL_Color keyBgBot = selected ? SDL_Color{80, 60, 140, 255} : SDL_Color{40, 40, 50, 255};
            renderer.drawGradientRect(x, y, keyWidth, keyHeight, keyBgTop, keyBgBot);

            // Key border
            SDL_Color keyBorder = selected ? SDL_Color{180, 160, 255, 255} : SDL_Color{80, 80, 100, 255};
            renderer.drawRectOutline(x, y, keyWidth, keyHeight, keyBorder, selected ? 2.0f : 1.0f);

            // Draw character or label
            SDL_Color textColor = selected ? SDL_Color{255, 255, 255, 255} : SDL_Color{200, 200, 210, 255};
            std::string label;
            if (isSpecialKey(row, col)) {
                label = getSpecialKeyLabel(row, col);
                // Special keys get different color
                if (label == "DEL") {
                    textColor = selected ? SDL_Color{255, 150, 150, 255} : SDL_Color{200, 130, 130, 255};
                } else if (label == "SPC") {
                    textColor = selected ? SDL_Color{150, 200, 255, 255} : SDL_Color{130, 160, 200, 255};
                }
            } else {
                label = std::string(1, getCharAt(row, col));
            }
            renderer.drawText(label, x + keyWidth / 2, y + 10,
                             renderer.getFontSmall(), textColor, TextAlign::Center);
        }
    }
}

// MainMenuScreen
MainMenuScreen::MainMenuScreen() {
    selection = 0;
    background.init(WINDOW_WIDTH, WINDOW_HEIGHT);
}

void MainMenuScreen::enter() {
    selection = 0;
    animTime = 0.0f;
    selectionBounce = 0.0f;
}

void MainMenuScreen::handleInput(GameContext& ctx) {
    auto& input = InputManager::instance();
    const auto& keyboard = input.getKeyboard();
    const ControllerState* controller = input.getController(0);

    bool up = keyboard.upPressed() || (controller && controller->dpadUpPressed());
    bool down = keyboard.downPressed() || (controller && controller->dpadDownPressed());
    bool confirm = keyboard.enterPressed() || keyboard.spacePressed() ||
                  (controller && controller->buttonAPressed());
    bool quit = keyboard.escapePressed() || (controller && controller->buttonBPressed());

    if (up) {
        selection = (selection - 1 + ITEM_COUNT) % ITEM_COUNT;
        selectionBounce = 1.0f;
    }
    if (down) {
        selection = (selection + 1) % ITEM_COUNT;
        selectionBounce = 1.0f;
    }

    if (confirm) {
        switch (selection) {
            case 0: ctx.changeState(GameState::GameSetup); break;
            case 1: ctx.changeState(GameState::Stats); break;
            case 2: ctx.changeState(GameState::Tags); break;
            case 3: ctx.running = false; break;
        }
    }

    if (quit && selection == 3) {
        ctx.running = false;
    }
}

void MainMenuScreen::update(float dt) {
    animTime += dt;
    background.update(dt);

    // Decay selection bounce
    if (selectionBounce > 0) {
        selectionBounce -= dt * 4.0f;
        if (selectionBounce < 0) selectionBounce = 0;
    }
}

void MainMenuScreen::render() {
    auto& renderer = Renderer::instance();

    // Render animated background
    background.render();

    // Title with glow effect
    float titlePulse = 0.5f + 0.5f * std::sin(animTime * 2.0f);
    SDL_Color titleGlow = {255, 180, 50, static_cast<Uint8>(30 + 30 * titlePulse)};
    float titleY = 100.0f + std::sin(animTime * 1.5f) * 3.0f;

    // Draw title glow
    for (int i = 3; i >= 0; --i) {
        SDL_Color glow = {titleGlow.r, titleGlow.g, titleGlow.b,
                         static_cast<Uint8>(titleGlow.a * (4 - i) / 4)};
        renderer.drawText("SCRAP HEAP", WINDOW_WIDTH / 2.0f + i, titleY + i,
                         renderer.getFontLarge(), glow, TextAlign::Center);
    }

    SDL_Color titleColor = {255, 200, 50, 255};
    renderer.drawTextShadow("SCRAP HEAP", WINDOW_WIDTH / 2.0f, titleY,
                           renderer.getFontLarge(), titleColor, TextAlign::Center);

    // Subtitle
    SDL_Color subtitleColor = {150, 140, 120, 180};
    renderer.drawText("BATTLE BOTS ARENA", WINDOW_WIDTH / 2.0f, titleY + 55,
                     renderer.getFontSmall(), subtitleColor, TextAlign::Center);

    // Menu panel
    float panelWidth = 300.0f;
    float panelHeight = 280.0f;
    float panelX = (WINDOW_WIDTH - panelWidth) / 2.0f;
    float panelY = 200.0f;

    SDL_Color panelBg = {30, 35, 50, 220};
    SDL_Color panelBorder = {80, 90, 120, 255};
    renderer.drawPanel(panelX, panelY, panelWidth, panelHeight, panelBg, panelBorder, 2);

    // Menu items
    float startY = panelY + 30.0f;
    float itemSpacing = 55.0f;

    for (int i = 0; i < ITEM_COUNT; ++i) {
        bool selected = (i == selection);
        float itemY = startY + i * itemSpacing;

        // Selection highlight
        if (selected) {
            float bounce = selectionBounce * std::sin(selectionBounce * PI * 2) * 3.0f;
            float highlightAlpha = 0.7f + 0.3f * std::sin(animTime * 4.0f);

            SDL_Color highlight = {255, 200, 100, static_cast<Uint8>(80 * highlightAlpha)};
            renderer.drawRect(panelX + 20, itemY - 5 + bounce, panelWidth - 40, 40, highlight, true);

            SDL_Color highlightBorder = {255, 200, 100, static_cast<Uint8>(180 * highlightAlpha)};
            renderer.drawRectOutline(panelX + 20, itemY - 5 + bounce, panelWidth - 40, 40, highlightBorder, 2);
        }

        SDL_Color itemColor = selected ?
            SDL_Color{255, 255, 150, 255} : SDL_Color{160, 160, 170, 255};

        std::string text = ITEMS[i];
        if (selected) {
            float arrowAnim = std::sin(animTime * 6.0f) * 3.0f;
            // Draw animated arrows
            SDL_Color arrowColor = {255, 200, 100, 200};
            renderer.drawText(">", WINDOW_WIDTH / 2.0f - 80 - arrowAnim, itemY + 3,
                             renderer.getFontMedium(), arrowColor, TextAlign::Center);
            renderer.drawText("<", WINDOW_WIDTH / 2.0f + 80 + arrowAnim, itemY + 3,
                             renderer.getFontMedium(), arrowColor, TextAlign::Center);
        }

        renderer.drawTextShadow(text, WINDOW_WIDTH / 2.0f, itemY,
                               renderer.getFontMedium(), itemColor, TextAlign::Center);
    }

    // Controls hint with styling
    SDL_Color hintBg = {0, 0, 0, 100};
    renderer.drawRect(0, WINDOW_HEIGHT - 35, WINDOW_WIDTH, 35, hintBg, true);

    SDL_Color hintColor = {140, 140, 150, 255};
    renderer.drawText("UP/DOWN: Navigate   A/ENTER: Select   B/ESC: Quit",
                     WINDOW_WIDTH / 2.0f, WINDOW_HEIGHT - 25,
                     renderer.getFontSmall(), hintColor, TextAlign::Center);
}

// StatsScreen
void StatsScreen::enter() {
    scrollOffset = 0;
}

void StatsScreen::handleInput(GameContext& ctx) {
    auto& input = InputManager::instance();
    const auto& keyboard = input.getKeyboard();
    const ControllerState* controller = input.getController(0);

    bool back = keyboard.escapePressed() || (controller && controller->buttonBPressed());
    bool up = keyboard.upPressed() || (controller && controller->dpadUpPressed());
    bool down = keyboard.downPressed() || (controller && controller->dpadDownPressed());

    if (back) {
        ctx.changeState(GameState::MainMenu);
    }

    const auto& stats = DataManager::instance().getStats();
    int maxOffset = std::max(0, static_cast<int>(stats.size()) - 10);

    if (up && scrollOffset > 0) scrollOffset--;
    if (down && scrollOffset < maxOffset) scrollOffset++;
}

void StatsScreen::update(float dt) {
    // Nothing to update
}

void StatsScreen::render() {
    auto& renderer = Renderer::instance();

    // Gradient background
    SDL_Color topColor = {20, 25, 40, 255};
    SDL_Color bottomColor = {35, 30, 55, 255};
    renderer.drawGradientRect(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, topColor, bottomColor);

    // Title panel
    SDL_Color titlePanelBg = {40, 45, 70, 255};
    renderer.drawRect(0, 0, WINDOW_WIDTH, 80, titlePanelBg, true);
    SDL_Color titlePanelBorder = {80, 90, 130, 255};
    renderer.drawRect(0, 78, WINDOW_WIDTH, 2, titlePanelBorder, true);

    SDL_Color titleColor = {255, 200, 50, 255};
    renderer.drawTextShadow("STATS", WINDOW_WIDTH / 2.0f, 25,
                           renderer.getFontLarge(), titleColor, TextAlign::Center);

    // Content panel
    float contentX = 100;
    float contentY = 100;
    float contentW = WINDOW_WIDTH - 200;
    float contentH = WINDOW_HEIGHT - 180;

    SDL_Color contentBg = {25, 30, 45, 240};
    SDL_Color contentBorder = {70, 80, 110, 255};
    renderer.drawPanel(contentX, contentY, contentW, contentH, contentBg, contentBorder, 2);

    // Headers
    float startY = contentY + 20;
    float rowHeight = 35.0f;
    SDL_Color headerColor = {200, 180, 100, 255};
    SDL_Color headerUnderline = {200, 180, 100, 100};

    renderer.drawText("PLAYER", contentX + 50, startY, renderer.getFontSmall(), headerColor, TextAlign::Left);
    renderer.drawText("WINS", contentX + 280, startY, renderer.getFontSmall(), headerColor, TextAlign::Center);
    renderer.drawText("LOSSES", contentX + 380, startY, renderer.getFontSmall(), headerColor, TextAlign::Center);
    renderer.drawText("RATIO", contentX + 490, startY, renderer.getFontSmall(), headerColor, TextAlign::Center);

    renderer.drawRect(contentX + 20, startY + 28, contentW - 40, 2, headerUnderline, true);

    // Stats
    const auto& stats = DataManager::instance().getStats();

    if (stats.empty()) {
        SDL_Color emptyColor = {150, 150, 160, 255};
        renderer.drawText("No stats recorded yet. Play some matches!",
                         WINDOW_WIDTH / 2.0f, contentY + contentH / 2,
                         renderer.getFontSmall(), emptyColor, TextAlign::Center);
    } else {
        int visible = std::min(10, static_cast<int>(stats.size()) - scrollOffset);

        for (int i = 0; i < visible; ++i) {
            const auto& ps = stats[scrollOffset + i];
            float y = startY + 45 + i * rowHeight;

            // Alternating row background
            if (i % 2 == 0) {
                SDL_Color rowBg = {35, 40, 55, 100};
                renderer.drawRect(contentX + 20, y - 5, contentW - 40, rowHeight, rowBg, true);
            }

            SDL_Color rowColor = {220, 220, 230, 255};
            renderer.drawText(ps.tag, contentX + 50, y, renderer.getFontSmall(), rowColor, TextAlign::Left);

            SDL_Color winColor = {100, 255, 150, 255};
            renderer.drawText(std::to_string(ps.wins), contentX + 280, y, renderer.getFontSmall(), winColor, TextAlign::Center);

            SDL_Color lossColor = {255, 130, 130, 255};
            renderer.drawText(std::to_string(ps.losses), contentX + 380, y, renderer.getFontSmall(), lossColor, TextAlign::Center);

            char ratioStr[16];
            snprintf(ratioStr, sizeof(ratioStr), "%.1f%%", ps.getWinRatio() * 100.0f);

            SDL_Color ratioColor = ps.getWinRatio() >= 0.5f ?
                SDL_Color{100, 255, 150, 255} : SDL_Color{255, 180, 100, 255};
            renderer.drawText(ratioStr, contentX + 490, y, renderer.getFontSmall(), ratioColor, TextAlign::Center);
        }
    }

    // Controls hint
    SDL_Color hintBg = {0, 0, 0, 100};
    renderer.drawRect(0, WINDOW_HEIGHT - 35, WINDOW_WIDTH, 35, hintBg, true);

    SDL_Color hintColor = {140, 140, 150, 255};
    renderer.drawText("B/ESC: Back   UP/DOWN: Scroll",
                     WINDOW_WIDTH / 2.0f, WINDOW_HEIGHT - 25,
                     renderer.getFontSmall(), hintColor, TextAlign::Center);
}

// TagsScreen
TagsScreen::TagsScreen() : keyboard() {
    selection = 0;
    enteringNewTag = false;
}

void TagsScreen::enter() {
    selection = 0;
    enteringNewTag = false;
}

void TagsScreen::handleInput(GameContext& ctx) {
    auto& input = InputManager::instance();
    const auto& kb = input.getKeyboard();
    const ControllerState* controller = input.getController(0);

    if (enteringNewTag) {
        if (!keyboard.handleInput(controller, &kb)) {
            if (keyboard.isConfirmed() && !keyboard.getInput().empty()) {
                DataManager::instance().addTag(keyboard.getInput());
            }
            enteringNewTag = false;
            keyboard.reset();
        }
        return;
    }

    bool back = kb.escapePressed() || (controller && controller->buttonBPressed());
    bool up = kb.upPressed() || (controller && controller->dpadUpPressed());
    bool down = kb.downPressed() || (controller && controller->dpadDownPressed());
    bool confirm = kb.enterPressed() || kb.spacePressed() ||
                  (controller && controller->buttonAPressed());
    bool del = (controller && controller->buttonXPressed());

    if (back) {
        ctx.changeState(GameState::MainMenu);
        return;
    }

    const auto& tags = DataManager::instance().getTags();
    int itemCount = static_cast<int>(tags.size()) + 1;  // +1 for "Add New"

    if (up) selection = (selection - 1 + itemCount) % itemCount;
    if (down) selection = (selection + 1) % itemCount;

    if (confirm) {
        if (selection == static_cast<int>(tags.size())) {
            // Add new tag
            enteringNewTag = true;
            keyboard.reset();
        }
    }

    if (del && selection < static_cast<int>(tags.size())) {
        DataManager::instance().removeTag(selection);
        if (selection >= static_cast<int>(DataManager::instance().getTags().size())) {
            selection = std::max(0, static_cast<int>(DataManager::instance().getTags().size()));
        }
    }
}

void TagsScreen::update(float dt) {
    // Nothing to update
}

void TagsScreen::render() {
    auto& renderer = Renderer::instance();

    // Gradient background
    SDL_Color topColor = {20, 25, 40, 255};
    SDL_Color bottomColor = {35, 30, 55, 255};
    renderer.drawGradientRect(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, topColor, bottomColor);

    // Title panel
    SDL_Color titlePanelBg = {40, 45, 70, 255};
    renderer.drawRect(0, 0, WINDOW_WIDTH, 80, titlePanelBg, true);
    SDL_Color titlePanelBorder = {80, 90, 130, 255};
    renderer.drawRect(0, 78, WINDOW_WIDTH, 2, titlePanelBorder, true);

    SDL_Color titleColor = {255, 200, 50, 255};
    renderer.drawTextShadow("PLAYER TAGS", WINDOW_WIDTH / 2.0f, 25,
                           renderer.getFontLarge(), titleColor, TextAlign::Center);

    if (enteringNewTag) {
        keyboard.render(WINDOW_WIDTH / 2.0f, WINDOW_HEIGHT / 2.0f);

        SDL_Color hintBg = {0, 0, 0, 100};
        renderer.drawRect(0, WINDOW_HEIGHT - 35, WINDOW_WIDTH, 35, hintBg, true);

        SDL_Color hintColor = {140, 140, 150, 255};
        renderer.drawText("A/ENTER: Select   B/ESC: Cancel",
                         WINDOW_WIDTH / 2.0f, WINDOW_HEIGHT - 25,
                         renderer.getFontSmall(), hintColor, TextAlign::Center);
        return;
    }

    // Content panel
    float contentX = 150;
    float contentY = 100;
    float contentW = WINDOW_WIDTH - 300;
    float contentH = WINDOW_HEIGHT - 180;

    SDL_Color contentBg = {25, 30, 45, 240};
    SDL_Color contentBorder = {70, 80, 110, 255};
    renderer.drawPanel(contentX, contentY, contentW, contentH, contentBg, contentBorder, 2);

    // Tag list
    const auto& tags = DataManager::instance().getTags();
    float startY = contentY + 30;
    float itemSpacing = 45.0f;

    for (int i = 0; i <= static_cast<int>(tags.size()); ++i) {
        bool selected = (i == selection);
        float itemY = startY + i * itemSpacing;

        // Selection highlight
        if (selected) {
            SDL_Color highlight = {100, 120, 180, 150};
            renderer.drawRect(contentX + 20, itemY - 8, contentW - 40, 38, highlight, true);
            SDL_Color highlightBorder = {150, 170, 230, 200};
            renderer.drawRectOutline(contentX + 20, itemY - 8, contentW - 40, 38, highlightBorder, 2);
        }

        SDL_Color itemColor = selected ?
            SDL_Color{255, 255, 200, 255} : SDL_Color{180, 180, 190, 255};

        std::string text;
        if (i < static_cast<int>(tags.size())) {
            text = tags[i];
            if (selected) text = "> " + text;
        } else {
            SDL_Color addColor = {100, 255, 150, 255};
            if (!selected) addColor = {80, 180, 120, 255};
            text = selected ? "> + ADD NEW TAG" : "+ ADD NEW TAG";
            itemColor = addColor;
        }

        renderer.drawText(text, contentX + 40, itemY,
                         renderer.getFontSmall(), itemColor, TextAlign::Left);
    }

    // Controls hint
    SDL_Color hintBg = {0, 0, 0, 100};
    renderer.drawRect(0, WINDOW_HEIGHT - 35, WINDOW_WIDTH, 35, hintBg, true);

    SDL_Color hintColor = {140, 140, 150, 255};
    renderer.drawText("UP/DOWN: Navigate   A: Select   X: Delete   B: Back",
                     WINDOW_WIDTH / 2.0f, WINDOW_HEIGHT - 25,
                     renderer.getFontSmall(), hintColor, TextAlign::Center);
}

} // namespace ScrapHeap
