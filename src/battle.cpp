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
    // Handle respawning
    for (auto& bot : state.bots) {
        if (!bot.isAlive && !bot.isRespawning) {
            // Bot just died, start respawn timer
            bot.isRespawning = true;
            bot.respawnTimer = Bot::RESPAWN_DELAY;
        }

        if (bot.isRespawning) {
            bot.respawnTimer -= dt;
            if (bot.respawnTimer <= 0) {
                // Respawn the bot
                bot.isRespawning = false;
                bot.isAlive = true;
                bot.health = bot.maxHealth;

                // Find a safe spawn position
                float spawnX, spawnY;
                if (state.wall.active) {
                    // Spawn within safe zone
                    float safeW = state.wall.right - state.wall.left;
                    float safeH = state.wall.bottom - state.wall.top;
                    float margin = bot.radius * 2;
                    spawnX = state.wall.left + margin + (safeW - margin * 2) * (0.2f + 0.6f * (bot.playerIndex % 2));
                    spawnY = state.wall.top + margin + (safeH - margin * 2) * (0.2f + 0.6f * ((bot.playerIndex / 2) % 2));
                } else {
                    // Use regular spawn positions
                    auto spawns = getSpawnPositions(state.stage, 4);
                    int spawnIdx = bot.playerIndex % static_cast<int>(spawns.size());
                    spawnX = spawns[spawnIdx].x;
                    spawnY = spawns[spawnIdx].y;
                }

                float spawnAngle = std::atan2(
                    state.stage.height / 2.0f - spawnY,
                    state.stage.width / 2.0f - spawnX
                );
                bot.reset(spawnX, spawnY, spawnAngle);

                // Clear any status effects
                bot.speedBoostTimer = 0;
                bot.damageBoostTimer = 0;
                bot.shieldActive = false;
                bot.boostActive = false;
                bot.berserkActive = false;
                bot.specialCooldown = 0;
                bot.heldPowerup = -1;
            }
        }
    }

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
    // Update animation timers (even when inactive for visual continuity)
    state.wall.animTimer += dt * 3.0f;  // Fast animation
    state.wall.pulseTimer += dt * 1.5f;  // Slower pulse

    // Randomize arc offsets periodically for lightning effect
    if (static_cast<int>(state.wall.animTimer * 10) % 3 == 0) {
        for (int i = 0; i < 16; ++i) {
            state.wall.arcOffsets[i] = (std::sin(state.wall.animTimer * 7.0f + i * 2.3f) +
                                        std::cos(state.wall.animTimer * 11.0f + i * 1.7f)) * 8.0f;
        }
    }

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
        // Process events for stats tracking (only once per event)
        if (!it->statsProcessed) {
            it->statsProcessed = true;

            if (it->type == CombatEvent::Type::Damage) {
                // Track damage dealt and taken
                if (it->sourceBot >= 0 && state.botStats.count(it->sourceBot)) {
                    state.botStats[it->sourceBot].damageDealt += it->value;
                }
                if (it->targetBot >= 0 && state.botStats.count(it->targetBot)) {
                    state.botStats[it->targetBot].damageTaken += it->value;
                }
            }

            if (it->type == CombatEvent::Type::Death) {
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

                    // Reset the storm on player kill
                    state.wall.reset(state.stage.width, state.stage.height);
                }
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

    // Only end game on timeout (infinite respawns, timed match)
    if (state.matchTimer >= state.maxMatchTime) {
        // Find bot with most kills
        int bestBot = -1;
        int bestKills = -1;
        bool tie = false;

        for (size_t i = 0; i < state.bots.size(); ++i) {
            auto it = state.botStats.find(state.bots[i].playerIndex);
            int kills = (it != state.botStats.end()) ? it->second.kills : 0;

            if (kills > bestKills) {
                bestKills = kills;
                bestBot = static_cast<int>(i);
                tie = false;
            } else if (kills == bestKills && kills > 0) {
                tie = true;
            }
        }

        if (tie || bestBot < 0) {
            state.result = BattleResult::Draw;
        } else {
            state.result = BattleResult::Winner;
            state.winnerIndex = bestBot;
        }
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

    // Draw powerups
    for (const auto& powerup : state.powerups) {
        renderer.drawPowerup(powerup, state.cameraOffsetX, state.cameraOffsetY);
    }

    // Draw mines
    for (const auto& mine : state.mines) {
        renderer.drawMine(mine, state.cameraOffsetX, state.cameraOffsetY);
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
        renderer.drawBot(bot, color, state.cameraOffsetX, state.cameraOffsetY);
    }

    // Draw combat events
    for (const auto& event : state.combatEvents) {
        renderer.drawCombatEvent(event, state.cameraOffsetX, state.cameraOffsetY);
    }

    // Draw smoke clouds ON TOP of bots
    for (const auto& cloud : state.smokeClouds) {
        renderer.drawSmokeCloud(cloud, state.cameraOffsetX, state.cameraOffsetY);
    }

    // Draw shrinking wall ON TOP of everything
    renderShrinkingWall(state);

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

    float ox = state.cameraOffsetX;
    float oy = state.cameraOffsetY;

    // Animated pulse effect
    float pulse = 0.7f + 0.3f * std::sin(state.wall.pulseTimer * 2.0f);
    float fastPulse = 0.5f + 0.5f * std::sin(state.wall.animTimer * 5.0f);

    // Safe zone boundaries
    float safeX = ox + state.wall.left;
    float safeY = oy + state.wall.top;
    float safeW = state.wall.right - state.wall.left;
    float safeH = state.wall.bottom - state.wall.top;

    // === Layer 1: Dense storm background (~80% opacity) ===
    uint8_t baseAlpha = static_cast<uint8_t>(190 + 30 * pulse);
    SDL_Color stormBase = {25, 5, 50, baseAlpha};

    // Draw storm zone rectangles (the dangerous areas outside safe zone)
    // Left zone
    if (state.wall.left > 0) {
        renderer.drawRect(ox, oy, state.wall.left, state.stage.height, stormBase, true);
    }
    // Right zone
    if (state.wall.right < state.stage.width) {
        renderer.drawRect(ox + state.wall.right, oy,
                         state.stage.width - state.wall.right, state.stage.height, stormBase, true);
    }
    // Top zone (between left and right)
    if (state.wall.top > 0) {
        renderer.drawRect(ox + state.wall.left, oy,
                         state.wall.right - state.wall.left, state.wall.top, stormBase, true);
    }
    // Bottom zone (between left and right)
    if (state.wall.bottom < state.stage.height) {
        renderer.drawRect(ox + state.wall.left, oy + state.wall.bottom,
                         state.wall.right - state.wall.left,
                         state.stage.height - state.wall.bottom, stormBase, true);
    }

    // === Layer 2: Pulsing energy overlay ===
    uint8_t energyAlpha = static_cast<uint8_t>(60 + 40 * fastPulse);
    SDL_Color energyColor = {80, 40, 150, energyAlpha};

    if (state.wall.left > 0) {
        renderer.drawRect(ox, oy, state.wall.left, state.stage.height, energyColor, true);
    }
    if (state.wall.right < state.stage.width) {
        renderer.drawRect(ox + state.wall.right, oy,
                         state.stage.width - state.wall.right, state.stage.height, energyColor, true);
    }
    if (state.wall.top > 0) {
        renderer.drawRect(ox + state.wall.left, oy,
                         state.wall.right - state.wall.left, state.wall.top, energyColor, true);
    }
    if (state.wall.bottom < state.stage.height) {
        renderer.drawRect(ox + state.wall.left, oy + state.wall.bottom,
                         state.wall.right - state.wall.left,
                         state.stage.height - state.wall.bottom, energyColor, true);
    }

    // === Layer 3: Lightning bolts throughout the storm zones ===
    // Generate random-looking lightning using animated offsets
    auto drawStormLightning = [&](float zoneX, float zoneY, float zoneW, float zoneH) {
        if (zoneW < 5 || zoneH < 5) return;

        // Draw multiple lightning bolts in the zone
        int numBolts = static_cast<int>((zoneW * zoneH) / 3000.0f) + 2;
        numBolts = std::min(numBolts, 8);

        for (int b = 0; b < numBolts; ++b) {
            // Pseudo-random position based on animation timer and bolt index
            float boltSeed = state.wall.animTimer * 3.0f + b * 7.3f;
            float boltX = zoneX + zoneW * (0.1f + 0.8f * std::abs(std::sin(boltSeed * 1.7f)));
            float boltY = zoneY + zoneH * (0.1f + 0.8f * std::abs(std::cos(boltSeed * 2.3f)));

            // Flicker effect - some bolts visible, some not
            float flicker = std::sin(state.wall.animTimer * 20.0f + b * 4.1f);
            if (flicker < 0.2f) continue;

            uint8_t boltAlpha = static_cast<uint8_t>(150 + 105 * flicker);

            // Alternate colors
            SDL_Color boltColor = (b % 3 == 0) ?
                SDL_Color{150, 220, 255, boltAlpha} :  // Cyan
                (b % 3 == 1) ?
                SDL_Color{220, 180, 255, boltAlpha} :  // Purple
                SDL_Color{255, 255, 255, boltAlpha};   // White

            // Draw a jagged lightning bolt (3-4 segments)
            float segLen = 15.0f + 10.0f * std::sin(boltSeed);
            float x1 = boltX;
            float y1 = boltY;

            for (int seg = 0; seg < 4; ++seg) {
                float angle = -PI/2 + std::sin(boltSeed + seg * 2.1f) * 0.8f;
                float x2 = x1 + std::cos(angle) * segLen;
                float y2 = y1 + std::sin(angle) * segLen;

                // Keep within zone bounds
                x2 = std::max(zoneX, std::min(zoneX + zoneW, x2));
                y2 = std::max(zoneY, std::min(zoneY + zoneH, y2));

                renderer.drawLine(x1, y1, x2, y2, boltColor, 2.0f);

                // Branch occasionally
                if (seg == 1 && flicker > 0.6f) {
                    float branchAngle = angle + (std::sin(boltSeed) > 0 ? 0.7f : -0.7f);
                    float bx = x1 + std::cos(branchAngle) * segLen * 0.6f;
                    float by = y1 + std::sin(branchAngle) * segLen * 0.6f;
                    SDL_Color branchColor = boltColor;
                    branchColor.a = boltAlpha / 2;
                    renderer.drawLine(x1, y1, bx, by, branchColor, 1.0f);
                }

                x1 = x2;
                y1 = y2;
            }
        }
    };

    // Draw lightning in each storm zone
    if (state.wall.left > 0) {
        drawStormLightning(ox, oy, state.wall.left, state.stage.height);
    }
    if (state.wall.right < state.stage.width) {
        drawStormLightning(ox + state.wall.right, oy,
                          state.stage.width - state.wall.right, state.stage.height);
    }
    if (state.wall.top > 0) {
        drawStormLightning(ox + state.wall.left, oy,
                          state.wall.right - state.wall.left, state.wall.top);
    }
    if (state.wall.bottom < state.stage.height) {
        drawStormLightning(ox + state.wall.left, oy + state.wall.bottom,
                          state.wall.right - state.wall.left,
                          state.stage.height - state.wall.bottom);
    }

    // === Layer 4: Main energy border (bright electric) ===
    // Outer glow (pulsing)
    uint8_t glowAlpha = static_cast<uint8_t>(120 + 80 * pulse);
    SDL_Color outerGlow = {100, 50, 200, static_cast<uint8_t>(glowAlpha / 2)};
    renderer.drawRectOutline(safeX - 6, safeY - 6, safeW + 12, safeH + 12, outerGlow, 8.0f);

    SDL_Color midGlow = {150, 100, 255, glowAlpha};
    renderer.drawRectOutline(safeX - 3, safeY - 3, safeW + 6, safeH + 6, midGlow, 4.0f);

    // Core border (bright cyan/white)
    uint8_t coreAlpha = static_cast<uint8_t>(200 + 55 * fastPulse);
    SDL_Color coreBorder = {200, 230, 255, coreAlpha};
    renderer.drawRectOutline(safeX, safeY, safeW, safeH, coreBorder, 2.0f);

    // === Layer 5: Lightning arcs along the border ===
    int numArcs = 10;
    float segmentW = safeW / numArcs;
    float segmentH = safeH / numArcs;

    for (int i = 0; i < numArcs; ++i) {
        float arcOffset = state.wall.arcOffsets[i % 16];
        float arcOffset2 = state.wall.arcOffsets[(i + 8) % 16];

        // Flicker effect
        float flicker = (std::sin(state.wall.animTimer * 18.0f + i * 1.7f) > 0.2f) ? 1.0f : 0.3f;
        uint8_t arcAlpha = static_cast<uint8_t>(220 * flicker);

        // Electric colors - alternating cyan and purple
        SDL_Color arcColor = (i % 2 == 0) ?
            SDL_Color{120, 220, 255, arcAlpha} :
            SDL_Color{220, 170, 255, arcAlpha};

        // Top edge arcs (pointing into storm)
        float topX1 = safeX + i * segmentW;
        float topX2 = safeX + (i + 1) * segmentW;
        float topY = safeY - std::abs(arcOffset) * 0.8f;
        renderer.drawLine(topX1, safeY, topX1 + segmentW * 0.5f, topY, arcColor, 2.0f);
        renderer.drawLine(topX1 + segmentW * 0.5f, topY, topX2, safeY, arcColor, 2.0f);

        // Bottom edge arcs
        float botY = safeY + safeH + std::abs(arcOffset2) * 0.8f;
        renderer.drawLine(topX1, safeY + safeH, topX1 + segmentW * 0.5f, botY, arcColor, 2.0f);
        renderer.drawLine(topX1 + segmentW * 0.5f, botY, topX2, safeY + safeH, arcColor, 2.0f);

        // Left edge arcs
        float leftY1 = safeY + i * segmentH;
        float leftY2 = safeY + (i + 1) * segmentH;
        float leftX = safeX - std::abs(arcOffset) * 0.8f;
        renderer.drawLine(safeX, leftY1, leftX, leftY1 + segmentH * 0.5f, arcColor, 2.0f);
        renderer.drawLine(leftX, leftY1 + segmentH * 0.5f, safeX, leftY2, arcColor, 2.0f);

        // Right edge arcs
        float rightX = safeX + safeW + std::abs(arcOffset2) * 0.8f;
        renderer.drawLine(safeX + safeW, leftY1, rightX, leftY1 + segmentH * 0.5f, arcColor, 2.0f);
        renderer.drawLine(rightX, leftY1 + segmentH * 0.5f, safeX + safeW, leftY2, arcColor, 2.0f);
    }

    // === Layer 6: Spark particles at corners ===
    float sparkSize = 5.0f + 4.0f * fastPulse;
    SDL_Color sparkColor = {255, 255, 255, static_cast<uint8_t>(220 * pulse)};

    // Corner sparks with glow
    SDL_Color sparkGlow = {150, 200, 255, static_cast<uint8_t>(100 * pulse)};
    float glowSize = sparkSize * 2.5f;

    renderer.drawRect(safeX - glowSize/2, safeY - glowSize/2, glowSize, glowSize, sparkGlow, true);
    renderer.drawRect(safeX - sparkSize/2, safeY - sparkSize/2, sparkSize, sparkSize, sparkColor, true);

    renderer.drawRect(safeX + safeW - glowSize/2, safeY - glowSize/2, glowSize, glowSize, sparkGlow, true);
    renderer.drawRect(safeX + safeW - sparkSize/2, safeY - sparkSize/2, sparkSize, sparkSize, sparkColor, true);

    renderer.drawRect(safeX - glowSize/2, safeY + safeH - glowSize/2, glowSize, glowSize, sparkGlow, true);
    renderer.drawRect(safeX - sparkSize/2, safeY + safeH - sparkSize/2, sparkSize, sparkSize, sparkColor, true);

    renderer.drawRect(safeX + safeW - glowSize/2, safeY + safeH - glowSize/2, glowSize, glowSize, sparkGlow, true);
    renderer.drawRect(safeX + safeW - sparkSize/2, safeY + safeH - sparkSize/2, sparkSize, sparkSize, sparkColor, true);

    // === Layer 7: Target zone indicator ===
    if (state.wall.currentPhase < ShrinkingWall::MAX_PHASES) {
        uint8_t targetAlpha = static_cast<uint8_t>(50 + 40 * std::sin(state.wall.animTimer * 2.0f));
        SDL_Color targetColor = {255, 100, 200, targetAlpha};
        renderer.drawRectOutline(ox + state.wall.targetLeft, oy + state.wall.targetTop,
                                state.wall.targetRight - state.wall.targetLeft,
                                state.wall.targetBottom - state.wall.targetTop,
                                targetColor, 2.0f);
    }

    // === Storm warning text ===
    float warningPulse = 0.5f + 0.5f * std::sin(state.wall.animTimer * 4.0f);
    SDL_Color warningColor = {255, 150, 255, static_cast<uint8_t>(180 + 75 * warningPulse)};
    renderer.drawText("STORM", WINDOW_WIDTH / 2.0f, oy + state.stage.height + 15,
                     renderer.getFontSmall(), warningColor, TextAlign::Center);
}

void BattleManager::renderHUD(const BattleState& state) {
    auto& renderer = Renderer::instance();

    // === RETRO ARCADE STYLE HUD ===

    // Timer at top center with retro box
    int timeLeft = static_cast<int>(state.maxMatchTime - state.matchTimer);
    if (timeLeft < 0) timeLeft = 0;
    int minutes = timeLeft / 60;
    int seconds = timeLeft % 60;

    char timerStr[16];
    snprintf(timerStr, sizeof(timerStr), "%d:%02d", minutes, seconds);

    // Timer box
    float timerBoxW = 80;
    float timerBoxH = 28;
    float timerBoxX = WINDOW_WIDTH / 2.0f - timerBoxW / 2.0f;
    float timerBoxY = 6;

    // Chunky retro timer box
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

    // Health bars - arcade style horizontal bars at top
    float hudY = 45;
    float barWidth = 140;
    float barHeight = 16;
    float barSpacing = 24;
    float nameHeight = 14;

    float totalWidth = state.bots.size() * (barWidth + barSpacing) - barSpacing;
    float startX = (WINDOW_WIDTH - totalWidth) / 2.0f;

    for (size_t i = 0; i < state.bots.size(); ++i) {
        const auto& bot = state.bots[i];
        float x = startX + i * (barWidth + barSpacing);
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

        // Health bar with segments for retro look
        float healthRatio = clamp(bot.health / bot.maxHealth, 0.0f, 1.0f);
        float healthW = barWidth * healthRatio;

        // Dark background
        renderer.drawRect(x, barY, barWidth, barHeight, darkColor, true);

        // Filled health with gradient effect
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

        // Segment lines for retro look
        SDL_Color segmentColor = {0, 0, 0, 80};
        int segments = 10;
        float segmentWidth = barWidth / segments;
        for (int s = 1; s < segments; ++s) {
            renderer.drawLine(x + s * segmentWidth, barY, x + s * segmentWidth, barY + barHeight, segmentColor, 1.0f);
        }

        // HP percentage text
        char hpStr[16];
        int hpPercent = static_cast<int>(healthRatio * 100);
        snprintf(hpStr, sizeof(hpStr), "%d%%", hpPercent);
        renderer.drawText(hpStr, x + barWidth / 2, barY + 3,
                         renderer.getFontSmall(), {255, 255, 255, 200}, TextAlign::Center);

        // Escape progress if grabbed
        if (bot.grabState == GrabState::Grabbed) {
            float escapeY = barY + barHeight + 3;
            SDL_Color escapeColor = {255, 200, 50, 255};
            SDL_Color escapeBg = {50, 40, 20, 255};
            renderer.drawRect(x, escapeY, barWidth, 6, escapeBg, true);
            renderer.drawRect(x, escapeY, barWidth * bot.grabEscapeProgress, 6, escapeColor, true);
        }

        // === ABILITY STATUS INDICATORS (CIRCLES, BIGGER, ALWAYS VISIBLE) ===
        float abilityY = barY + barHeight + 14;
        float iconRadius = 10.0f;  // Bigger circles
        float iconSpacing = 26.0f;

        // Special ability cooldown
        const auto& special = ComponentRegistry::instance().getSpecial(bot.specialIndex);
        float cooldownRatio = bot.specialCooldown / special.cooldown;

        SDL_Color emptyCircle = {50, 50, 60, 180};
        SDL_Color abilityReady = {100, 255, 100, 255};
        SDL_Color abilityOnCooldown = {80, 80, 90, 200};

        // Draw special ability circle (always visible)
        float specialX = x + iconRadius;
        renderer.drawCircle(specialX, abilityY, iconRadius, emptyCircle, true);

        if (bot.specialActiveTimer > 0) {
            // Active - show yellow pulsing filled circle
            float pulse = 0.5f + 0.5f * std::sin(state.matchTimer * 10.0f);
            SDL_Color activeColor = {255, static_cast<Uint8>(180 + 75 * pulse), 50, 255};
            renderer.drawCircle(specialX, abilityY, iconRadius - 1, activeColor, true);
        } else if (bot.specialCooldown <= 0) {
            // Ready - green filled circle
            renderer.drawCircle(specialX, abilityY, iconRadius - 1, abilityReady, true);
        } else {
            // On cooldown - partial fill from bottom up
            renderer.drawCircle(specialX, abilityY, iconRadius - 1, abilityOnCooldown, true);
        }
        renderer.drawCircleOutline(specialX, abilityY, iconRadius, {100, 100, 120, 255}, 2.0f);

        // Powerup indicators - 3 fixed circles that show empty when no powerup
        float powerup1X = x + iconSpacing + iconRadius;       // Speed boost slot
        float powerup2X = x + iconSpacing * 2 + iconRadius;   // Damage boost slot
        float powerup3X = x + iconSpacing * 3 + iconRadius;   // Shield/Ability slot

        // Speed boost circle (always visible)
        renderer.drawCircle(powerup1X, abilityY, iconRadius, emptyCircle, true);
        if (bot.speedBoostTimer > 0) {
            SDL_Color speedColor = {255, 255, 80, 255};
            renderer.drawCircle(powerup1X, abilityY, iconRadius - 1, speedColor, true);
        }
        renderer.drawCircleOutline(powerup1X, abilityY, iconRadius, {100, 100, 120, 255}, 2.0f);

        // Damage boost circle (always visible)
        renderer.drawCircle(powerup2X, abilityY, iconRadius, emptyCircle, true);
        if (bot.damageBoostTimer > 0) {
            SDL_Color dmgColor = {255, 80, 80, 255};
            renderer.drawCircle(powerup2X, abilityY, iconRadius - 1, dmgColor, true);
        }
        renderer.drawCircleOutline(powerup2X, abilityY, iconRadius, {100, 100, 120, 255}, 2.0f);

        // Active ability/shield circle (always visible)
        renderer.drawCircle(powerup3X, abilityY, iconRadius, emptyCircle, true);
        if (bot.shieldActive) {
            SDL_Color shieldColor = {100, 180, 255, 255};
            renderer.drawCircle(powerup3X, abilityY, iconRadius - 1, shieldColor, true);
        } else if (bot.boostActive) {
            SDL_Color boostColor = {80, 200, 255, 255};
            renderer.drawCircle(powerup3X, abilityY, iconRadius - 1, boostColor, true);
        } else if (bot.berserkActive) {
            SDL_Color berserkColor = {255, 50, 50, 255};
            renderer.drawCircle(powerup3X, abilityY, iconRadius - 1, berserkColor, true);
        } else if (bot.anchorActive) {
            SDL_Color anchorColor = {200, 200, 200, 255};
            renderer.drawCircle(powerup3X, abilityY, iconRadius - 1, anchorColor, true);
        } else if (bot.overdriveActive) {
            SDL_Color overdriveColor = {255, 200, 50, 255};
            renderer.drawCircle(powerup3X, abilityY, iconRadius - 1, overdriveColor, true);
        }
        renderer.drawCircleOutline(powerup3X, abilityY, iconRadius, {100, 100, 120, 255}, 2.0f);
    }

    // === CORNER SCORE BOXES - Retro arcade style ===
    float boxWidth = 64;
    float boxHeight = 48;
    float cornerPad = 8;

    // Corner positions: TL, TR, BL, BR
    float cornerX[4] = {cornerPad, WINDOW_WIDTH - boxWidth - cornerPad,
                        cornerPad, WINDOW_WIDTH - boxWidth - cornerPad};
    float cornerY[4] = {90, 90,
                        WINDOW_HEIGHT - boxHeight - cornerPad, WINDOW_HEIGHT - boxHeight - cornerPad};

    for (size_t i = 0; i < state.bots.size() && i < 4; ++i) {
        const auto& bot = state.bots[i];
        const auto& stats = state.botStats.at(bot.playerIndex);
        SDL_Color playerColor = Renderer::getPlayerColor(bot.colorIndex);

        if (!bot.isAlive) {
            playerColor.r = playerColor.r / 3;
            playerColor.g = playerColor.g / 3;
            playerColor.b = playerColor.b / 3;
        }

        float bx = cornerX[i];
        float by = cornerY[i];

        // Retro score box with beveled edges
        SDL_Color boxBg = {15, 15, 25, 240};
        SDL_Color boxBorder = playerColor;
        SDL_Color boxDark = {
            static_cast<Uint8>(playerColor.r * 0.4f),
            static_cast<Uint8>(playerColor.g * 0.4f),
            static_cast<Uint8>(playerColor.b * 0.4f),
            255
        };

        // Main box
        renderer.drawRect(bx, by, boxWidth, boxHeight, boxBg, true);

        // Colored top bar
        renderer.drawRect(bx, by, boxWidth, 6, playerColor, true);

        // Beveled border
        renderer.drawRect(bx, by, boxWidth, 2, boxBorder, true);  // Top
        renderer.drawRect(bx, by + boxHeight - 2, boxWidth, 2, boxDark, true);  // Bottom
        renderer.drawRect(bx, by, 2, boxHeight, boxBorder, true);  // Left
        renderer.drawRect(bx + boxWidth - 2, by, 2, boxHeight, boxDark, true);  // Right

        // Player number
        char pNumStr[8];
        snprintf(pNumStr, sizeof(pNumStr), "P%zu", i + 1);
        renderer.drawText(pNumStr, bx + boxWidth / 2, by + 10,
                         renderer.getFontSmall(), {200, 200, 200, 255}, TextAlign::Center);

        // KO count - big and bold
        std::string koStr = std::to_string(stats.kills);
        SDL_Color koColor = stats.kills > 0 ? SDL_Color{100, 255, 100, 255} : SDL_Color{150, 150, 150, 255};
        renderer.drawText(koStr, bx + boxWidth / 2, by + 22,
                         renderer.getFontMedium(), koColor, TextAlign::Center);

        // "KO" label
        renderer.drawText("KO", bx + boxWidth / 2, by + 38,
                         renderer.getFontSmall(), {120, 120, 140, 255}, TextAlign::Center);

        // Kill popup (+1) animation
        for (const auto& popup : state.killPopups) {
            if (popup.playerIndex == bot.playerIndex) {
                uint8_t alpha = static_cast<uint8_t>(popup.getAlpha() * 255);
                SDL_Color popupColor = {100, 255, 100, alpha};
                float popupY = by + 18 - (1.0f - popup.timer) * 30;
                renderer.drawText("+1", bx + boxWidth + 8, popupY,
                                 renderer.getFontSmall(), popupColor, TextAlign::Left);
            }
        }
    }

    // Wall warnings
    if (state.wall.active) {
        float timeUntilShrink = ShrinkingWall::PHASE_DURATION - state.wall.phaseTimer;
        if (timeUntilShrink > 0 && timeUntilShrink <= 5.0f) {
            float pulse = 0.5f + 0.5f * std::sin(state.matchTimer * 8.0f);
            uint8_t alpha = static_cast<uint8_t>(180 + 75 * pulse);
            SDL_Color warningColor = {255, 80, 255, alpha};
            renderer.drawText("! WALL CLOSING !", WINDOW_WIDTH / 2.0f, WINDOW_HEIGHT - 60,
                             renderer.getFontSmall(), warningColor, TextAlign::Center);
        }
    } else if (state.matchTimer >= ShrinkingWall::INITIAL_DELAY - 5.0f &&
               state.matchTimer < ShrinkingWall::INITIAL_DELAY) {
        float pulse = 0.5f + 0.5f * std::sin(state.matchTimer * 6.0f);
        uint8_t alpha = static_cast<uint8_t>(150 + 100 * pulse);
        SDL_Color warningColor = {255, 200, 80, alpha};
        int countdown = static_cast<int>(ShrinkingWall::INITIAL_DELAY - state.matchTimer) + 1;
        std::string warning = "STORM IN " + std::to_string(countdown);
        renderer.drawText(warning, WINDOW_WIDTH / 2.0f, WINDOW_HEIGHT - 60,
                         renderer.getFontSmall(), warningColor, TextAlign::Center);
    }
}

void BattleManager::renderGameOver(const BattleState& state) {
    auto& renderer = Renderer::instance();

    // === RETRO ARCADE GAME OVER SCREEN ===

    // Scanline overlay effect
    SDL_Color overlay = {0, 0, 10, 220};
    renderer.drawRect(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, overlay, true);

    // Scanlines for CRT effect
    SDL_Color scanline = {0, 0, 0, 30};
    for (int y = 0; y < WINDOW_HEIGHT; y += 4) {
        renderer.drawRect(0, y, WINDOW_WIDTH, 2, scanline, true);
    }

    // Title banner
    float bannerY = 30;
    float bannerH = 50;
    SDL_Color bannerBg = {20, 20, 40, 255};
    SDL_Color bannerBorder = {255, 200, 50, 255};
    renderer.drawRect(0, bannerY, WINDOW_WIDTH, bannerH, bannerBg, true);
    renderer.drawRect(0, bannerY, WINDOW_WIDTH, 4, bannerBorder, true);
    renderer.drawRect(0, bannerY + bannerH - 4, WINDOW_WIDTH, 4, bannerBorder, true);

    // Title text
    std::string titleText = (state.result == BattleResult::Draw) ? "DRAW!" : "GAME!";
    SDL_Color titleColor = {255, 220, 80, 255};
    renderer.drawText(titleText, WINDOW_WIDTH / 2.0f, bannerY + 14,
                     renderer.getFontLarge(), titleColor, TextAlign::Center);

    // Calculate compact panel layout
    int numPlayers = static_cast<int>(state.bots.size());
    float panelWidth = 150.0f;
    float panelHeight = 280.0f;
    float panelSpacing = 20.0f;
    float totalWidth = numPlayers * panelWidth + (numPlayers - 1) * panelSpacing;
    float startX = (WINDOW_WIDTH - totalWidth) / 2.0f;
    float panelY = 100.0f;

    // Sort players by kills for placement
    std::vector<int> placements(numPlayers);
    for (int i = 0; i < numPlayers; ++i) placements[i] = i;
    std::sort(placements.begin(), placements.end(), [&state](int a, int b) {
        const auto& statsA = state.botStats.at(state.bots[a].playerIndex);
        const auto& statsB = state.botStats.at(state.bots[b].playerIndex);
        if (statsA.kills != statsB.kills) return statsA.kills > statsB.kills;
        return statsA.damageDealt > statsB.damageDealt;
    });

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
        SDL_Color darkColor = {
            static_cast<Uint8>(playerColor.r * 0.3f),
            static_cast<Uint8>(playerColor.g * 0.3f),
            static_cast<Uint8>(playerColor.b * 0.3f),
            255
        };

        // Panel with retro bevel
        SDL_Color panelBg = isWinner ? SDL_Color{40, 35, 15, 255} : SDL_Color{25, 25, 35, 255};
        renderer.drawRect(panelX, panelY, panelWidth, panelHeight, panelBg, true);

        // Beveled border
        SDL_Color light = isWinner ? SDL_Color{255, 200, 50, 255} : SDL_Color{80, 80, 100, 255};
        SDL_Color dark = isWinner ? SDL_Color{150, 120, 30, 255} : SDL_Color{40, 40, 60, 255};
        renderer.drawRect(panelX, panelY, panelWidth, 3, light, true);  // Top
        renderer.drawRect(panelX, panelY, 3, panelHeight, light, true);  // Left
        renderer.drawRect(panelX, panelY + panelHeight - 3, panelWidth, 3, dark, true);  // Bottom
        renderer.drawRect(panelX + panelWidth - 3, panelY, 3, panelHeight, dark, true);  // Right

        // Colored header bar
        float headerH = 24;
        renderer.drawRect(panelX + 3, panelY + 3, panelWidth - 6, headerH, playerColor, true);

        float contentY = panelY + 8;

        // Winner/Placement in header
        if (isWinner) {
            renderer.drawText("WINNER", panelX + panelWidth / 2, contentY,
                             renderer.getFontSmall(), {0, 0, 0, 255}, TextAlign::Center);
        } else {
            char placeStr[8];
            snprintf(placeStr, sizeof(placeStr), "#%d", ranks[i]);
            renderer.drawText(placeStr, panelX + panelWidth / 2, contentY,
                             renderer.getFontSmall(), {0, 0, 0, 255}, TextAlign::Center);
        }
        contentY += headerH + 8;

        // Player name
        renderer.drawText(bot.displayName, panelX + panelWidth / 2, contentY,
                         renderer.getFontSmall(), playerColor, TextAlign::Center);
        contentY += 20;

        // Bot preview (using frame shape)
        float previewY = contentY + 30;
        SDL_Color previewColor = bot.isAlive ? playerColor : SDL_Color{60, 60, 60, 255};
        renderer.drawRect(panelX + panelWidth / 2 - 20, previewY - 20, 40, 40, previewColor, true);
        renderer.drawRect(panelX + panelWidth / 2 - 16, previewY - 16, 32, 32, darkColor, true);
        if (!bot.isAlive) {
            renderer.drawText("X", panelX + panelWidth / 2, previewY - 6,
                             renderer.getFontMedium(), {255, 60, 60, 255}, TextAlign::Center);
        }
        contentY = previewY + 30;

        // Stats with retro styling
        float statX = panelX + 10;
        float statValX = panelX + panelWidth - 10;
        float statSpacing = 28;

        SDL_Color labelColor = {120, 120, 140, 255};
        SDL_Color valColor = {220, 220, 220, 255};

        // KOs
        renderer.drawText("KO", statX, contentY, renderer.getFontSmall(), labelColor, TextAlign::Left);
        SDL_Color koColor = stats.kills > 0 ? SDL_Color{80, 255, 80, 255} : valColor;
        renderer.drawText(std::to_string(stats.kills), statValX, contentY,
                         renderer.getFontSmall(), koColor, TextAlign::Right);
        contentY += statSpacing;

        // Deaths
        renderer.drawText("FALL", statX, contentY, renderer.getFontSmall(), labelColor, TextAlign::Left);
        SDL_Color deathColor = stats.deaths > 0 ? SDL_Color{255, 80, 80, 255} : valColor;
        renderer.drawText(std::to_string(stats.deaths), statValX, contentY,
                         renderer.getFontSmall(), deathColor, TextAlign::Right);
        contentY += statSpacing;

        // Damage dealt
        renderer.drawText("DMG", statX, contentY, renderer.getFontSmall(), labelColor, TextAlign::Left);
        char dmgStr[16];
        snprintf(dmgStr, sizeof(dmgStr), "%.0f", stats.damageDealt);
        renderer.drawText(dmgStr, statValX, contentY, renderer.getFontSmall(), valColor, TextAlign::Right);
        contentY += statSpacing;

        // Damage taken
        renderer.drawText("HIT", statX, contentY, renderer.getFontSmall(), labelColor, TextAlign::Left);
        char hitStr[16];
        snprintf(hitStr, sizeof(hitStr), "%.0f", stats.damageTaken);
        SDL_Color hitColor = {255, 180, 80, 255};
        renderer.drawText(hitStr, statValX, contentY, renderer.getFontSmall(), hitColor, TextAlign::Right);
    }

    // Match time in retro box
    float timeBoxY = panelY + panelHeight + 15;
    float timeBoxW = 120;
    float timeBoxH = 24;
    float timeBoxX = WINDOW_WIDTH / 2.0f - timeBoxW / 2.0f;

    renderer.drawRect(timeBoxX, timeBoxY, timeBoxW, timeBoxH, {20, 20, 30, 255}, true);
    renderer.drawRectOutline(timeBoxX, timeBoxY, timeBoxW, timeBoxH, {80, 80, 100, 255}, 2.0f);

    int totalSeconds = static_cast<int>(state.matchTimer);
    char timeStr[32];
    snprintf(timeStr, sizeof(timeStr), "TIME %d:%02d", totalSeconds / 60, totalSeconds % 60);
    renderer.drawText(timeStr, WINDOW_WIDTH / 2.0f, timeBoxY + 6,
                     renderer.getFontSmall(), {150, 255, 150, 255}, TextAlign::Center);

    // Confirmation indicators
    float confirmY = timeBoxY + timeBoxH + 20;
    float confirmBoxSize = 28;
    float confirmSpacing = 40;
    float confirmTotalW = numPlayers * confirmSpacing;
    float confirmStartX = WINDOW_WIDTH / 2.0f - confirmTotalW / 2.0f + (confirmSpacing - confirmBoxSize) / 2.0f;

    for (int i = 0; i < numPlayers; ++i) {
        float ix = confirmStartX + i * confirmSpacing;
        SDL_Color playerColor = Renderer::getPlayerColor(state.bots[i].colorIndex);

        if (state.playersConfirmed[i]) {
            renderer.drawRect(ix, confirmY, confirmBoxSize, confirmBoxSize, playerColor, true);
            renderer.drawText("OK", ix + confirmBoxSize / 2, confirmY + 8,
                             renderer.getFontSmall(), {0, 0, 0, 255}, TextAlign::Center);
        } else {
            SDL_Color dimColor = {
                static_cast<Uint8>(playerColor.r / 2),
                static_cast<Uint8>(playerColor.g / 2),
                static_cast<Uint8>(playerColor.b / 2), 180};
            renderer.drawRectOutline(ix, confirmY, confirmBoxSize, confirmBoxSize, dimColor, 2.0f);
        }
    }

    // Bottom prompt with pulsing effect
    float pulse = 0.6f + 0.4f * std::sin(state.gameOverTimer * 4.0f);
    uint8_t promptAlpha = static_cast<uint8_t>(180 * pulse + 75);
    SDL_Color promptColor = {200, 200, 220, promptAlpha};
    renderer.drawText("PRESS START", WINDOW_WIDTH / 2.0f, WINDOW_HEIGHT - 30,
                     renderer.getFontSmall(), promptColor, TextAlign::Center);
}

} // namespace ScrapHeap
