#include "arena_mode.h"
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

void ArenaMode::initialize(const PlayerSlot* slots, int stageIndex) {
    stageIndex_ = stageIndex;
    stage_ = StageRegistry::instance().getStage(stageIndex);

    // Create bots from player slots
    std::vector<int> joinedSlots;
    for (int i = 0; i < 4; ++i) {
        if (slots[i].state != PlayerSlotState::Empty) {
            joinedSlots.push_back(i);
        }
    }

    auto spawnPositions = getSpawnPositions(stage_, static_cast<int>(joinedSlots.size()));

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

        float spawnAngle = std::atan2(
            stage_.height / 2.0f - spawnPositions[i].y,
            stage_.width / 2.0f - spawnPositions[i].x
        );
        bot.reset(spawnPositions[i].x, spawnPositions[i].y, spawnAngle);

        bots_.push_back(bot);
        botStats_[bot.playerIndex] = BotBattleStats();

        // Record component usage
        const auto& frame = ComponentRegistry::instance().getFrame(slot.frameIndex);
        const auto& engine = ComponentRegistry::instance().getEngine(slot.engineIndex);
        const auto& weapon = ComponentRegistry::instance().getWeapon(slot.weaponIndex);
        const auto& special = ComponentRegistry::instance().getSpecial(slot.specialIndex);

        DataManager::instance().recordComponentUsage(
            bot.displayName, frame.name, engine.name, weapon.name, special.name);
    }

    // Create powerups
    powerups_ = PowerupManager::instance().createPowerupsForStage(stageIndex);

    // Initialize storm
    storm_.reset(stage_.width, stage_.height);

    // Calculate camera offset
    cameraOffsetX_ = (WINDOW_WIDTH - stage_.width) / 2.0f;
    cameraOffsetY_ = (WINDOW_HEIGHT - stage_.height) / 2.0f + 30.0f;

    matchTimer_ = 0.0f;
    maxMatchTime_ = MATCH_DURATION;
    result_ = ArenaResult::InProgress;
}

void ArenaMode::update(GameContext& ctx, float dt) {
    if (paused_) return;

    if (result_ != ArenaResult::InProgress) {
        gameOverTimer_ += dt;
        handleWinScreenInput(ctx);
        if (resultsConfirmed_) {
            recordResults(ctx);
            ctx.changeState(GameState::MainMenu);
        }
        return;
    }

    matchTimer_ += dt;

    // Update input for all bots
    auto& input = InputManager::instance();
    for (auto& bot : bots_) {
        if (!bot.isAlive) continue;

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

    updateBots(dt);

    // Process weapons and combat
    Combat::processWeapons(bots_, stage_, combatEvents_, dt);
    Combat::processGrabs(bots_, combatEvents_, dt);
    Combat::processSpecials(bots_, combatEvents_, dt);
    Combat::processHazards(bots_, stage_, combatEvents_, dt);

    // Handle special activations
    for (auto& bot : bots_) {
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
                smokeClouds_.push_back(cloud);
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
                mines_.push_back(mine);
                bot.specialCooldown = special.cooldown;
            } else {
                Combat::activateSpecial(bot, bots_, combatEvents_);
            }
        }

        // Use held powerup
        if (bot.inputPowerupPressed && bot.heldPowerup >= 0) {
            PowerupManager::instance().useHeldPowerup(bot);
        }
    }

    // Update powerups
    PowerupManager::instance().update(powerups_, dt);

    // Check powerup collection for each bot
    for (auto& bot : bots_) {
        if (bot.isAlive) {
            PowerupManager::instance().checkCollection(powerups_, bot);
        }
    }

    // Update mines and smoke
    updateMines(dt);
    updateSmokeClouds(dt);
    updateCombatEvents(dt);
    BattleHUD::updateKillPopups(killPopups_, dt);

    // Update storm
    StormManager::update(storm_, stage_, matchTimer_, dt);
    StormManager::applyDamage(storm_, bots_, combatEvents_, dt);

    // Check for deaths and game over
    for (auto& bot : bots_) {
        if (bot.isAlive && bot.health <= 0) {
            bot.health = 0;
            bot.isAlive = false;
            botStats_[bot.playerIndex].deaths++;
        }
    }

    checkGameOver();
}

