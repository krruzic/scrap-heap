#include "menu.h"
#include "game_context.h"
#include "renderer.h"
#include "data.h"
#include "utils.h"

namespace ScrapHeap {

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

    float keyWidth = 40.0f;
    float keyHeight = 40.0f;
    float spacing = 5.0f;

    // Draw current input
    SDL_Color inputColor = {255, 255, 255, 255};
    std::string displayInput = input + "_";
    renderer.drawTextShadow(displayInput, centerX, centerY - 80,
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
            SDL_Color bgColor = selected ? SDL_Color{100, 100, 200, 255} : SDL_Color{60, 60, 70, 255};

            // Handle DONE button specially (spans multiple cells)
            if (row == 3) {
                if (col == 3) {
                    float doneWidth = 4 * (keyWidth + spacing) - spacing;
                    renderer.drawRect(x, y, doneWidth, keyHeight, bgColor, true);
                    SDL_Color textColor = {255, 255, 255, 255};
                    renderer.drawText("DONE", x + doneWidth / 2, y + 8,
                                      renderer.getFontSmall(), textColor, TextAlign::Center);
                }
                continue;
            }

            renderer.drawRect(x, y, keyWidth, keyHeight, bgColor, true);

            // Draw character or label
            SDL_Color textColor = {255, 255, 255, 255};
            std::string label;
            if (isSpecialKey(row, col)) {
                label = getSpecialKeyLabel(row, col);
            } else {
                label = std::string(1, getCharAt(row, col));
            }
            renderer.drawText(label, x + keyWidth / 2, y + 8,
                             renderer.getFontSmall(), textColor, TextAlign::Center);
        }
    }
}

// MainMenuScreen
MainMenuScreen::MainMenuScreen() {
    selection = 0;
}

void MainMenuScreen::enter() {
    selection = 0;
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
    }
    if (down) {
        selection = (selection + 1) % ITEM_COUNT;
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
    // Nothing to update
}

void MainMenuScreen::render() {
    auto& renderer = Renderer::instance();

    // Title
    SDL_Color titleColor = {255, 200, 50, 255};
    renderer.drawTextShadow("SCRAP HEAP", WINDOW_WIDTH / 2.0f, 100,
                           renderer.getFontLarge(), titleColor, TextAlign::Center);

    // Menu items
    float startY = 280.0f;
    float itemSpacing = 60.0f;

    for (int i = 0; i < ITEM_COUNT; ++i) {
        SDL_Color itemColor = (i == selection) ?
            SDL_Color{255, 255, 100, 255} : SDL_Color{180, 180, 180, 255};

        std::string text = ITEMS[i];
        if (i == selection) {
            text = "> " + text + " <";
        }

        renderer.drawTextShadow(text, WINDOW_WIDTH / 2.0f, startY + i * itemSpacing,
                               renderer.getFontMedium(), itemColor, TextAlign::Center);
    }

    // Controls hint
    SDL_Color hintColor = {120, 120, 130, 255};
    renderer.drawText("UP/DOWN: Navigate   A/ENTER: Select   B/ESC: Quit",
                     WINDOW_WIDTH / 2.0f, WINDOW_HEIGHT - 50,
                     renderer.getFontSmall(), hintColor, TextAlign::Center);
}

