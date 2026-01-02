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
    float contentY = y + 20;

    if (slot.state == PlayerSlotState::Empty) {
        SDL_Color textColor = {150, 150, 160, 255};
        renderer.drawText("Press A to join", centerX, y + height / 2.0f - 15,
                         renderer.getFontMedium(), textColor, TextAlign::Center);
        return;
    }

    SDL_Color playerColor = Renderer::getPlayerColor(slot.colorIndex);

    if (slot.state == PlayerSlotState::Ready) {
        // Show bot preview and stats when ready
        renderBotPreview(slot, centerX, y + height * 0.35f, 60.0f);

        renderer.drawTextShadow("READY", centerX, y + height * 0.65f,
                               renderer.getFontMedium(), playerColor, TextAlign::Center);
        renderer.drawText(slot.getDisplayName(slotIndex), centerX, y + height * 0.75f,
                         renderer.getFontSmall(), playerColor, TextAlign::Center);

        // Show stats below
        BotStats stats = calculateBotStats(slot);
        renderStatsDisplay(stats, x + 15, y + height * 0.8f, width - 30, playerColor);
        return;
    }

    // Configuring state - split into left (options) and right (preview + stats)
    auto& registry = ComponentRegistry::instance();
    const char* optionLabels[] = {"TAG", "ENGINE", "FRAME", "WEAPON", "SPECIAL", "COLOR", "OK"};

    // Left side: options (narrower)
    float optionsWidth = width * 0.55f;
    float lineHeight = 24.0f;
    float labelX = x + 15;
    float valueX = x + optionsWidth - 10;

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
            case ConfigOption::Tag: {
                std::string tagName = slot.getDisplayName(slotIndex);
                if (tagName.length() > 8) tagName = tagName.substr(0, 7) + "..";
                value = "< " + tagName + " >";
                break;
            }
            case ConfigOption::Engine: {
                std::string name = registry.getEngine(slot.engineIndex).name;
                if (name.length() > 8) name = name.substr(0, 7) + "..";
                value = "< " + name + " >";
                break;
            }
            case ConfigOption::Frame: {
                std::string name = registry.getFrame(slot.frameIndex).name;
                if (name.length() > 8) name = name.substr(0, 7) + "..";
                value = "< " + name + " >";
                break;
            }
            case ConfigOption::Weapon: {
                std::string name = registry.getWeapon(slot.weaponIndex).name;
                if (name.length() > 8) name = name.substr(0, 7) + "..";
                value = "< " + name + " >";
                break;
            }
            case ConfigOption::Special: {
                std::string name = registry.getSpecial(slot.specialIndex).name;
                if (name.length() > 8) name = name.substr(0, 7) + "..";
                value = "< " + name + " >";
                break;
            }
            case ConfigOption::Color:
                // Draw color swatch instead
                renderer.drawRect(valueX - 45, lineY + 2, 40, 16, playerColor, true);
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

    // Right side: bot preview and stats
    float previewX = x + optionsWidth + (width - optionsWidth) / 2.0f;
    float previewY = y + 70;

    // Bot preview
    renderBotPreview(slot, previewX, previewY, 50.0f);

    // Stats display below preview
    BotStats stats = calculateBotStats(slot);
    float statsWidth = width - optionsWidth - 20;
    renderStatsDisplay(stats, x + optionsWidth + 5, y + 150, statsWidth, playerColor);
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

    // Title
    SDL_Color titleColor = {255, 200, 50, 255};
    renderer.drawTextShadow("SELECT STAGE", WINDOW_WIDTH / 2.0f, 40,
                           renderer.getFontMedium(), titleColor, TextAlign::Center);

    // Who's selecting
    SDL_Color selectingColor = Renderer::getPlayerColor(selectingPlayerColor);
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

