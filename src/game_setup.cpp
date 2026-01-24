#include "game_setup.h"
#include "game_context.h"
#include "renderer.h"
#include "data.h"
#include "components.h"
#include "stage.h"
#include "bot.h"
#include <algorithm>
#include <random>

namespace ScrapHeap {

std::string PlayerSlot::getDisplayName(int slotIndex) const {
    if (isAI) {
        return "CPU " + std::to_string(slotIndex + 1);
    }
    if (tagIndex < 0) {
        return "PLAYER " + std::to_string(slotIndex + 1);
    }
    const auto& tags = DataManager::instance().getTags();
    if (tagIndex < static_cast<int>(tags.size())) {
        return tags[tagIndex];
    }
    return "PLAYER " + std::to_string(slotIndex + 1);
}

// GameSetupScreen
void GameSetupScreen::enter() {
    reset();
}

void GameSetupScreen::reset() {
    for (int i = 0; i < 4; ++i) {
        slots[i] = PlayerSlot();
        slots[i].colorIndex = i;  // Default to different colors
    }
    firstReadyPlayer = -1;

    // Unassign all controllers
    auto& input = InputManager::instance();
    for (int i = 0; i < 8; ++i) {
        input.unassignController(i);
    }
}

int GameSetupScreen::countJoinedPlayers() const {
    int count = 0;
    for (int i = 0; i < 4; ++i) {
        if (slots[i].state != PlayerSlotState::Empty) count++;
    }
    return count;
}

int GameSetupScreen::countReadyPlayers() const {
    int count = 0;
    for (int i = 0; i < 4; ++i) {
        if (slots[i].state == PlayerSlotState::Ready) count++;
    }
    return count;
}

std::vector<int> GameSetupScreen::getAvailableTagIndices(int currentSlot) const {
    const auto& tags = DataManager::instance().getTags();
    std::vector<int> available;

    for (int i = 0; i < static_cast<int>(tags.size()); ++i) {
        bool taken = false;
        for (int j = 0; j < 4; ++j) {
            if (j != currentSlot && slots[j].state != PlayerSlotState::Empty &&
                slots[j].tagIndex == i) {
                taken = true;
                break;
            }
        }
        if (!taken) available.push_back(i);
    }

    return available;
}

std::vector<int> GameSetupScreen::getAvailableColorIndices(int currentSlot) const {
    std::vector<int> available;
    const auto& colors = Renderer::getPlayerColors();

    for (int i = 0; i < static_cast<int>(colors.size()); ++i) {
        bool taken = false;
        for (int j = 0; j < 4; ++j) {
            if (j != currentSlot && slots[j].state != PlayerSlotState::Empty &&
                slots[j].colorIndex == i) {
                taken = true;
                break;
            }
        }
        if (!taken) available.push_back(i);
    }

    return available;
}

void GameSetupScreen::handleInput(GameContext& ctx) {
    auto& input = InputManager::instance();
    const auto& keyboard = input.getKeyboard();

    // Track which controllers already triggered a join this frame to prevent doubles
    bool controllerJoinedThisFrame[8] = {false};

    // Check for any controller pressing A to join
    for (int c = 0; c < 8; ++c) {
        ControllerState* controller = input.getController(c);
        if (!controller || !controller->connected) continue;
        if (controller->playerSlot >= 0) continue;  // Already assigned to a slot

        if (controller->buttonAPressed()) {
            // Find empty slot
            for (int s = 0; s < 4; ++s) {
                if (slots[s].state == PlayerSlotState::Empty) {
                    slots[s].state = PlayerSlotState::Configuring;
                    slots[s].controllerIndex = c;
                    input.assignController(c, s);
                    controllerJoinedThisFrame[c] = true;
                    break;
                }
            }
        }
    }

    // Keyboard player - can only join if no keyboard player exists yet
    bool keyboardUsed = false;
    for (int s = 0; s < 4; ++s) {
        if (slots[s].controllerIndex == -1 && slots[s].state != PlayerSlotState::Empty) {
            keyboardUsed = true;
            break;
        }
    }

    // Only allow keyboard join with Enter key (not Space, to avoid conflicts)
    if (!keyboardUsed && keyboard.enterPressed()) {
        for (int s = 0; s < 4; ++s) {
            if (slots[s].state == PlayerSlotState::Empty) {
                slots[s].state = PlayerSlotState::Configuring;
                slots[s].controllerIndex = -1;  // Keyboard
                break;
            }
        }
    }

    // X key adds AI player with random build
    if (keyboard.xPressed()) {
        for (int s = 0; s < 4; ++s) {
            if (slots[s].state == PlayerSlotState::Empty) {
                auto& registry = ComponentRegistry::instance();
                static std::random_device rd;
                static std::mt19937 gen(rd());

                // Random build
                std::uniform_int_distribution<> frameDist(0, registry.getFrameCount() - 1);
                std::uniform_int_distribution<> engineDist(0, registry.getEngineCount() - 1);
                std::uniform_int_distribution<> weaponDist(0, registry.getWeaponCount() - 1);
                std::uniform_int_distribution<> specialDist(0, registry.getSpecialCount() - 1);

                slots[s].frameIndex = frameDist(gen);
                slots[s].engineIndex = engineDist(gen);
                slots[s].weaponIndex = weaponDist(gen);
                slots[s].specialIndex = specialDist(gen);

                // Find available color
                auto availableColors = getAvailableColorIndices(s);
                if (!availableColors.empty()) {
                    std::uniform_int_distribution<> colorDist(0, static_cast<int>(availableColors.size()) - 1);
                    slots[s].colorIndex = availableColors[colorDist(gen)];
                }

                // Mark as AI and auto-ready
                slots[s].controllerIndex = -2;  // AI indicator
                slots[s].isAI = true;
                slots[s].tagIndex = -1;  // Will show as "CPU N"
                slots[s].state = PlayerSlotState::Ready;

                // Track first ready player
                if (firstReadyPlayer < 0) {
                    firstReadyPlayer = s;
                }
                break;
            }
        }
    }

    // Handle input for each slot
    for (int s = 0; s < 4; ++s) {
        if (slots[s].state == PlayerSlotState::Empty) continue;
        if (slots[s].isAI) continue;  // AI slots don't receive input

        const ControllerState* controller = nullptr;
        const KeyboardState* kb = nullptr;

        if (slots[s].controllerIndex >= 0) {
            controller = input.getController(slots[s].controllerIndex);
        } else if (slots[s].controllerIndex == -1) {
            kb = &keyboard;
        }

        handleSlotInput(s, controller, kb, ctx);
    }

    // Check for transition to stage select
    int joined = countJoinedPlayers();
    int ready = countReadyPlayers();

    if (joined >= 2 && ready == joined) {
        ctx.stageSelect.enter(firstReadyPlayer, slots[firstReadyPlayer].colorIndex);
        ctx.changeState(GameState::StageSelect);
    }
}

void GameSetupScreen::handleSlotInput(int slotIndex, const ControllerState* controller,
                                      const KeyboardState* keyboard, GameContext& ctx) {
    PlayerSlot& slot = slots[slotIndex];

    bool up = (controller && controller->dpadUpPressed()) ||
              (keyboard && keyboard->upPressed());
    bool down = (controller && controller->dpadDownPressed()) ||
                (keyboard && keyboard->downPressed());
    bool left = (controller && controller->dpadLeftPressed()) ||
                (keyboard && keyboard->leftPressed());
    bool right = (controller && controller->dpadRightPressed()) ||
                 (keyboard && keyboard->rightPressed());
    bool confirm = (controller && controller->buttonAPressed()) ||
                   (keyboard && (keyboard->enterPressed() || keyboard->spacePressed()));
    bool cancel = (controller && controller->buttonBPressed()) ||
                  (keyboard && keyboard->escapePressed());

    if (slot.state == PlayerSlotState::Ready) {
        if (cancel) {
            slot.state = PlayerSlotState::Configuring;
            if (firstReadyPlayer == slotIndex) {
                // Find new first ready player
                firstReadyPlayer = -1;
                for (int i = 0; i < 4; ++i) {
                    if (slots[i].state == PlayerSlotState::Ready) {
                        firstReadyPlayer = i;
                        break;
                    }
                }
            }
        }
        return;
    }

    // Configuring state
    if (cancel) {
        slot.state = PlayerSlotState::Empty;
        if (slot.controllerIndex >= 0) {
            InputManager::instance().unassignController(slot.controllerIndex);
        }
        slot.controllerIndex = -1;
        return;
    }

    if (up) {
        int opt = static_cast<int>(slot.currentOption);
        opt = (opt - 1 + static_cast<int>(ConfigOption::COUNT)) % static_cast<int>(ConfigOption::COUNT);
        slot.currentOption = static_cast<ConfigOption>(opt);
    }
    if (down) {
        int opt = static_cast<int>(slot.currentOption);
        opt = (opt + 1) % static_cast<int>(ConfigOption::COUNT);
        slot.currentOption = static_cast<ConfigOption>(opt);
    }

    auto& registry = ComponentRegistry::instance();
    auto availableTags = getAvailableTagIndices(slotIndex);
    auto availableColors = getAvailableColorIndices(slotIndex);

    if (left || right) {
        int dir = right ? 1 : -1;

        switch (slot.currentOption) {
            case ConfigOption::Tag: {
                if (availableTags.empty()) break;
                // Find current position in available tags
                int pos = -1;
                for (int i = 0; i < static_cast<int>(availableTags.size()); ++i) {
                    if (availableTags[i] == slot.tagIndex) {
                        pos = i;
                        break;
                    }
                }
                if (pos < 0) {
                    slot.tagIndex = availableTags[0];
                } else {
                    pos = (pos + dir + static_cast<int>(availableTags.size()) + 1) %
                          (static_cast<int>(availableTags.size()) + 1);
                    slot.tagIndex = (pos == static_cast<int>(availableTags.size())) ?
                                    -1 : availableTags[pos];
                }
                break;
            }
            case ConfigOption::Engine:
                slot.engineIndex = (slot.engineIndex + dir + registry.getEngineCount()) %
                                   registry.getEngineCount();
                break;
            case ConfigOption::Frame:
                slot.frameIndex = (slot.frameIndex + dir + registry.getFrameCount()) %
                                  registry.getFrameCount();
                break;
            case ConfigOption::Weapon:
                slot.weaponIndex = (slot.weaponIndex + dir + registry.getWeaponCount()) %
                                   registry.getWeaponCount();
                break;
            case ConfigOption::Special:
                slot.specialIndex = (slot.specialIndex + dir + registry.getSpecialCount()) %
                                    registry.getSpecialCount();
                break;
            case ConfigOption::Color: {
                if (availableColors.empty()) break;
                int pos = -1;
                for (int i = 0; i < static_cast<int>(availableColors.size()); ++i) {
                    if (availableColors[i] == slot.colorIndex) {
                        pos = i;
                        break;
                    }
                }
                if (pos < 0) pos = 0;
                pos = (pos + dir + static_cast<int>(availableColors.size())) %
                      static_cast<int>(availableColors.size());
                slot.colorIndex = availableColors[pos];
                break;
            }
            case ConfigOption::OK:
                break;
            default:
                break;
        }
    }

    if (confirm) {
        // A button advances to next option (like pressing down), or confirms for OK
        if (slot.currentOption == ConfigOption::OK) {
            slot.state = PlayerSlotState::Ready;
            if (firstReadyPlayer < 0) {
                firstReadyPlayer = slotIndex;
            }
        } else {
            // Move to next option
            int opt = static_cast<int>(slot.currentOption);
            opt = (opt + 1) % static_cast<int>(ConfigOption::COUNT);
            slot.currentOption = static_cast<ConfigOption>(opt);
        }
    }
}

void GameSetupScreen::update(float dt) {
    // Nothing to update
}

void GameSetupScreen::render() {
    auto& renderer = Renderer::instance();

    // === RETRO GARAGE STYLE ===

    // Dark concrete floor background
    SDL_Color floorColor = {30, 28, 32, 255};
    renderer.drawRect(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, floorColor, true);

    // Floor grid pattern (like garage tiles)
    SDL_Color gridColor = {40, 38, 44, 255};
    float tileSize = 32.0f;
    for (float x = 0; x < WINDOW_WIDTH; x += tileSize) {
        renderer.drawLine(x, 0, x, WINDOW_HEIGHT, gridColor, 1.0f);
    }
    for (float y = 0; y < WINDOW_HEIGHT; y += tileSize) {
        renderer.drawLine(0, y, WINDOW_WIDTH, y, gridColor, 1.0f);
    }

    // Title banner - industrial style
    SDL_Color bannerBg = {20, 18, 24, 255};
    SDL_Color bannerBorder = {255, 200, 50, 255};
    renderer.drawRect(0, 0, WINDOW_WIDTH, 50, bannerBg, true);
    renderer.drawRect(0, 48, WINDOW_WIDTH, 4, bannerBorder, true);

    // Hazard stripes on banner
    SDL_Color hazardYellow = {255, 200, 50, 255};
    SDL_Color hazardBlack = {20, 18, 24, 255};
    for (float x = 0; x < WINDOW_WIDTH; x += 40) {
        renderer.drawRect(x, 0, 20, 6, hazardYellow, true);
    }

    // Title
    SDL_Color titleColor = {255, 220, 80, 255};
    renderer.drawText("BUILD YOUR BOT", WINDOW_WIDTH / 2.0f, 18,
                     renderer.getFontMedium(), titleColor, TextAlign::Center);

    // Render 4 garage bays (quadrants)
    float quadWidth = WINDOW_WIDTH / 2.0f;
    float quadHeight = (WINDOW_HEIGHT - 85) / 2.0f;

    for (int i = 0; i < 4; ++i) {
        float x = (i % 2) * quadWidth;
        float y = 55 + (i / 2) * quadHeight;
        renderSlot(i, x, y, quadWidth, quadHeight);
    }

    // Controls hint bar - industrial
    SDL_Color hintBg = {15, 13, 18, 240};
    renderer.drawRect(0, WINDOW_HEIGHT - 28, WINDOW_WIDTH, 28, hintBg, true);
    renderer.drawRect(0, WINDOW_HEIGHT - 28, WINDOW_WIDTH, 2, {60, 60, 70, 255}, true);

    SDL_Color hintColor = {140, 140, 150, 255};
    renderer.drawText("A:JOIN  B:LEAVE  DPAD:SELECT",
                     WINDOW_WIDTH / 2.0f, WINDOW_HEIGHT - 18,
                     renderer.getFontSmall(), hintColor, TextAlign::Center);
}

void GameSetupScreen::renderSlot(int slotIndex, float x, float y, float width, float height) {
    auto& renderer = Renderer::instance();
    const PlayerSlot& slot = slots[slotIndex];

    // Get player color (used for garage bay tint)
    SDL_Color playerColor = Renderer::getPlayerColor(slot.colorIndex);
    SDL_Color darkPlayerColor = {
        static_cast<Uint8>(playerColor.r * 0.15f),
        static_cast<Uint8>(playerColor.g * 0.15f),
        static_cast<Uint8>(playerColor.b * 0.15f),
        255
    };

    // === GARAGE BAY BACKGROUND ===
    float padding = 4.0f;
    float bayX = x + padding;
    float bayY = y + padding;
    float bayW = width - padding * 2;
    float bayH = height - padding * 2;

    // Garage bay floor (darker)
    SDL_Color bayFloor = (slot.state == PlayerSlotState::Empty) ?
        SDL_Color{22, 20, 26, 255} : darkPlayerColor;
    renderer.drawRect(bayX, bayY, bayW, bayH, bayFloor, true);

    // Garage door frame (top bar with color)
    SDL_Color frameColor = (slot.state == PlayerSlotState::Empty) ?
        SDL_Color{50, 48, 55, 255} : playerColor;
    renderer.drawRect(bayX, bayY, bayW, 6, frameColor, true);

    // Side stripes (industrial look)
    SDL_Color stripeColor = (slot.state == PlayerSlotState::Empty) ?
        SDL_Color{40, 38, 44, 255} :
        SDL_Color{
            static_cast<Uint8>(playerColor.r * 0.4f),
            static_cast<Uint8>(playerColor.g * 0.4f),
            static_cast<Uint8>(playerColor.b * 0.4f),
            255
        };
    renderer.drawRect(bayX, bayY + 6, 4, bayH - 6, stripeColor, true);
    renderer.drawRect(bayX + bayW - 4, bayY + 6, 4, bayH - 6, stripeColor, true);

    // Player number badge
    char pNum[8];
    snprintf(pNum, sizeof(pNum), "P%d", slotIndex + 1);
    SDL_Color badgeColor = (slot.state == PlayerSlotState::Empty) ?
        SDL_Color{60, 58, 65, 255} : playerColor;
    renderer.drawRect(bayX + 8, bayY + 12, 28, 18, badgeColor, true);
    renderer.drawText(pNum, bayX + 22, bayY + 14,
                     renderer.getFontSmall(), {0, 0, 0, 255}, TextAlign::Center);

    float centerX = x + width / 2.0f;

    if (slot.state == PlayerSlotState::Empty) {
        // Empty bay - waiting for player
        SDL_Color textColor = {100, 100, 110, 255};
        renderer.drawText("PRESS A", centerX, y + height / 2.0f - 8,
                         renderer.getFontSmall(), textColor, TextAlign::Center);
        renderer.drawText("TO JOIN", centerX, y + height / 2.0f + 8,
                         renderer.getFontSmall(), textColor, TextAlign::Center);
        return;
    }

    if (slot.state == PlayerSlotState::Ready) {
        // Ready state - show bot in garage with READY banner
        renderBotPreview(slot, centerX, y + height * 0.32f, 45.0f);

        // Ready banner
        SDL_Color readyBg = {20, 60, 20, 255};
        SDL_Color readyText = {100, 255, 100, 255};
        renderer.drawRect(bayX + 10, y + height * 0.50f, bayW - 20, 20, readyBg, true);
        renderer.drawText("READY!", centerX, y + height * 0.50f + 3,
                         renderer.getFontSmall(), readyText, TextAlign::Center);

        // Player name
        renderer.drawText(slot.getDisplayName(slotIndex), centerX, y + height * 0.60f,
                         renderer.getFontSmall(), playerColor, TextAlign::Center);

        // Compact stats - positioned to fit within bay (panel height ~98px)
        BotStats stats = calculateBotStats(slot);
        renderStatsDisplay(stats, x + 12, y + height * 0.67f, width - 24, playerColor);
        return;
    }

    // === CONFIGURING STATE - NEW LAYOUT ===
    // Parts on left, Bot preview on right (bigger), Stats at bottom
    auto& registry = ComponentRegistry::instance();
    const char* optionLabels[] = {"TAG", "ENGINE", "FRAME", "WEAPON", "SPECIAL", "COLOR", "OK"};

    // Layout dimensions
    float statsHeight = 50.0f;  // Height for stats bar at bottom
    float contentHeight = bayH - 38 - statsHeight;  // Above stats, below header
    float optionsWidth = width * 0.55f;  // Left panel wider to fit names
    float previewWidth = bayW - optionsWidth - 8;  // Right panel for bot preview

    // === LEFT SIDE: Parts list ===
    float lineHeight = 20.0f;
    float labelX = bayX + 10;
    float valueX = bayX + optionsWidth - 8;
    float contentY = bayY + 32;

    for (int opt = 0; opt < static_cast<int>(ConfigOption::COUNT); ++opt) {
        bool selected = (slot.currentOption == static_cast<ConfigOption>(opt));
        float lineY = contentY + opt * lineHeight;

        // Selection highlight bar
        if (selected) {
            SDL_Color highlightBg = {
                static_cast<Uint8>(playerColor.r * 0.3f),
                static_cast<Uint8>(playerColor.g * 0.3f),
                static_cast<Uint8>(playerColor.b * 0.3f),
                255
            };
            renderer.drawRect(labelX - 2, lineY - 1, optionsWidth - 12, lineHeight - 2, highlightBg, true);
        }

        SDL_Color labelColor = selected ? playerColor : SDL_Color{130, 130, 140, 255};
        SDL_Color valueColor = selected ? SDL_Color{255, 255, 255, 255} : SDL_Color{100, 100, 110, 255};

        std::string label = optionLabels[opt];
        if (selected) label = ">" + label;

        renderer.drawText(label, labelX, lineY, renderer.getFontSmall(), labelColor, TextAlign::Left);

        std::string value;
        switch (static_cast<ConfigOption>(opt)) {
            case ConfigOption::Tag: {
                std::string tagName = slot.getDisplayName(slotIndex);
                if (tagName.length() > 12) tagName = tagName.substr(0, 11) + "..";
                value = "<" + tagName + ">";
                break;
            }
            case ConfigOption::Engine: {
                std::string name = registry.getEngine(slot.engineIndex).name;
                if (name.length() > 12) name = name.substr(0, 11) + "..";
                value = "<" + name + ">";
                break;
            }
            case ConfigOption::Frame: {
                std::string name = registry.getFrame(slot.frameIndex).name;
                if (name.length() > 12) name = name.substr(0, 11) + "..";
                value = "<" + name + ">";
                break;
            }
            case ConfigOption::Weapon: {
                std::string name = registry.getWeapon(slot.weaponIndex).name;
                if (name.length() > 14) name = name.substr(0, 13) + "..";
                value = "<" + name + ">";
                break;
            }
            case ConfigOption::Special: {
                std::string name = registry.getSpecial(slot.specialIndex).name;
                if (name.length() > 14) name = name.substr(0, 13) + "..";
                value = "<" + name + ">";
                break;
            }
            case ConfigOption::Color:
                // Draw color swatch
                renderer.drawRect(valueX - 30, lineY + 2, 24, 12, playerColor, true);
                renderer.drawRectOutline(valueX - 30, lineY + 2, 24, 12, {80, 80, 90, 255}, 1.0f);
                value = "";
                break;
            case ConfigOption::OK:
                value = "[GO!]";
                valueColor = selected ? SDL_Color{100, 255, 100, 255} : SDL_Color{80, 150, 80, 255};
                break;
            default:
                break;
        }

        if (!value.empty()) {
            renderer.drawText(value, valueX, lineY, renderer.getFontSmall(), valueColor, TextAlign::Right);
        }
    }

    // === RIGHT SIDE: Bot preview (BIGGER) ===
    float previewX = bayX + optionsWidth + previewWidth / 2.0f;
    float previewY = bayY + 32 + contentHeight / 2.0f - 10;

    // Preview area background
    SDL_Color previewBg = {
        static_cast<Uint8>(playerColor.r * 0.1f),
        static_cast<Uint8>(playerColor.g * 0.1f),
        static_cast<Uint8>(playerColor.b * 0.1f),
        255
    };
    renderer.drawRect(bayX + optionsWidth + 4, bayY + 32, previewWidth - 8, contentHeight - 4, previewBg, true);

    // Preview platform
    SDL_Color platformColor = {
        static_cast<Uint8>(playerColor.r * 0.25f),
        static_cast<Uint8>(playerColor.g * 0.25f),
        static_cast<Uint8>(playerColor.b * 0.25f),
        255
    };
    float platW = previewWidth - 24;
    renderer.drawRect(previewX - platW / 2, previewY + 45, platW, 8, platformColor, true);

    // Bot preview - MUCH BIGGER (65 instead of 45)
    renderBotPreview(slot, previewX, previewY, 65.0f);

    // === BOTTOM: Stats bar spanning full width ===
    float statsY = bayY + bayH - statsHeight - 4;
    BotStats stats = calculateBotStats(slot);
    renderStatsCompact(stats, bayX + 8, statsY, bayW - 16, playerColor);
}

// StageSelectScreen
void StageSelectScreen::enter(int player, int playerColorIndex) {
    selectingPlayer = player;
    selectingPlayerColor = playerColorIndex;
    selection = 0;
    previewTimer = 0.0f;
}

void StageSelectScreen::handleInput(GameContext& ctx) {
    auto& input = InputManager::instance();

    // Only the selecting player can control
    const ControllerState* controller = input.getControllerForPlayer(selectingPlayer);
    const KeyboardState* keyboard = (selectingPlayer == 0 ||
                                     ctx.gameSetup.getSlots()[selectingPlayer].controllerIndex == -1) ?
                                    &input.getKeyboard() : nullptr;

    bool left = (controller && controller->dpadLeftPressed()) ||
                (keyboard && keyboard->leftPressed());
    bool right = (controller && controller->dpadRightPressed()) ||
                 (keyboard && keyboard->rightPressed());
    bool up = (controller && controller->dpadUpPressed()) ||
              (keyboard && keyboard->upPressed());
    bool down = (controller && controller->dpadDownPressed()) ||
                (keyboard && keyboard->downPressed());
    bool confirm = (controller && controller->buttonAPressed()) ||
                   (keyboard && (keyboard->enterPressed() || keyboard->spacePressed()));
    bool cancel = (controller && controller->buttonBPressed()) ||
                  (keyboard && keyboard->escapePressed());

    int stageCount = StageRegistry::instance().getStageCount();
    int cols = 2;

    if (left) selection = (selection - 1 + stageCount) % stageCount;
    if (right) selection = (selection + 1) % stageCount;
    if (up) selection = (selection - cols + stageCount) % stageCount;
    if (down) selection = (selection + cols) % stageCount;

    if (confirm) {
        // Start battle!
        ctx.battle = std::make_unique<BattleState>(
            BattleManager::createBattle(ctx.gameSetup.getSlots(), selection));
        ctx.changeState(GameState::Battle);
    }

    if (cancel) {
        // Go back to game setup, unready all players
        ctx.gameSetup.reset();
        ctx.changeState(GameState::GameSetup);
    }
}

void StageSelectScreen::update(float dt) {
    previewTimer += dt;
}

void StageSelectScreen::render() {
    auto& renderer = Renderer::instance();

    // Gradient background
    SDL_Color topColor = {18, 22, 38, 255};
    SDL_Color bottomColor = {32, 28, 48, 255};
    renderer.drawGradientRect(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, topColor, bottomColor);

    // Title bar with gradient
    SDL_Color titleBgTop = {45, 50, 75, 255};
    SDL_Color titleBgBot = {35, 40, 60, 255};
    renderer.drawGradientRect(0, 0, WINDOW_WIDTH, 100, titleBgTop, titleBgBot);

    // Title bar accent line
    SDL_Color accentLine = {255, 180, 80, 180};
    renderer.drawRect(0, 98, WINDOW_WIDTH, 2, accentLine, true);

    // Title
    SDL_Color titleColor = {255, 200, 50, 255};
    renderer.drawTextShadow("SELECT STAGE", WINDOW_WIDTH / 2.0f, 25,
                           renderer.getFontMedium(), titleColor, TextAlign::Center);

    // Who's selecting - with glowing player color indicator
    SDL_Color selectingColor = Renderer::getPlayerColor(selectingPlayerColor);
    SDL_Color selectingGlow = {
        static_cast<Uint8>(selectingColor.r / 3),
        static_cast<Uint8>(selectingColor.g / 3),
        static_cast<Uint8>(selectingColor.b / 3), 100};
    renderer.drawRect(WINDOW_WIDTH / 2.0f - 120, 60, 240, 30, selectingGlow, true);
    renderer.drawText("Player " + std::to_string(selectingPlayer + 1) + " is selecting",
                     WINDOW_WIDTH / 2.0f, 66,
                     renderer.getFontSmall(), selectingColor, TextAlign::Center);

    // Stage grid
    auto& stageReg = StageRegistry::instance();
    int stageCount = stageReg.getStageCount();
    int cols = 2;
    int rows = (stageCount + cols - 1) / cols;

    float gridWidth = 420.0f;
    float gridHeight = 280.0f;
    float cellWidth = gridWidth / cols;
    float cellHeight = gridHeight / rows;
    float gridX = (WINDOW_WIDTH - gridWidth) / 2.0f;
    float gridY = 120.0f;

    for (int i = 0; i < stageCount; ++i) {
        const auto& stage = stageReg.getStage(i);
        int col = i % cols;
        int row = i / cols;

        float x = gridX + col * cellWidth;
        float y = gridY + row * cellHeight;

        bool selected = (i == selection);

        // Card background with gradient
        if (selected) {
            // Glowing selected card
            SDL_Color glowColor = {255, 200, 50, 60};
            renderer.drawRect(x + 2, y + 2, cellWidth - 4, cellHeight - 4, glowColor, true);

            SDL_Color cardTop = {70, 75, 110, 255};
            SDL_Color cardBot = {55, 60, 95, 255};
            renderer.drawGradientRect(x + 8, y + 8, cellWidth - 16, cellHeight - 16, cardTop, cardBot);

            // Animated border
            float pulse = 0.7f + 0.3f * std::sin(previewTimer * 4.0f);
            SDL_Color borderColor = {
                static_cast<Uint8>(255 * pulse),
                static_cast<Uint8>(200 * pulse),
                static_cast<Uint8>(50), 255};
            renderer.drawRectOutline(x + 8, y + 8, cellWidth - 16, cellHeight - 16, borderColor, 3);
        } else {
            // Unselected card
            SDL_Color cardTop = {45, 48, 65, 255};
            SDL_Color cardBot = {35, 38, 55, 255};
            renderer.drawGradientRect(x + 8, y + 8, cellWidth - 16, cellHeight - 16, cardTop, cardBot);

            SDL_Color borderColor = {60, 65, 80, 255};
            renderer.drawRectOutline(x + 8, y + 8, cellWidth - 16, cellHeight - 16, borderColor, 1);
        }

        // Mini stage preview in card
        float miniScale = 0.3f;
        float miniW = stage.width * miniScale;
        float miniH = stage.height * miniScale;
        float miniX = x + (cellWidth - miniW) / 2;
        float miniY = y + 20;
        renderer.drawRect(miniX, miniY, miniW, miniH, stage.backgroundColor, true);
        renderer.drawRectOutline(miniX, miniY, miniW, miniH, stage.wallColor, 2);

        // Stage name
        SDL_Color textColor = selected ?
            SDL_Color{255, 255, 255, 255} : SDL_Color{160, 165, 180, 255};
        renderer.drawText(stage.name, x + cellWidth / 2, y + cellHeight - 30,
                         renderer.getFontSmall(), textColor, TextAlign::Center);
    }

    // Stage preview panel
    const auto& selectedStage = stageReg.getStage(selection);
    float previewPanelX = (WINDOW_WIDTH - 280) / 2.0f;
    float previewPanelY = 420.0f;
    float previewPanelW = 280.0f;
    float previewPanelH = 160.0f;

    // Panel background with glow
    SDL_Color panelGlow = {40, 45, 70, 150};
    renderer.drawRect(previewPanelX - 5, previewPanelY - 5, previewPanelW + 10, previewPanelH + 10, panelGlow, true);

    SDL_Color panelTop = {35, 38, 55, 255};
    SDL_Color panelBot = {28, 30, 45, 255};
    renderer.drawGradientRect(previewPanelX, previewPanelY, previewPanelW, previewPanelH, panelTop, panelBot);

    SDL_Color panelBorder = {70, 75, 100, 255};
    renderer.drawRectOutline(previewPanelX, previewPanelY, previewPanelW, previewPanelH, panelBorder, 2);

    // Stage preview inside panel
    float scale = 180.0f / std::max(selectedStage.width, selectedStage.height);
    float previewW = selectedStage.width * scale;
    float previewH = selectedStage.height * scale;
    float previewX = previewPanelX + (previewPanelW - previewW) / 2;
    float previewY = previewPanelY + 10;

    renderer.drawRect(previewX, previewY, previewW, previewH, selectedStage.backgroundColor, true);
    renderer.drawRectOutline(previewX, previewY, previewW, previewH, selectedStage.wallColor, 3);

    // Description below preview
    SDL_Color descColor = {180, 185, 200, 255};
    renderer.drawText(selectedStage.description, WINDOW_WIDTH / 2.0f, previewPanelY + previewPanelH - 20,
                     renderer.getFontSmall(), descColor, TextAlign::Center);

    // Controls hint bar at bottom
    SDL_Color hintBg = {0, 0, 0, 120};
    renderer.drawRect(0, WINDOW_HEIGHT - 35, WINDOW_WIDTH, 35, hintBg, true);

    SDL_Color hintColor = {150, 150, 160, 255};
    renderer.drawText("A: Confirm   B: Back",
                     WINDOW_WIDTH / 2.0f, WINDOW_HEIGHT - 22,
                     renderer.getFontSmall(), hintColor, TextAlign::Center);
}

BotStats GameSetupScreen::calculateBotStats(const PlayerSlot& slot) const {
    auto& registry = ComponentRegistry::instance();
    const auto& frame = registry.getFrame(slot.frameIndex);
    const auto& engine = registry.getEngine(slot.engineIndex);
    const auto& weapon = registry.getWeapon(slot.weaponIndex);

    BotStats stats;

    // Speed: based on engine power (120-350) and torque (80-250)
    // Higher power = faster, but weight affects it
    float totalWeight = frame.weight + engine.weight + weapon.weight;
    float speedRaw = (engine.power * 0.6f + engine.torque * 0.4f) / totalWeight;
    // speedRaw ranges ~1.5 (slow heavy) to ~8 (fast light)
    stats.speed = std::min(1.0f, std::max(0.0f, (speedRaw - 1.0f) / 7.0f));

    // Damage: based on weapon damage (5-35)
    stats.damage = std::min(1.0f, std::max(0.0f, weapon.damage / 35.0f));

    // Armor: based on frame armor (0.5 to 1.3)
    stats.armor = std::min(1.0f, std::max(0.0f, (frame.armor - 0.4f) / 1.0f));

    // Weight: combined weight normalized (42-130 range)
    // Show as proportion of max possible weight
    float minWeight = 42.0f;  // Roach + Standard Engine + Whip
    float maxWeight = 135.0f; // Brick + Omni-Drive + Battering Ram
    stats.weight = std::min(1.0f, std::max(0.0f, (totalWeight - minWeight) / (maxWeight - minWeight)));

    return stats;
}

void GameSetupScreen::renderStatsDisplay(const BotStats& stats, float x, float y, float width, SDL_Color playerColor) {
    auto& renderer = Renderer::instance();

    // Stat labels and values
    struct StatInfo {
        const char* name;
        const char* icon;  // Simple ASCII icon
        float value;
        SDL_Color barColor;
    };

    StatInfo statInfos[] = {
        {"Speed",  ">>", stats.speed,  {100, 200, 255, 255}},   // Blue
        {"Damage", "**", stats.damage, {255, 100, 100, 255}},   // Red
        {"Armor",  "[]", stats.armor,  {100, 255, 150, 255}},   // Green
        {"Weight", "##", stats.weight, {200, 150, 100, 255}}    // Brown/orange
    };

    float barHeight = 16.0f;
    float barSpacing = 22.0f;
    float iconWidth = 25.0f;
    float labelWidth = 55.0f;
    float barWidth = width - iconWidth - labelWidth - 10.0f;

    // Background panel
    SDL_Color panelBg = {25, 25, 35, 220};
    renderer.drawRect(x - 5, y - 5, width + 10, 4 * barSpacing + 10, panelBg, true);

    // Border accent
    SDL_Color borderColor = {
        static_cast<Uint8>(playerColor.r / 2),
        static_cast<Uint8>(playerColor.g / 2),
        static_cast<Uint8>(playerColor.b / 2), 200};
    renderer.drawRectOutline(x - 5, y - 5, width + 10, 4 * barSpacing + 10, borderColor, 2.0f);

    for (int i = 0; i < 4; ++i) {
        float lineY = y + i * barSpacing;
        const auto& info = statInfos[i];

        // Icon (simple text icon)
        SDL_Color iconColor = info.barColor;
        renderer.drawText(info.icon, x, lineY, renderer.getFontSmall(), iconColor, TextAlign::Left);

        // Label
        SDL_Color labelColor = {200, 200, 200, 255};
        renderer.drawText(info.name, x + iconWidth, lineY, renderer.getFontSmall(), labelColor, TextAlign::Left);

        // Bar background
        float barX = x + iconWidth + labelWidth;
        SDL_Color barBg = {40, 40, 50, 255};
        renderer.drawRect(barX, lineY + 2, barWidth, barHeight, barBg, true);

        // Bar fill
        float fillWidth = barWidth * info.value;
        renderer.drawRect(barX, lineY + 2, fillWidth, barHeight, info.barColor, true);

        // Bar segments (notches for visual style)
        SDL_Color notchColor = {20, 20, 30, 200};
        int numSegments = 5;
        for (int s = 1; s < numSegments; ++s) {
            float notchX = barX + (barWidth * s / numSegments);
            renderer.drawRect(notchX - 1, lineY + 2, 2, barHeight, notchColor, true);
        }

        // Bar border
        SDL_Color barBorder = {80, 80, 100, 255};
        renderer.drawRectOutline(barX, lineY + 2, barWidth, barHeight, barBorder, 1.0f);
    }
}

void GameSetupScreen::renderStatsCompact(const BotStats& stats, float x, float y, float width, SDL_Color playerColor) {
    auto& renderer = Renderer::instance();

    // Horizontal compact layout - 4 stats in a row
    struct StatInfo {
        const char* icon;
        float value;
        SDL_Color color;
    };

    StatInfo statInfos[] = {
        {"SPD", stats.speed,  {100, 200, 255, 255}},   // Blue
        {"DMG", stats.damage, {255, 100, 100, 255}},   // Red
        {"ARM", stats.armor,  {100, 255, 150, 255}},   // Green
        {"WGT", stats.weight, {200, 150, 100, 255}}    // Brown
    };

    // Background
    SDL_Color panelBg = {25, 25, 35, 220};
    renderer.drawRect(x, y, width, 45, panelBg, true);

    // Border accent
    SDL_Color borderColor = {
        static_cast<Uint8>(playerColor.r / 2),
        static_cast<Uint8>(playerColor.g / 2),
        static_cast<Uint8>(playerColor.b / 2), 200};
    renderer.drawRectOutline(x, y, width, 45, borderColor, 1.0f);

    float statWidth = width / 4.0f;
    float barWidth = statWidth - 10;
    float barHeight = 10.0f;

    for (int i = 0; i < 4; ++i) {
        float statX = x + i * statWidth + 5;
        const auto& info = statInfos[i];

        // Label
        renderer.drawText(info.icon, statX + barWidth / 2, y + 4,
                         renderer.getFontSmall(), info.color, TextAlign::Center);

        // Bar background
        SDL_Color barBg = {40, 40, 50, 255};
        renderer.drawRect(statX, y + 20, barWidth, barHeight, barBg, true);

        // Bar fill
        float fillWidth = barWidth * info.value;
        renderer.drawRect(statX, y + 20, fillWidth, barHeight, info.color, true);

        // Bar border
        SDL_Color barBorder = {60, 60, 80, 255};
        renderer.drawRectOutline(statX, y + 20, barWidth, barHeight, barBorder, 1.0f);
    }
}

void GameSetupScreen::renderBotPreview(const PlayerSlot& slot, float centerX, float centerY, float size) {
    auto& renderer = Renderer::instance();
    auto& registry = ComponentRegistry::instance();
    const auto& frame = registry.getFrame(slot.frameIndex);

    Bot previewBot;
    previewBot.x = centerX;
    previewBot.y = centerY;
    previewBot.angle = -PI / 2.0f;
    previewBot.frameIndex = slot.frameIndex;
    previewBot.engineIndex = slot.engineIndex;
    previewBot.weaponIndex = slot.weaponIndex;
    previewBot.specialIndex = slot.specialIndex;
    previewBot.colorIndex = slot.colorIndex;
    previewBot.radius = frame.radius * (size / 50.0f);
    previewBot.isAlive = true;
    previewBot.spinnerSpeed = 0.5f;

    SDL_Color playerColor = Renderer::getPlayerColor(slot.colorIndex);
    renderer.drawBot(previewBot, playerColor, 0, 0);

    SDL_Color labelColor = {200, 200, 200, 255};
    renderer.drawText(frame.name, centerX, centerY + previewBot.radius + 18,
                     renderer.getFontSmall(), labelColor, TextAlign::Center);
}

} // namespace ScrapHeap
