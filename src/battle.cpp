#include "battle.h"
#include "game_context.h"
#include "renderer.h"
#include "physics.h"
#include "combat.h"
#include "powerup.h"
#include "data.h"
#include "components.h"
#include <algorithm>

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

        // Record component usage
        const auto& frame = ComponentRegistry::instance().getFrame(slot.frameIndex);
        const auto& weapon = ComponentRegistry::instance().getWeapon(slot.weaponIndex);
        DataManager::instance().recordComponentUsage(frame.name, weapon.name);
    }

    // Create powerups
    state.powerups = PowerupManager::instance().createPowerupsForStage(stageIndex);

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
        if (state.gameOverTimer >= BattleState::GAME_OVER_DELAY) {
            recordResults(state);
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

    // Update combat events
    updateCombatEvents(state, dt);

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

                    // Damage all bots in explosion radius
                    for (auto& target : state.bots) {
                        if (!target.isAlive) continue;
                        float targetDist = distance(mine.x, mine.y, target.x, target.y);
                        if (targetDist < Mine::EXPLOSION_RADIUS) {
                            Vec2 knockDir(target.x - mine.x, target.y - mine.y);
                            Combat::applyDamage(target, Mine::DAMAGE, 150.0f, knockDir,
                                               nullptr, state.combatEvents);
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

void BattleManager::updateCombatEvents(BattleState& state, float dt) {
    for (auto it = state.combatEvents.begin(); it != state.combatEvents.end(); ) {
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

void BattleManager::recordResults(const BattleState& state) {
    auto& data = DataManager::instance();
    data.incrementMatchCount();

    if (state.result == BattleResult::Winner && state.winnerIndex >= 0) {
        for (size_t i = 0; i < state.bots.size(); ++i) {
            if (static_cast<int>(i) == state.winnerIndex) {
                data.recordWin(state.bots[i].displayName);
            } else {
                data.recordLoss(state.bots[i].displayName);
            }
        }
    }
    // For draws, we don't record wins/losses
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

void BattleManager::renderHUD(const BattleState& state) {
    auto& renderer = Renderer::instance();

    float hudY = 10;
    float barWidth = 150;
    float barHeight = 20;
    float spacing = 20;

    // Health bars for each player
    float totalWidth = state.bots.size() * (barWidth + spacing) - spacing;
    float startX = (WINDOW_WIDTH - totalWidth) / 2.0f;

    for (size_t i = 0; i < state.bots.size(); ++i) {
        const auto& bot = state.bots[i];
        float x = startX + i * (barWidth + spacing);

        SDL_Color playerColor = Renderer::getPlayerColor(bot.colorIndex);
        SDL_Color bgColor = {40, 40, 50, 255};

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
}

void BattleManager::renderGameOver(const BattleState& state) {
    auto& renderer = Renderer::instance();

    // Darken screen
    SDL_Color overlay = {0, 0, 0, 180};
    renderer.drawRect(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, overlay, true);

    // Winner text
    std::string resultText = state.getWinnerName();
    if (state.result == BattleResult::Winner) {
        resultText += " WINS!";
    }

    SDL_Color textColor = {255, 200, 50, 255};
    renderer.drawTextShadow(resultText, WINDOW_WIDTH / 2.0f, WINDOW_HEIGHT / 2.0f - 30,
                           renderer.getFontLarge(), textColor, TextAlign::Center);

    // Countdown
    int countdown = static_cast<int>(BattleState::GAME_OVER_DELAY - state.gameOverTimer) + 1;
    if (countdown > 0) {
        SDL_Color countColor = {180, 180, 180, 255};
        renderer.drawText("Returning to menu in " + std::to_string(countdown) + "...",
                         WINDOW_WIDTH / 2.0f, WINDOW_HEIGHT / 2.0f + 50,
                         renderer.getFontSmall(), countColor, TextAlign::Center);
    }
}

} // namespace ScrapHeap