// StatsScreen
void StatsScreen::enter() {
    scrollOffset = 0;
    // Data is already loaded at startup, no need to reload
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

    // Title
    SDL_Color titleColor = {255, 200, 50, 255};
    renderer.drawTextShadow("STATS", WINDOW_WIDTH / 2.0f, 60,
                           renderer.getFontLarge(), titleColor, TextAlign::Center);

    // Headers
    float startY = 150.0f;
    float rowHeight = 35.0f;
    SDL_Color headerColor = {200, 200, 200, 255};

    renderer.drawText("PLAYER", 200, startY, renderer.getFontSmall(), headerColor, TextAlign::Left);
    renderer.drawText("WINS", 500, startY, renderer.getFontSmall(), headerColor, TextAlign::Center);
    renderer.drawText("LOSSES", 620, startY, renderer.getFontSmall(), headerColor, TextAlign::Center);
    renderer.drawText("RATIO", 740, startY, renderer.getFontSmall(), headerColor, TextAlign::Center);

    // Line
    renderer.drawRect(150, startY + 30, WINDOW_WIDTH - 300, 2, headerColor, true);

    // Stats
    const auto& stats = DataManager::instance().getStats();

    if (stats.empty()) {
        SDL_Color emptyColor = {150, 150, 150, 255};
        renderer.drawText("No stats recorded yet. Play some matches!",
                         WINDOW_WIDTH / 2.0f, startY + 100,
                         renderer.getFontSmall(), emptyColor, TextAlign::Center);
    } else {
        SDL_Color rowColor = {220, 220, 220, 255};
        int visible = std::min(10, static_cast<int>(stats.size()) - scrollOffset);

        for (int i = 0; i < visible; ++i) {
            const auto& ps = stats[scrollOffset + i];
            float y = startY + 50 + i * rowHeight;

            renderer.drawText(ps.tag, 200, y, renderer.getFontSmall(), rowColor, TextAlign::Left);
            renderer.drawText(std::to_string(ps.wins), 500, y, renderer.getFontSmall(), rowColor, TextAlign::Center);
            renderer.drawText(std::to_string(ps.losses), 620, y, renderer.getFontSmall(), rowColor, TextAlign::Center);

            char ratioStr[16];
            snprintf(ratioStr, sizeof(ratioStr), "%.1f%%", ps.getWinRatio() * 100.0f);
            renderer.drawText(ratioStr, 740, y, renderer.getFontSmall(), rowColor, TextAlign::Center);
        }
    }

    // Controls hint
    SDL_Color hintColor = {120, 120, 130, 255};
    renderer.drawText("B/ESC: Back   UP/DOWN: Scroll",
                     WINDOW_WIDTH / 2.0f, WINDOW_HEIGHT - 50,
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
    // Data is already loaded at startup, no need to reload
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

    // Title
    SDL_Color titleColor = {255, 200, 50, 255};
    renderer.drawTextShadow("TAGS", WINDOW_WIDTH / 2.0f, 60,
                           renderer.getFontLarge(), titleColor, TextAlign::Center);

    if (enteringNewTag) {
        keyboard.render(WINDOW_WIDTH / 2.0f, WINDOW_HEIGHT / 2.0f);

        SDL_Color hintColor = {120, 120, 130, 255};
        renderer.drawText("A/ENTER: Select   B/ESC: Cancel",
                         WINDOW_WIDTH / 2.0f, WINDOW_HEIGHT - 50,
                         renderer.getFontSmall(), hintColor, TextAlign::Center);
        return;
    }

    // Tag list
    const auto& tags = DataManager::instance().getTags();
    float startY = 150.0f;
    float itemSpacing = 40.0f;

    for (int i = 0; i <= static_cast<int>(tags.size()); ++i) {
        bool selected = (i == selection);
        SDL_Color itemColor = selected ?
            SDL_Color{255, 255, 100, 255} : SDL_Color{180, 180, 180, 255};

        std::string text;
        if (i < static_cast<int>(tags.size())) {
            text = tags[i];
        } else {
            text = "+ ADD NEW TAG";
        }

        if (selected) {
            text = "> " + text;
        }

        renderer.drawText(text, 200, startY + i * itemSpacing,
                         renderer.getFontSmall(), itemColor, TextAlign::Left);
    }

    // Controls hint
    SDL_Color hintColor = {120, 120, 130, 255};
    renderer.drawText("UP/DOWN: Navigate   A: Select   X: Delete   B: Back",
                     WINDOW_WIDTH / 2.0f, WINDOW_HEIGHT - 50,
                     renderer.getFontSmall(), hintColor, TextAlign::Center);
}

} // namespace ScrapHeap
