#include "game_setup.h"
#include "game_context.h"
#include "renderer.h"
#include "data.h"
#include "components.h"
#include "stage.h"
#include <algorithm>

namespace ScrapHeap {

std::string PlayerSlot::getDisplayName(int slotIndex) const {
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

    // Check for any controller pressing A to join
    for (int c = 0; c < 8; ++c) {
        ControllerState* controller = input.getController(c);
        if (!controller || !controller->connected) continue;
        if (controller->playerSlot >= 0) continue;  // Already assigned

        if (controller->buttonAPressed()) {
            // Find empty slot
            for (int s = 0; s < 4; ++s) {
                if (slots[s].state == PlayerSlotState::Empty) {
                    slots[s].state = PlayerSlotState::Configuring;
                    slots[s].controllerIndex = c;
                    input.assignController(c, s);
                    break;
                }
            }
        }
    }

    // Keyboard player (slot 0)
    bool keyboardUsed = false;
    for (int s = 0; s < 4; ++s) {
        if (slots[s].controllerIndex == -1 && slots[s].state != PlayerSlotState::Empty) {
            keyboardUsed = true;
            break;
        }
    }

    if (!keyboardUsed && keyboard.enterPressed()) {
        for (int s = 0; s < 4; ++s) {
            if (slots[s].state == PlayerSlotState::Empty) {
                slots[s].state = PlayerSlotState::Configuring;
                slots[s].controllerIndex = -1;  // Keyboard
                break;
            }
        }
    }

    // Handle input for each slot
    for (int s = 0; s < 4; ++s) {
        if (slots[s].state == PlayerSlotState::Empty) continue;

        const ControllerState* controller = nullptr;
        const KeyboardState* kb = nullptr;

        if (slots[s].controllerIndex >= 0) {
            controller = input.getController(slots[s].controllerIndex);
        } else {
            kb = &keyboard;
        }

        handleSlotInput(s, controller, kb, ctx);
    }

    // Check for transition to stage select
    int joined = countJoinedPlayers();
    int ready = countReadyPlayers();

    if (joined >= 2 && ready == joined) {
        ctx.stageSelect.enter(firstReadyPlayer);
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

    if (confirm && slot.currentOption == ConfigOption::OK) {
        slot.state = PlayerSlotState::Ready;
        if (firstReadyPlayer < 0) {
            firstReadyPlayer = slotIndex;
        }
    }
}

void GameSetupScreen::update(float dt) {
    // Nothing to update
}

void GameSetupScreen::render() {
    auto& renderer = Renderer::instance();

    // Title
    SDL_Color titleColor = {255, 200, 50, 255};
    renderer.drawTextShadow("SELECT YOUR BOT", WINDOW_WIDTH / 2.0f, 30,
                           renderer.getFontMedium(), titleColor, TextAlign::Center);

    // Render 4 quadrants
    float quadWidth = WINDOW_WIDTH / 2.0f;
    float quadHeight = (WINDOW_HEIGHT - 80) / 2.0f;

    for (int i = 0; i < 4; ++i) {
        float x = (i % 2) * quadWidth;
        float y = 80 + (i / 2) * quadHeight;
        renderSlot(i, x, y, quadWidth, quadHeight);
    }

    // Controls hint
    SDL_Color hintColor = {120, 120, 130, 255};
    renderer.drawText("A: Join/Select   B: Back/Leave   D-PAD: Navigate",
                     WINDOW_WIDTH / 2.0f, WINDOW_HEIGHT - 20,
                     renderer.getFontSmall(), hintColor, TextAlign::Center);
}

void GameSetupScreen::renderSlot(int slotIndex, float x, float y, float width, float height) {
    auto& renderer = Renderer::instance();
    const PlayerSlot& slot = slots[slotIndex];

    // Border
    SDL_Color borderColor = {80, 80, 90, 255};
    renderer.drawRectOutline(x + 5, y + 5, width - 10, height - 10, borderColor, 2);

    float centerX = x + width / 2.0f;
    float contentY = y + 30;

    if (slot.state == PlayerSlotState::Empty) {
        SDL_Color textColor = {150, 150, 160, 255};
        renderer.drawText("Press A to join", centerX, y + height / 2.0f - 15,
                         renderer.getFontMedium(), textColor, TextAlign::Center);
        return;
    }

    SDL_Color playerColor = Renderer::getPlayerColor(slot.colorIndex);

    if (slot.state == PlayerSlotState::Ready) {
        renderer.drawTextShadow("READY", centerX, y + height / 2.0f - 30,
                               renderer.getFontLarge(), playerColor, TextAlign::Center);
        renderer.drawText(slot.getDisplayName(slotIndex), centerX, y + height / 2.0f + 30,
                         renderer.getFontSmall(), playerColor, TextAlign::Center);
        return;
    }

    // Configuring state - show all options
    auto& registry = ComponentRegistry::instance();
    const char* optionLabels[] = {"TAG", "ENGINE", "FRAME", "WEAPON", "SPECIAL", "COLOR", "OK"};

    float lineHeight = 28.0f;
    float labelX = x + 20;
    float valueX = x + width - 20;

    for (int opt = 0; opt < static_cast<int>(ConfigOption::COUNT); ++opt) {
        bool selected = (slot.currentOption == static_cast<ConfigOption>(opt));
        SDL_Color labelColor = selected ? SDL_Color{255, 255, 100, 255} : SDL_Color{180, 180, 180, 255};
        SDL_Color valueColor = selected ? SDL_Color{255, 255, 255, 255} : SDL_Color{150, 150, 160, 255};

        float lineY = contentY + opt * lineHeight;

        std::string label = optionLabels[opt];
        if (selected) label = "> " + label;

        renderer.drawText(label, labelX, lineY, renderer.getFontSmall(), labelColor, TextAlign::Left);

        std::string value;
        switch (static_cast<ConfigOption>(opt)) {
            case ConfigOption::Tag:
                value = "< " + slot.getDisplayName(slotIndex) + " >";
                break;
            case ConfigOption::Engine:
                value = "< " + registry.getEngine(slot.engineIndex).name + " >";
                break;
            case ConfigOption::Frame:
                value = "< " + registry.getFrame(slot.frameIndex).name + " >";
                break;
            case ConfigOption::Weapon:
                value = "< " + registry.getWeapon(slot.weaponIndex).name + " >";
                break;
            case ConfigOption::Special:
                value = "< " + registry.getSpecial(slot.specialIndex).name + " >";
                break;
            case ConfigOption::Color:
                // Draw color swatch instead
                renderer.drawRect(valueX - 60, lineY, 50, 20, playerColor, true);
                value = "";
                break;
            case ConfigOption::OK:
                value = "[READY]";
                break;
            default:
                break;
        }

        if (!value.empty()) {
            renderer.drawText(value, valueX, lineY, renderer.getFontSmall(), valueColor, TextAlign::Right);
        }
    }
}

// StageSelectScreen
void StageSelectScreen::enter(int player) {
    selectingPlayer = player;
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

    // Title
    SDL_Color titleColor = {255, 200, 50, 255};
    renderer.drawTextShadow("SELECT STAGE", WINDOW_WIDTH / 2.0f, 40,
                           renderer.getFontMedium(), titleColor, TextAlign::Center);

    // Who's selecting
    SDL_Color selectingColor = Renderer::getPlayerColor(selectingPlayer);
    renderer.drawText("Player " + std::to_string(selectingPlayer + 1) + " is selecting",
                     WINDOW_WIDTH / 2.0f, 80,
                     renderer.getFontSmall(), selectingColor, TextAlign::Center);

    // Stage grid
    auto& stageReg = StageRegistry::instance();
    int stageCount = stageReg.getStageCount();
    int cols = 2;
    int rows = (stageCount + cols - 1) / cols;

    float gridWidth = 400.0f;
    float gridHeight = 300.0f;
    float cellWidth = gridWidth / cols;
    float cellHeight = gridHeight / rows;
    float gridX = (WINDOW_WIDTH - gridWidth) / 2.0f;
    float gridY = 150.0f;

    for (int i = 0; i < stageCount; ++i) {
        const auto& stage = stageReg.getStage(i);
        int col = i % cols;
        int row = i / cols;

        float x = gridX + col * cellWidth;
        float y = gridY + row * cellHeight;

        bool selected = (i == selection);
        SDL_Color bgColor = selected ?
            SDL_Color{80, 80, 120, 255} : SDL_Color{50, 50, 60, 255};

        renderer.drawRect(x + 5, y + 5, cellWidth - 10, cellHeight - 10, bgColor, true);

        if (selected) {
            SDL_Color borderColor = {255, 200, 50, 255};
            renderer.drawRectOutline(x + 5, y + 5, cellWidth - 10, cellHeight - 10, borderColor, 3);
        }

        // Stage name
        SDL_Color textColor = selected ?
            SDL_Color{255, 255, 255, 255} : SDL_Color{180, 180, 180, 255};
        renderer.drawText(stage.name, x + cellWidth / 2, y + cellHeight / 2 - 10,
                         renderer.getFontSmall(), textColor, TextAlign::Center);
    }

    // Stage preview
    const auto& selectedStage = stageReg.getStage(selection);
    float previewX = (WINDOW_WIDTH - 200) / 2.0f;
    float previewY = 480.0f;
    float scale = 200.0f / std::max(selectedStage.width, selectedStage.height);

    renderer.drawRect(previewX, previewY, selectedStage.width * scale,
                     selectedStage.height * scale, selectedStage.backgroundColor, true);
    renderer.drawRectOutline(previewX, previewY, selectedStage.width * scale,
                            selectedStage.height * scale, selectedStage.wallColor, 4);

    // Description
    SDL_Color descColor = {150, 150, 160, 255};
    renderer.drawText(selectedStage.description, WINDOW_WIDTH / 2.0f, WINDOW_HEIGHT - 60,
                     renderer.getFontSmall(), descColor, TextAlign::Center);

    // Controls
    SDL_Color hintColor = {120, 120, 130, 255};
    renderer.drawText("A: Confirm   B: Back",
                     WINDOW_WIDTH / 2.0f, WINDOW_HEIGHT - 30,
                     renderer.getFontSmall(), hintColor, TextAlign::Center);
}

} // namespace ScrapHeap
