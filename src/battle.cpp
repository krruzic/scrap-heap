#include "battle.h"
#include "game_context.h"
#include "game_setup.h"
#include "renderer.h"
#include "physics.h"
#include "combat.h"
#include "powerup.h"
#include "data.h"
#include "components.h"
#include <algorithm>
#include <cmath>

namespace ScrapHeap {

int BattleState::countAliveBots() const {
    int count = 0;
    for (const auto& bot : bots) {
        if (bot.isAlive) count++;
    }
    return count;
}

std::string BattleState::getWinnerName() const {
    if (result == BattleResult::Draw) return "DRAW";
    if (winnerIndex >= 0 && winnerIndex < static_cast<int>(bots.size())) {
        return bots[winnerIndex].displayName;
    }
    return "UNKNOWN";
}

bool BattleState::isOutsideSafeZone(float x, float y) const {
    if (!wall.active) return false;
    return x < wall.left || x > wall.right ||
           y < wall.top || y > wall.bottom;
}

BattleState BattleManager::createBattle(const PlayerSlot* slots, int stageIndex) {
    BattleState state;

    state.stageIndex = stageIndex;
    state.stage = StageRegistry::instance().getStage(stageIndex);

    // Create bots from player slots
    std::vector<int> joinedSlots;
    for (int i = 0; i < 4; ++i) {
        if (slots[i].state != PlayerSlotState::Empty) {
            joinedSlots.push_back(i);
        }
    }

    // Get spawn positions
    auto spawnPositions = getSpawnPositions(state.stage, static_cast<int>(joinedSlots.size()));

    for (size_t i = 0; i < joinedSlots.size(); ++i) {
        int slotIndex = joinedSlots[i];
        const PlayerSlot& slot = slots[slotIndex];

        Bot bot = createBot(
            static_cast<int>(i),
            slot.getDisplayName(slotIndex),
            slot.frameIndex,
            slot.engineIndex,
            slot.weaponIndex,
            slot.specialIndex,
            slot.colorIndex
        );

        // Set spawn position
        float spawnAngle = std::atan2(
            state.stage.height / 2.0f - spawnPositions[i].y,
            state.stage.width / 2.0f - spawnPositions[i].x
        );
        bot.reset(spawnPositions[i].x, spawnPositions[i].y, spawnAngle);

        state.bots.push_back(bot);

        // Initialize bot battle stats
        state.botStats[bot.playerIndex] = BotBattleStats();

        // Record component usage
        const auto& frame = ComponentRegistry::instance().getFrame(slot.frameIndex);
        const auto& engine = ComponentRegistry::instance().getEngine(slot.engineIndex);
        const auto& weapon = ComponentRegistry::instance().getWeapon(slot.weaponIndex);
        const auto& special = ComponentRegistry::instance().getSpecial(slot.specialIndex);

        DataManager::instance().recordComponentUsage(
            bot.displayName, frame.name, engine.name, weapon.name, special.name);
    }

    // Create powerups
    state.powerups = PowerupManager::instance().createPowerupsForStage(stageIndex);

    // Initialize shrinking wall (starts at stage boundaries)
    state.wall.left = 0.0f;
    state.wall.right = state.stage.width;
    state.wall.top = 0.0f;
    state.wall.bottom = state.stage.height;
    state.wall.active = false;
    state.wall.currentPhase = 0;
    state.wall.phaseTimer = 0.0f;

    // Set target for first shrink (toward center)
    float centerX = state.stage.width / 2.0f;
    float centerY = state.stage.height / 2.0f;
    float shrinkAmount = state.stage.width * 0.15f;  // 15% shrink per phase
    state.wall.targetLeft = shrinkAmount;
    state.wall.targetRight = state.stage.width - shrinkAmount;
    state.wall.targetTop = shrinkAmount;
    state.wall.targetBottom = state.stage.height - shrinkAmount;

    // Calculate camera offset to center stage
    state.cameraOffsetX = (WINDOW_WIDTH - state.stage.width) / 2.0f;
    state.cameraOffsetY = (WINDOW_HEIGHT - state.stage.height) / 2.0f + 30.0f;  // Leave room for HUD

    state.matchTimer = 0.0f;
    state.maxMatchTime = MATCH_DURATION;
    state.result = BattleResult::InProgress;

    return state;
}

void BattleManager::update(BattleState& state, GameContext& ctx, float dt) {
    if (state.paused) return;

    if (state.isGameOver()) {
        state.gameOverTimer += dt;
        // Check for confirmation input
        handleWinScreenInput(state, ctx);
        if (state.resultsConfirmed) {
            recordResults(state, ctx);
            ctx.changeState(GameState::MainMenu);
        }
        return;
    }

    state.matchTimer += dt;

    // Update input for all bots
    auto& input = InputManager::instance();
    for (auto& bot : state.bots) {
        if (!bot.isAlive) continue;

        // Find the player slot that matches this bot
        int slotIndex = -1;
        for (int i = 0; i < 4; ++i) {
            if (ctx.gameSetup.getSlots()[i].state != PlayerSlotState::Empty) {
                if (ctx.gameSetup.getSlots()[i].getDisplayName(i) == bot.displayName) {
                    slotIndex = i;
                    break;
                }
            }
        }

        if (slotIndex >= 0) {
            input.applyInputToBot(bot, slotIndex);
        }
    }

    // Update bots
    updateBots(state, dt);

    // Process weapons and combat
    Combat::processWeapons(state.bots, state.stage, state.combatEvents, dt);
    Combat::processGrabs(state.bots, state.combatEvents, dt);
    Combat::processSpecials(state.bots, state.combatEvents, dt);
    Combat::processHazards(state.bots, state.stage, state.combatEvents, dt);

    // Handle special activations
    for (auto& bot : state.bots) {
        if (!bot.isAlive) continue;

        if (bot.inputSpecialPressed) {
            const auto& special = ComponentRegistry::instance().getSpecial(bot.specialIndex);

            if (special.name == "Smoke") {
                SmokeCloud cloud;
                cloud.x = bot.x;
                cloud.y = bot.y;
                cloud.timer = SmokeCloud::DURATION;
                cloud.radius = SmokeCloud::MAX_RADIUS;
                cloud.ownerIndex = bot.playerIndex;
                state.smokeClouds.push_back(cloud);
                bot.specialCooldown = special.cooldown;
            } else if (special.name == "Mine") {
                Vec2 facing = bot.getFacingVector();
                Mine mine;
                mine.x = bot.x - facing.x * (bot.radius + Mine::RADIUS);
                mine.y = bot.y - facing.y * (bot.radius + Mine::RADIUS);
                mine.armTimer = Mine::ARM_TIME;
                mine.armed = false;
                mine.exploded = false;
                mine.ownerIndex = bot.playerIndex;
                state.mines.push_back(mine);
                bot.specialCooldown = special.cooldown;
            } else {
                Combat::activateSpecial(bot, state.bots, state.combatEvents);
            }
        }

        // Use held powerup
        if (bot.inputPowerupPressed) {
            PowerupManager::instance().useHeldPowerup(bot);
        }
    }

    // Update powerups
    PowerupManager::instance().update(state.powerups, dt);
    for (auto& bot : state.bots) {
        if (!bot.isAlive) continue;
        PowerupManager::instance().checkCollection(state.powerups, bot);
    }

    // Update mines and smoke
    updateMines(state, dt);
    updateSmokeClouds(state, dt);

    // Update shrinking wall
    updateShrinkingWall(state, dt);
    applyWallDamage(state, dt);

    // Update combat events and kill popups
    updateCombatEvents(state, dt);
    updateKillPopups(state, dt);

    // Check for game over
    checkGameOver(state);
}

void BattleManager::updateBots(BattleState& state, float dt) {
    // Update movement
    for (auto& bot : state.bots) {
        if (!bot.isAlive) continue;

        Physics::updateBotMovement(bot, dt);
        Physics::applyFriction(bot, dt);
        Physics::resolveWallCollisions(bot, state.stage);
    }

    // Resolve bot-bot collisions
    for (size_t i = 0; i < state.bots.size(); ++i) {
        if (!state.bots[i].isAlive) continue;

        for (size_t j = i + 1; j < state.bots.size(); ++j) {
            if (!state.bots[j].isAlive) continue;

            auto collision = Physics::checkBotCollision(state.bots[i], state.bots[j]);
            if (collision.collided) {
                Physics::resolveBotCollision(state.bots[i], state.bots[j], collision);
            }
        }
    }
}

void BattleManager::updateShrinkingWall(BattleState& state, float dt) {
    // Don't start wall until after initial delay
    if (state.matchTimer < ShrinkingWall::INITIAL_DELAY) {
        return;
    }

    // Activate wall on first update after delay
    if (!state.wall.active) {
        state.wall.active = true;
        state.wall.phaseTimer = 0.0f;
    }

    state.wall.phaseTimer += dt;

    // Shrink toward target
    float speed = state.wall.getShrinkSpeed() * dt;

    // Move boundaries toward targets
    if (state.wall.left < state.wall.targetLeft) {
        state.wall.left = std::min(state.wall.left + speed, state.wall.targetLeft);
    }
    if (state.wall.right > state.wall.targetRight) {
        state.wall.right = std::max(state.wall.right - speed, state.wall.targetRight);
    }
    if (state.wall.top < state.wall.targetTop) {
        state.wall.top = std::min(state.wall.top + speed, state.wall.targetTop);
    }
    if (state.wall.bottom > state.wall.targetBottom) {
        state.wall.bottom = std::max(state.wall.bottom - speed, state.wall.targetBottom);
    }

    // Check if we've reached target and should start next phase
    bool reachedTarget =
        std::abs(state.wall.left - state.wall.targetLeft) < 1.0f &&
        std::abs(state.wall.right - state.wall.targetRight) < 1.0f &&
        std::abs(state.wall.top - state.wall.targetTop) < 1.0f &&
        std::abs(state.wall.bottom - state.wall.targetBottom) < 1.0f;

    if (reachedTarget && state.wall.phaseTimer >= ShrinkingWall::PHASE_DURATION &&
        state.wall.currentPhase < ShrinkingWall::MAX_PHASES) {

        state.wall.currentPhase++;
        state.wall.phaseTimer = 0.0f;

        // Calculate new targets
        float centerX = state.stage.width / 2.0f;
        float centerY = state.stage.height / 2.0f;

        // Each phase shrinks more aggressively
        float shrinkAmount = state.stage.width * (0.1f + state.wall.currentPhase * 0.05f);

        float newLeft = state.wall.left + shrinkAmount;
        float newRight = state.wall.right - shrinkAmount;
        float newTop = state.wall.top + shrinkAmount;
        float newBottom = state.wall.bottom - shrinkAmount;

        // Clamp to minimum safe zone
        float minWidth = ShrinkingWall::FINAL_RADIUS * 2;
        float currentWidth = newRight - newLeft;
        float currentHeight = newBottom - newTop;

        if (currentWidth < minWidth) {
            float adjustment = (minWidth - currentWidth) / 2.0f;
            newLeft -= adjustment;
            newRight += adjustment;
        }
        if (currentHeight < minWidth) {
            float adjustment = (minWidth - currentHeight) / 2.0f;
            newTop -= adjustment;
            newBottom += adjustment;
        }

        state.wall.targetLeft = newLeft;
        state.wall.targetRight = newRight;
        state.wall.targetTop = newTop;
        state.wall.targetBottom = newBottom;

        // Increase damage per second each phase
        state.wall.damagePerSecond = 5.0f + state.wall.currentPhase * 3.0f;
    }
}

void BattleManager::applyWallDamage(BattleState& state, float dt) {
    if (!state.wall.active) return;

    for (auto& bot : state.bots) {
        if (!bot.isAlive) continue;

        if (state.isOutsideSafeZone(bot.x, bot.y)) {
            float damage = state.wall.damagePerSecond * dt;
            bot.health -= damage;

            // Track damage taken
            state.botStats[bot.playerIndex].damageTaken += damage;

            if (bot.health <= 0) {
                bot.health = 0;
                bot.isAlive = false;

                CombatEvent event;
                event.type = CombatEvent::Type::Death;
                event.x = bot.x;
                event.y = bot.y;
                event.targetBot = bot.playerIndex;
                event.sourceBot = -1;  // Wall kill (no player gets credit)
                event.timer = 1.0f;
                state.combatEvents.push_back(event);
            }
        }
    }
}

void BattleManager::updateMines(BattleState& state, float dt) {
    for (auto it = state.mines.begin(); it != state.mines.end(); ) {
        Mine& mine = *it;

        if (!mine.armed) {
            mine.armTimer -= dt;
            if (mine.armTimer <= 0) {
                mine.armed = true;
            }
        } else if (!mine.exploded) {
            // Check for bot contact
            for (auto& bot : state.bots) {
                if (!bot.isAlive) continue;
                if (bot.playerIndex == mine.ownerIndex) continue;  // Don't hit owner

                float dist = distance(mine.x, mine.y, bot.x, bot.y);
                if (dist < Mine::RADIUS + bot.radius) {
                    // Explode!
                    mine.exploded = true;

                    // Find mine owner for kill attribution
                    Bot* owner = nullptr;
                    for (auto& b : state.bots) {
                        if (b.playerIndex == mine.ownerIndex) {
                            owner = &b;
                            break;
                        }
                    }

                    // Damage all bots in explosion radius
                    for (auto& target : state.bots) {
                        if (!target.isAlive) continue;
                        float targetDist = distance(mine.x, mine.y, target.x, target.y);
                        if (targetDist < Mine::EXPLOSION_RADIUS) {
                            Vec2 knockDir(target.x - mine.x, target.y - mine.y);
                            Combat::applyDamage(target, Mine::DAMAGE, 150.0f, knockDir,
                                               owner, state.combatEvents);
                        }
                    }
                    break;
                }
            }
        }

        if (mine.exploded) {
            it = state.mines.erase(it);
        } else {
            ++it;
        }
    }
}

void BattleManager::updateSmokeClouds(BattleState& state, float dt) {
    for (auto it = state.smokeClouds.begin(); it != state.smokeClouds.end(); ) {
        SmokeCloud& cloud = *it;
        cloud.timer -= dt;

        if (cloud.timer <= 0) {
            it = state.smokeClouds.erase(it);
        } else {
            ++it;
        }
    }
}

void BattleManager::updateKillPopups(BattleState& state, float dt) {
    for (auto it = state.killPopups.begin(); it != state.killPopups.end(); ) {
        it->timer -= dt;
        if (it->timer <= 0) {
            it = state.killPopups.erase(it);
        } else {
            ++it;
        }
    }
}

void BattleManager::updateCombatEvents(BattleState& state, float dt) {
    for (auto it = state.combatEvents.begin(); it != state.combatEvents.end(); ) {
        // Process events for stats tracking (only once when newly created)
        bool isNewEvent = it->timer > 0.4f;  // Just spawned

        if (it->type == CombatEvent::Type::Damage && isNewEvent) {
            // Track damage dealt and taken
            if (it->sourceBot >= 0 && state.botStats.count(it->sourceBot)) {
                state.botStats[it->sourceBot].damageDealt += it->value;
            }
            if (it->targetBot >= 0 && state.botStats.count(it->targetBot)) {
                state.botStats[it->targetBot].damageTaken += it->value;
            }
        }

        if (it->type == CombatEvent::Type::Death && it->timer > 0.9f) {
            // Only process once (when timer is near max)
            // Track death for the victim
            if (it->targetBot >= 0 && state.botStats.count(it->targetBot)) {
                state.botStats[it->targetBot].deaths++;
            }
            // Track kill for the killer and create popup
            if (it->sourceBot >= 0 && state.botStats.count(it->sourceBot)) {
                state.botStats[it->sourceBot].kills++;

                // Create kill popup
                KillPopup popup;
                popup.playerIndex = it->sourceBot;
                popup.timer = KillPopup::DURATION;
                state.killPopups.push_back(popup);
            }
        }

        it->timer -= dt;
        if (it->timer <= 0) {
            it = state.combatEvents.erase(it);
        } else {
            ++it;
        }
    }
}

void BattleManager::handleInput(BattleState& state, GameContext& ctx) {
    auto& input = InputManager::instance();
    const auto& keyboard = input.getKeyboard();

    // Check for pause/quit
    if (keyboard.escapePressed()) {
        ctx.changeState(GameState::MainMenu);
    }

    // Check all controllers for start button
    for (int i = 0; i < 8; ++i) {
        const ControllerState* controller = input.getController(i);
        if (controller && controller->buttonStartPressed()) {
            ctx.changeState(GameState::MainMenu);
        }
    }
}

void BattleManager::handleWinScreenInput(BattleState& state, GameContext& ctx) {
    auto& input = InputManager::instance();
    const auto& keyboard = input.getKeyboard();

    // Check for Start button (any player) - instant confirm
    for (int i = 0; i < 8; ++i) {
        const ControllerState* controller = input.getController(i);
        if (controller && controller->buttonStartPressed()) {
            state.resultsConfirmed = true;
            return;
        }
    }

    // Keyboard: Enter or Space as alternative
    if (keyboard.enterPressed() || keyboard.spacePressed()) {
        state.resultsConfirmed = true;
        return;
    }

    // Check A button for each player
    int numPlayers = static_cast<int>(state.bots.size());
    for (int i = 0; i < numPlayers; ++i) {
        const Bot& bot = state.bots[i];

        // Find the slot for this bot
        for (int slot = 0; slot < 4; ++slot) {
            const auto& slotData = ctx.gameSetup.getSlots()[slot];
            if (slotData.state != PlayerSlotState::Empty &&
                slotData.getDisplayName(slot) == bot.displayName) {

                // Check controller input for this slot
                const ControllerState* controller = input.getController(slot);
                if (controller && controller->buttonAPressed()) {
                    state.playersConfirmed[i] = true;
                }
                break;
            }
        }
    }

    // Check if all players have confirmed
    bool allConfirmed = true;
    for (int i = 0; i < numPlayers; ++i) {
        if (!state.playersConfirmed[i]) {
            allConfirmed = false;
            break;
        }
    }
    if (allConfirmed) {
        state.resultsConfirmed = true;
    }
}

void BattleManager::checkGameOver(BattleState& state) {
    if (state.isGameOver()) return;

    int alive = state.countAliveBots();

    // Check for timeout
    if (state.matchTimer >= state.maxMatchTime) {
        // Find bot with most health
        int bestBot = -1;
        float bestHealth = -1;
        bool tie = false;

        for (size_t i = 0; i < state.bots.size(); ++i) {
            if (state.bots[i].isAlive) {
                if (state.bots[i].health > bestHealth) {
                    bestHealth = state.bots[i].health;
                    bestBot = static_cast<int>(i);
                    tie = false;
                } else if (state.bots[i].health == bestHealth) {
                    tie = true;
                }
            }
        }

        if (tie || bestBot < 0) {
            state.result = BattleResult::Draw;
        } else {
            state.result = BattleResult::Winner;
            state.winnerIndex = bestBot;
        }
        return;
    }

    // Check for single survivor
    if (alive == 1) {
        for (size_t i = 0; i < state.bots.size(); ++i) {
            if (state.bots[i].isAlive) {
                state.result = BattleResult::Winner;
                state.winnerIndex = static_cast<int>(i);
                break;
            }
        }
    } else if (alive == 0) {
        state.result = BattleResult::Draw;
    }
}

void BattleManager::recordResults(const BattleState& state, GameContext& ctx) {
    auto& data = DataManager::instance();
    data.incrementMatchCount();
    data.addPlayTime(state.matchTimer);

    bool isDraw = (state.result == BattleResult::Draw);

    for (size_t i = 0; i < state.bots.size(); ++i) {
        const auto& bot = state.bots[i];
        const auto& stats = state.botStats.at(bot.playerIndex);

        bool won = !isDraw && static_cast<int>(i) == state.winnerIndex;

        data.recordMatchResult(
            bot.displayName,
            won,
            isDraw,
            stats.kills,
            stats.deaths,
            stats.damageDealt,
            stats.damageTaken,
            state.matchTimer
        );
    }
}

void BattleManager::render(const BattleState& state) {
    auto& renderer = Renderer::instance();

    // Draw stage
    renderer.drawStage(state.stage, state.cameraOffsetX, state.cameraOffsetY);

    // Draw shrinking wall (before bots so they appear on top)
    renderShrinkingWall(state);

    // Draw powerups
    for (const auto& powerup : state.powerups) {
        renderer.drawPowerup(powerup, state.cameraOffsetX, state.cameraOffsetY);
    }

    // Draw mines
    for (const auto& mine : state.mines) {
        renderer.drawMine(mine, state.cameraOffsetX, state.cameraOffsetY);
    }

    // Draw smoke clouds
    for (const auto& cloud : state.smokeClouds) {
        renderer.drawSmokeCloud(cloud, state.cameraOffsetX, state.cameraOffsetY);
    }

    // Draw grab tethers
    for (const auto& bot : state.bots) {
        if (bot.grabState == GrabState::Grabbing) {
            for (const auto& other : state.bots) {
                if (other.playerIndex == bot.grabbingBot) {
                    renderer.drawGrabTether(bot, other, state.cameraOffsetX, state.cameraOffsetY);
                    break;
                }
            }
        }
    }

    // Draw bots
    for (const auto& bot : state.bots) {
        if (!bot.isAlive) continue;

        SDL_Color color = Renderer::getPlayerColor(bot.colorIndex);
        renderer.drawBot(bot, color);
    }

    // Draw combat events
    for (const auto& event : state.combatEvents) {
        renderer.drawCombatEvent(event, state.cameraOffsetX, state.cameraOffsetY);
    }

    // Draw HUD
    renderHUD(state);

    // Draw game over overlay if needed
    if (state.isGameOver()) {
        renderGameOver(state);
    }
}

void BattleManager::renderShrinkingWall(const BattleState& state) {
    if (!state.wall.active) return;

    auto& renderer = Renderer::instance();

    // Translucent purple color for the danger zone
    SDL_Color wallColor = {128, 0, 180, 100};  // Purple with transparency

    float ox = state.cameraOffsetX;
    float oy = state.cameraOffsetY;

    // Draw four rectangles for the wall areas (outside safe zone)
    // Left wall
    if (state.wall.left > 0) {
        renderer.drawRect(ox, oy, state.wall.left, state.stage.height, wallColor, true);
    }

    // Right wall
    if (state.wall.right < state.stage.width) {
        renderer.drawRect(ox + state.wall.right, oy,
                         state.stage.width - state.wall.right, state.stage.height, wallColor, true);
    }

    // Top wall (between left and right walls)
    if (state.wall.top > 0) {
        renderer.drawRect(ox + state.wall.left, oy,
                         state.wall.right - state.wall.left, state.wall.top, wallColor, true);
    }

    // Bottom wall (between left and right walls)
    if (state.wall.bottom < state.stage.height) {
        renderer.drawRect(ox + state.wall.left, oy + state.wall.bottom,
                         state.wall.right - state.wall.left,
                         state.stage.height - state.wall.bottom, wallColor, true);
    }

    // Draw safe zone border
    SDL_Color borderColor = {200, 100, 255, 200};
    renderer.drawRectOutline(ox + state.wall.left, oy + state.wall.top,
                            state.wall.right - state.wall.left,
                            state.wall.bottom - state.wall.top,
                            borderColor, 2.0f);

    // Draw target zone border (where wall is heading)
    if (state.wall.currentPhase < ShrinkingWall::MAX_PHASES) {
        SDL_Color targetColor = {255, 150, 255, 80};
        renderer.drawRectOutline(ox + state.wall.targetLeft, oy + state.wall.targetTop,
                                state.wall.targetRight - state.wall.targetLeft,
                                state.wall.targetBottom - state.wall.targetTop,
                                targetColor, 1.0f);
    }
}

void BattleManager::renderHUD(const BattleState& state) {
    auto& renderer = Renderer::instance();

    float hudY = 10;
    float barWidth = 150;
    float barHeight = 20;
    float spacing = 20;

    // Health bars for each player (top center)
    float totalWidth = state.bots.size() * (barWidth + spacing) - spacing;
    float startX = (WINDOW_WIDTH - totalWidth) / 2.0f;

    for (size_t i = 0; i < state.bots.size(); ++i) {
        const auto& bot = state.bots[i];
        float x = startX + i * (barWidth + spacing);

        SDL_Color playerColor = Renderer::getPlayerColor(bot.colorIndex);
        SDL_Color bgColor = {40, 40, 50, 255};

        // Grey out dead players
        if (!bot.isAlive) {
            playerColor = {100, 100, 100, 255};
        }

        // Player name
        renderer.drawText(bot.displayName, x + barWidth / 2, hudY,
                         renderer.getFontSmall(), playerColor, TextAlign::Center);

        // Health bar
        renderer.drawHealthBar(x, hudY + 25, barWidth, barHeight,
                              bot.health, bot.maxHealth, playerColor, bgColor);

        // Escape progress bar if grabbed
        if (bot.grabState == GrabState::Grabbed) {
            SDL_Color escapeColor = {255, 200, 50, 255};
            renderer.drawProgressBar(x, hudY + 48, barWidth, 8,
                                    bot.grabEscapeProgress, escapeColor, bgColor);
        }
    }

    // Corner score displays (kills count)
    // Positions: P1=top-left, P2=top-right, P3=bottom-left, P4=bottom-right
    float cornerMargin = 20.0f;
    float cornerPositions[4][2] = {
        {cornerMargin, 90},                                    // Top-left
        {WINDOW_WIDTH - cornerMargin, 90},                     // Top-right
        {cornerMargin, WINDOW_HEIGHT - cornerMargin - 30},     // Bottom-left
        {WINDOW_WIDTH - cornerMargin, WINDOW_HEIGHT - cornerMargin - 30}  // Bottom-right
    };
    TextAlign cornerAligns[4] = {
        TextAlign::Left, TextAlign::Right, TextAlign::Left, TextAlign::Right
    };

    for (size_t i = 0; i < state.bots.size() && i < 4; ++i) {
        const auto& bot = state.bots[i];
        const auto& stats = state.botStats.at(bot.playerIndex);
        SDL_Color playerColor = Renderer::getPlayerColor(bot.colorIndex);

        if (!bot.isAlive) {
            playerColor = {100, 100, 100, 255};
        }

        float px = cornerPositions[i][0];
        float py = cornerPositions[i][1];
        TextAlign align = cornerAligns[i];

        // Draw score box background
        float boxWidth = 80;
        float boxHeight = 50;
        float boxX = (align == TextAlign::Left) ? px - 5 : px - boxWidth + 5;
        SDL_Color boxBg = {20, 20, 30, 180};
        renderer.drawRect(boxX, py - 5, boxWidth, boxHeight, boxBg, true);

        // Player indicator (small colored square)
        float sqSize = 12;
        float sqX = (align == TextAlign::Left) ? px : px - sqSize;
        renderer.drawRect(sqX, py, sqSize, sqSize, playerColor, true);

        // KO count
        std::string koText = std::to_string(stats.kills) + " KO";
        float textX = (align == TextAlign::Left) ? px + sqSize + 5 : px - sqSize - 5;
        renderer.drawText(koText, textX, py - 2, renderer.getFontSmall(),
                         {255, 255, 255, 255}, align);

        // Show kill popups (+1) for this player
        for (const auto& popup : state.killPopups) {
            if (popup.playerIndex == bot.playerIndex) {
                uint8_t alpha = static_cast<uint8_t>(popup.getAlpha() * 255);
                SDL_Color popupColor = {100, 255, 100, alpha};
                float popupY = py + 18 - (1.0f - popup.timer) * 20;  // Float upward
                float popupX = (align == TextAlign::Left) ? px + 50 : px - 50;
                renderer.drawText("+1", popupX, popupY, renderer.getFontSmall(),
                                 popupColor, TextAlign::Center);
            }
        }
    }

    // Timer
    int timeLeft = static_cast<int>(state.maxMatchTime - state.matchTimer);
    if (timeLeft < 0) timeLeft = 0;
    int minutes = timeLeft / 60;
    int seconds = timeLeft % 60;

    char timerStr[16];
    snprintf(timerStr, sizeof(timerStr), "%d:%02d", minutes, seconds);

    SDL_Color timerColor = (timeLeft <= 30) ?
        SDL_Color{255, 100, 100, 255} : SDL_Color{200, 200, 200, 255};
    renderer.drawTextShadow(timerStr, WINDOW_WIDTH / 2.0f, hudY + 55,
                           renderer.getFontMedium(), timerColor, TextAlign::Center);

    // Wall warning
    if (state.wall.active) {
        float timeUntilShrink = ShrinkingWall::PHASE_DURATION - state.wall.phaseTimer;
        if (timeUntilShrink > 0 && timeUntilShrink <= 5.0f) {
            SDL_Color warningColor = {255, 100, 255, 255};
            renderer.drawText("WALL CLOSING!", WINDOW_WIDTH / 2.0f, WINDOW_HEIGHT - 80,
                             renderer.getFontSmall(), warningColor, TextAlign::Center);
        }
    } else if (state.matchTimer >= ShrinkingWall::INITIAL_DELAY - 5.0f &&
               state.matchTimer < ShrinkingWall::INITIAL_DELAY) {
        // Warning before wall appears
        SDL_Color warningColor = {255, 150, 255, 255};
        int countdown = static_cast<int>(ShrinkingWall::INITIAL_DELAY - state.matchTimer) + 1;
        std::string warning = "WALL APPEARS IN " + std::to_string(countdown);
        renderer.drawText(warning, WINDOW_WIDTH / 2.0f, WINDOW_HEIGHT - 80,
                         renderer.getFontSmall(), warningColor, TextAlign::Center);
    }
}

void BattleManager::renderGameOver(const BattleState& state) {
    auto& renderer = Renderer::instance();

    // Darken screen
    SDL_Color overlay = {0, 0, 0, 200};
    renderer.drawRect(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, overlay, true);

    // Title - "GAME!" or "DRAW"
    std::string titleText = (state.result == BattleResult::Draw) ? "DRAW!" : "GAME!";
    SDL_Color titleColor = {255, 200, 50, 255};
    renderer.drawTextShadow(titleText, WINDOW_WIDTH / 2.0f, 60,
                           renderer.getFontLarge(), titleColor, TextAlign::Center);

    // Calculate panel layout
    int numPlayers = static_cast<int>(state.bots.size());
    float panelWidth = 180.0f;
    float panelHeight = 320.0f;
    float panelSpacing = 30.0f;
    float totalWidth = numPlayers * panelWidth + (numPlayers - 1) * panelSpacing;
    float startX = (WINDOW_WIDTH - totalWidth) / 2.0f;
    float panelY = 120.0f;

    // Sort players by kills (for placement display)
    std::vector<int> placements(numPlayers);
    for (int i = 0; i < numPlayers; ++i) placements[i] = i;
    std::sort(placements.begin(), placements.end(), [&state](int a, int b) {
        const auto& statsA = state.botStats.at(state.bots[a].playerIndex);
        const auto& statsB = state.botStats.at(state.bots[b].playerIndex);
        if (statsA.kills != statsB.kills) return statsA.kills > statsB.kills;
        return statsA.damageDealt > statsB.damageDealt;
    });

    // Get placement rank for each player
    std::vector<int> ranks(numPlayers);
    for (int i = 0; i < numPlayers; ++i) {
        ranks[placements[i]] = i + 1;
    }

    // Draw player panels
    for (int i = 0; i < numPlayers; ++i) {
        const auto& bot = state.bots[i];
        const auto& stats = state.botStats.at(bot.playerIndex);
        bool isWinner = (state.result == BattleResult::Winner && i == state.winnerIndex);

        float panelX = startX + i * (panelWidth + panelSpacing);
        SDL_Color playerColor = Renderer::getPlayerColor(bot.colorIndex);

        // Panel background
        SDL_Color panelBg = isWinner ? SDL_Color{60, 50, 20, 240} : SDL_Color{30, 30, 40, 240};
        renderer.drawRect(panelX, panelY, panelWidth, panelHeight, panelBg, true);

        // Panel border (thicker for winner)
        SDL_Color borderColor = isWinner ? SDL_Color{255, 200, 50, 255} : SDL_Color{80, 80, 100, 255};
        float borderWidth = isWinner ? 4.0f : 2.0f;
        renderer.drawRectOutline(panelX, panelY, panelWidth, panelHeight, borderColor, borderWidth);

        // Winner crown / placement indicator
        float contentY = panelY + 15;
        if (isWinner) {
            SDL_Color crownColor = {255, 215, 0, 255};
            renderer.drawTextShadow("WINNER", panelX + panelWidth / 2, contentY,
                                   renderer.getFontSmall(), crownColor, TextAlign::Center);
        } else {
            std::string placeStr = "#" + std::to_string(ranks[i]);
            SDL_Color placeColor = {150, 150, 150, 255};
            renderer.drawText(placeStr, panelX + panelWidth / 2, contentY,
                             renderer.getFontSmall(), placeColor, TextAlign::Center);
        }
        contentY += 30;

        // Player name
        renderer.drawTextShadow(bot.displayName, panelX + panelWidth / 2, contentY,
                               renderer.getFontMedium(), playerColor, TextAlign::Center);
        contentY += 35;

        // Bot visual representation (simple colored circle)
        float botPreviewX = panelX + panelWidth / 2;
        float botPreviewY = contentY + 25;
        SDL_Color previewColor = bot.isAlive ? playerColor : SDL_Color{100, 100, 100, 255};
        renderer.drawRect(botPreviewX - 25, botPreviewY - 25, 50, 50, previewColor, true);
        if (!bot.isAlive) {
            SDL_Color xColor = {200, 50, 50, 255};
            renderer.drawText("X", botPreviewX, botPreviewY - 8,
                             renderer.getFontMedium(), xColor, TextAlign::Center);
        }
        contentY += 70;

        // Stats section
        SDL_Color labelColor = {150, 150, 160, 255};
        SDL_Color valueColor = {255, 255, 255, 255};
        float labelX = panelX + 15;
        float valueX = panelX + panelWidth - 15;
        float statSpacing = 35;

        // KOs (Kills)
        renderer.drawText("KOs", labelX, contentY, renderer.getFontSmall(), labelColor, TextAlign::Left);
        SDL_Color koColor = stats.kills > 0 ? SDL_Color{100, 255, 100, 255} : valueColor;
        renderer.drawText(std::to_string(stats.kills), valueX, contentY,
                         renderer.getFontSmall(), koColor, TextAlign::Right);
        contentY += statSpacing;

        // Falls (Deaths)
        renderer.drawText("Falls", labelX, contentY, renderer.getFontSmall(), labelColor, TextAlign::Left);
        SDL_Color deathColor = stats.deaths > 0 ? SDL_Color{255, 100, 100, 255} : valueColor;
        renderer.drawText(std::to_string(stats.deaths), valueX, contentY,
                         renderer.getFontSmall(), deathColor, TextAlign::Right);
        contentY += statSpacing;

        // Damage dealt
        renderer.drawText("Damage", labelX, contentY, renderer.getFontSmall(), labelColor, TextAlign::Left);
        char dmgStr[32];
        snprintf(dmgStr, sizeof(dmgStr), "%.0f%%", stats.damageDealt);
        renderer.drawText(dmgStr, valueX, contentY, renderer.getFontSmall(), valueColor, TextAlign::Right);
        contentY += statSpacing;

        // Damage taken
        renderer.drawText("Taken", labelX, contentY, renderer.getFontSmall(), labelColor, TextAlign::Left);
        char takenStr[32];
        snprintf(takenStr, sizeof(takenStr), "%.0f%%", stats.damageTaken);
        SDL_Color takenColor = {255, 180, 100, 255};
        renderer.drawText(takenStr, valueX, contentY, renderer.getFontSmall(), takenColor, TextAlign::Right);
    }

    // Match time display
    int totalSeconds = static_cast<int>(state.matchTimer);
    int minutes = totalSeconds / 60;
    int seconds = totalSeconds % 60;
    char timeStr[32];
    snprintf(timeStr, sizeof(timeStr), "Time: %d:%02d", minutes, seconds);
    SDL_Color timeColor = {180, 180, 180, 255};
    renderer.drawText(timeStr, WINDOW_WIDTH / 2.0f, panelY + panelHeight + 30,
                     renderer.getFontSmall(), timeColor, TextAlign::Center);

    // Player confirmation status
    float confirmY = panelY + panelHeight + 60;
    int confirmedCount = 0;
    for (int i = 0; i < numPlayers; ++i) {
        if (state.playersConfirmed[i]) confirmedCount++;
    }

    // Show confirmation indicators for each player
    float confirmTotalWidth = numPlayers * 30.0f + (numPlayers - 1) * 15.0f;
    float confirmStartX = (WINDOW_WIDTH - confirmTotalWidth) / 2.0f;
    for (int i = 0; i < numPlayers; ++i) {
        float indicatorX = confirmStartX + i * 45.0f;
        SDL_Color playerColor = Renderer::getPlayerColor(state.bots[i].colorIndex);

        if (state.playersConfirmed[i]) {
            // Confirmed - show checkmark with player color
            renderer.drawRect(indicatorX, confirmY, 30, 30, playerColor, true);
            renderer.drawText("OK", indicatorX + 15, confirmY + 6,
                             renderer.getFontSmall(), {255, 255, 255, 255}, TextAlign::Center);
        } else {
            // Not confirmed - dim outline
            SDL_Color dimColor = {playerColor.r / 2, playerColor.g / 2, playerColor.b / 2, 150};
            renderer.drawRectOutline(indicatorX, confirmY, 30, 30, dimColor, 2.0f);
            renderer.drawText("?", indicatorX + 15, confirmY + 6,
                             renderer.getFontSmall(), dimColor, TextAlign::Center);
        }
    }

    // Prompt at bottom
    SDL_Color promptColor = {200, 200, 200, 255};
    renderer.drawText("Press A to confirm or START to continue",
                     WINDOW_WIDTH / 2.0f, WINDOW_HEIGHT - 40,
                     renderer.getFontSmall(), promptColor, TextAlign::Center);
}

} // namespace ScrapHeap