void ArenaMode::handleInput(GameContext& ctx) {
    auto& input = InputManager::instance();

    // Check for pause
    for (int i = 0; i < 4; ++i) {
        const ControllerState* controller = input.getControllerForPlayer(i);
        if (controller && controller->buttonStartPressed()) {
            paused_ = !paused_;
            return;
        }
    }
    if (input.getKeyboard().escapePressed()) {
        paused_ = !paused_;
    }
}

void ArenaMode::render() {
    renderArena();

    BattleHUD::render(bots_, botStats_, matchTimer_, maxMatchTime_);
    BattleHUD::renderKillPopups(killPopups_, bots_);

    if (result_ != ArenaResult::InProgress) {
        renderGameOver();
    }
}

GameModeResult ArenaMode::getResult() const {
    return (result_ == ArenaResult::InProgress) ?
           GameModeResult::InProgress : GameModeResult::Finished;
}

std::string ArenaMode::getWinnerName() const {
    if (result_ == ArenaResult::Draw) return "DRAW";
    if (winnerIndex_ >= 0 && winnerIndex_ < static_cast<int>(bots_.size())) {
        return bots_[winnerIndex_].displayName;
    }
    return "UNKNOWN";
}

void ArenaMode::updateBots(float dt) {
    for (auto& bot : bots_) {
        if (!bot.isAlive) continue;

        Physics::updateBotMovement(bot, dt);
        Physics::applyFriction(bot, dt);
        Physics::resolveWallCollisions(bot, stage_);
    }

    // Bot-bot collision
    for (size_t i = 0; i < bots_.size(); ++i) {
        for (size_t j = i + 1; j < bots_.size(); ++j) {
            if (!bots_[i].isAlive || !bots_[j].isAlive) continue;
            auto collision = Physics::checkBotCollision(bots_[i], bots_[j]);
            if (collision.collided) {
                Physics::resolveBotCollision(bots_[i], bots_[j], collision);
            }
        }
    }
}

void ArenaMode::updateCombatEvents(float dt) {
    for (auto& event : combatEvents_) {
        event.timer -= dt;
    }
    combatEvents_.erase(
        std::remove_if(combatEvents_.begin(), combatEvents_.end(),
                      [](const CombatEvent& e) { return e.timer <= 0; }),
        combatEvents_.end());
}

void ArenaMode::updateMines(float dt) {
    for (auto& mine : mines_) {
        if (mine.exploded) continue;

        mine.armTimer -= dt;
        if (mine.armTimer <= 0 && !mine.armed) {
            mine.armed = true;
        }

        if (mine.armed) {
            for (auto& bot : bots_) {
                if (!bot.isAlive || bot.playerIndex == mine.ownerIndex) continue;

                float dx = bot.x - mine.x;
                float dy = bot.y - mine.y;
                float dist = std::sqrt(dx * dx + dy * dy);

                if (dist < bot.radius + Mine::RADIUS) {
                    mine.exploded = true;
                    Combat::applyDamage(bot, Mine::DAMAGE, mine.ownerIndex,
                                       bots_, combatEvents_, botStats_, killPopups_);
                    break;
                }
            }
        }
    }

    mines_.erase(
        std::remove_if(mines_.begin(), mines_.end(),
                      [](const Mine& m) { return m.exploded; }),
        mines_.end());
}

void ArenaMode::updateSmokeClouds(float dt) {
    for (auto& cloud : smokeClouds_) {
        cloud.timer -= dt;
    }
    smokeClouds_.erase(
        std::remove_if(smokeClouds_.begin(), smokeClouds_.end(),
                      [](const SmokeCloud& c) { return c.timer <= 0; }),
        smokeClouds_.end());
}

void ArenaMode::checkGameOver() {
    int alive = countAliveBots();

    // Time limit
    if (matchTimer_ >= maxMatchTime_) {
        if (alive > 1) {
            result_ = ArenaResult::Draw;
        } else if (alive == 1) {
            for (size_t i = 0; i < bots_.size(); ++i) {
                if (bots_[i].isAlive) {
                    result_ = ArenaResult::Winner;
                    winnerIndex_ = static_cast<int>(i);
                    break;
                }
            }
        } else {
            result_ = ArenaResult::Draw;
        }
        return;
    }

    // Single survivor
    if (alive == 1) {
        for (size_t i = 0; i < bots_.size(); ++i) {
            if (bots_[i].isAlive) {
                result_ = ArenaResult::Winner;
                winnerIndex_ = static_cast<int>(i);
                break;
            }
        }
    } else if (alive == 0) {
        result_ = ArenaResult::Draw;
    }
}