BotStats GameSetupScreen::calculateBotStats(const PlayerSlot& slot) const {
    auto& registry = ComponentRegistry::instance();
    const auto& frame = registry.getFrame(slot.frameIndex);
    const auto& engine = registry.getEngine(slot.engineIndex);
    const auto& weapon = registry.getWeapon(slot.weaponIndex);

    BotStats stats;

    // Speed: based on engine power and torque (0-1 normalized)
    // Max power is ~2.0, max torque is ~2.0
    float speedRaw = (engine.power * 0.6f + engine.torque * 0.4f);
    stats.speed = std::min(1.0f, speedRaw / 2.0f);

    // Damage: based on weapon damage (0-1 normalized)
    // Max damage is ~25
    stats.damage = std::min(1.0f, weapon.damage / 25.0f);

    // Armor: based on frame armor (0-1 normalized)
    // Armor ranges from 0.8 to 2.0
    stats.armor = std::min(1.0f, (frame.armor - 0.5f) / 1.5f);

    // Weight: combined weight (0-1 normalized)
    // Total weight can range from ~3 to ~10
    float totalWeight = frame.weight + engine.weight + weapon.weight;
    stats.weight = std::min(1.0f, (totalWeight - 2.0f) / 8.0f);

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
    SDL_Color borderColor = {playerColor.r / 2, playerColor.g / 2, playerColor.b / 2, 200};
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

void GameSetupScreen::renderBotPreview(const PlayerSlot& slot, float centerX, float centerY, float size) {
    auto& renderer = Renderer::instance();
    auto& registry = ComponentRegistry::instance();

    const auto& frame = registry.getFrame(slot.frameIndex);
    const auto& weapon = registry.getWeapon(slot.weaponIndex);
    SDL_Color playerColor = Renderer::getPlayerColor(slot.colorIndex);

    // Scale based on frame radius
    float scale = size / 50.0f;  // Normalize to 50 pixel base
    float bodyRadius = frame.radius * scale;

    // Draw shadow
    SDL_Color shadowColor = {0, 0, 0, 80};
    renderer.drawRect(centerX - bodyRadius + 3, centerY - bodyRadius + 3,
                     bodyRadius * 2, bodyRadius * 2, shadowColor, true);

    // Main body
    renderer.drawRect(centerX - bodyRadius, centerY - bodyRadius,
                     bodyRadius * 2, bodyRadius * 2, playerColor, true);

    // Body border
    SDL_Color borderColor = {
        static_cast<uint8_t>(playerColor.r * 0.6f),
        static_cast<uint8_t>(playerColor.g * 0.6f),
        static_cast<uint8_t>(playerColor.b * 0.6f),
        255
    };
    renderer.drawRectOutline(centerX - bodyRadius, centerY - bodyRadius,
                            bodyRadius * 2, bodyRadius * 2, borderColor, 2.0f);

    // Weapon indicator (front of bot)
    float weaponLength = 15.0f * scale;
    float weaponWidth = 8.0f * scale;

    // Weapon color based on type
    SDL_Color weaponColor = {180, 180, 200, 255};
    if (weapon.type == WeaponType::Passive) {
        weaponColor = {255, 150, 50, 255};  // Orange for spinners etc
    } else {
        weaponColor = {200, 200, 220, 255};  // Silver for active weapons
    }

    // Draw weapon at top (front)
    renderer.drawRect(centerX - weaponWidth / 2, centerY - bodyRadius - weaponLength,
                     weaponWidth, weaponLength, weaponColor, true);

    // Direction indicator (small triangle/arrow at front)
    SDL_Color arrowColor = {255, 255, 255, 200};
    float arrowSize = 6.0f * scale;
    renderer.drawRect(centerX - arrowSize / 2, centerY - bodyRadius + 5,
                     arrowSize, arrowSize, arrowColor, true);

    // Engine exhaust indicators (back of bot)
    SDL_Color exhaustColor = {100, 100, 120, 200};
    float exhaustWidth = 6.0f * scale;
    float exhaustLength = 8.0f * scale;

    renderer.drawRect(centerX - bodyRadius / 2 - exhaustWidth / 2, centerY + bodyRadius,
                     exhaustWidth, exhaustLength, exhaustColor, true);
    renderer.drawRect(centerX + bodyRadius / 2 - exhaustWidth / 2, centerY + bodyRadius,
                     exhaustWidth, exhaustLength, exhaustColor, true);

    // Frame name label
    SDL_Color labelColor = {200, 200, 200, 255};
    renderer.drawText(frame.name, centerX, centerY + bodyRadius + 20,
                     renderer.getFontSmall(), labelColor, TextAlign::Center);
}

} // namespace ScrapHeap