void ArenaMode::handleWinScreenInput(GameContext& ctx) {
    auto& input = InputManager::instance();

    for (int i = 0; i < 4; ++i) {
        const ControllerState* controller = input.getControllerForPlayer(i);
        if (controller && controller->buttonAPressed()) {
            playersConfirmed_[i] = true;
        }
        if (controller && controller->buttonStartPressed()) {
            resultsConfirmed_ = true;
        }
    }

    if (input.getKeyboard().enterPressed() || input.getKeyboard().spacePressed()) {
        resultsConfirmed_ = true;
    }
}

void ArenaMode::recordResults(GameContext& ctx) {
    auto& data = DataManager::instance();
    data.incrementMatchCount();
    data.addPlayTime(matchTimer_);

    bool isDraw = (result_ == ArenaResult::Draw);

    for (size_t i = 0; i < bots_.size(); ++i) {
        const auto& bot = bots_[i];
        const auto& stats = botStats_.at(bot.playerIndex);
        bool won = !isDraw && static_cast<int>(i) == winnerIndex_;

        data.recordMatchResult(
            bot.displayName, won, isDraw,
            stats.kills, stats.deaths,
            stats.damageDealt, stats.damageTaken,
            matchTimer_
        );
    }
}

int ArenaMode::countAliveBots() const {
    int count = 0;
    for (const auto& bot : bots_) {
        if (bot.isAlive) count++;
    }
    return count;
}

void ArenaMode::renderArena() {
    auto& renderer = Renderer::instance();

    renderer.drawStage(stage_, cameraOffsetX_, cameraOffsetY_);

    for (const auto& powerup : powerups_) {
        renderer.drawPowerup(powerup, cameraOffsetX_, cameraOffsetY_);
    }

    for (const auto& mine : mines_) {
        renderer.drawMine(mine, cameraOffsetX_, cameraOffsetY_);
    }

    for (const auto& bot : bots_) {
        if (bot.grabState == GrabState::Grabbing) {
            for (const auto& other : bots_) {
                if (other.playerIndex == bot.grabbingBot) {
                    renderer.drawGrabTether(bot, other, cameraOffsetX_, cameraOffsetY_);
                    break;
                }
            }
        }
    }

    for (const auto& bot : bots_) {
        if (!bot.isAlive) continue;
        SDL_Color color = Renderer::getPlayerColor(bot.colorIndex);
        renderer.drawBot(bot, color, cameraOffsetX_, cameraOffsetY_);
    }

    for (const auto& event : combatEvents_) {
        renderer.drawCombatEvent(event, cameraOffsetX_, cameraOffsetY_);
    }

    for (const auto& cloud : smokeClouds_) {
        renderer.drawSmokeCloud(cloud, cameraOffsetX_, cameraOffsetY_);
    }

    StormManager::render(storm_, stage_, cameraOffsetX_, cameraOffsetY_);
}

void ArenaMode::renderGameOver() {
    auto& renderer = Renderer::instance();

    // Dim overlay
    SDL_Color overlayColor = {0, 0, 0, 180};
    renderer.drawRect(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, overlayColor, true);

    // Result text
    std::string resultText = (result_ == ArenaResult::Draw) ? "DRAW!" : getWinnerName() + " WINS!";
    SDL_Color resultColor = (result_ == ArenaResult::Draw) ?
        SDL_Color{200, 200, 200, 255} : SDL_Color{255, 220, 50, 255};

    renderer.drawText(resultText, WINDOW_WIDTH / 2.0f, WINDOW_HEIGHT / 2.0f - 30,
                     renderer.getFontLarge(), resultColor, TextAlign::Center);

    // Prompt
    float pulse = 0.6f + 0.4f * std::sin(gameOverTimer_ * 4.0f);
    uint8_t promptAlpha = static_cast<uint8_t>(180 * pulse + 75);
    SDL_Color promptColor = {200, 200, 220, promptAlpha};
    renderer.drawText("PRESS START", WINDOW_WIDTH / 2.0f, WINDOW_HEIGHT - 30,
                     renderer.getFontSmall(), promptColor, TextAlign::Center);
}

// Game mode factory
std::unique_ptr<IGameMode> GameModeFactory::create(ModeType type) {
    switch (type) {
        case ModeType::Arena:
            return std::make_unique<ArenaMode>();
        default:
            return std::make_unique<ArenaMode>();
    }
}

} // namespace ScrapHeap
