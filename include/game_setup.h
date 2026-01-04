#pragma once

#include "input.h"
#include <SDL3/SDL.h>
#include <string>
#include <vector>

namespace ScrapHeap {

// Forward declarations
struct GameContext;

// Calculated bot stats for display
struct BotStats {
    float speed;      // 0.0 - 1.0 normalized
    float damage;     // 0.0 - 1.0 normalized
    float armor;      // 0.0 - 1.0 normalized
    float weight;     // 0.0 - 1.0 normalized
};

// Player slot state
enum class PlayerSlotState {
    Empty,      // No player, waiting for join
    Configuring,// Player joined, selecting options
    Ready       // Player confirmed selections
};

// Configuration option
enum class ConfigOption {
    Tag,
    Engine,
    Frame,
    Weapon,
    Special,
    Color,
    OK,
    COUNT
};

// Player slot in game setup
struct PlayerSlot {
    PlayerSlotState state = PlayerSlotState::Empty;
    int controllerIndex = -1;  // -1 = keyboard player

    // Selections
    int tagIndex = -1;         // -1 = use default "Player N"
    int engineIndex = 0;
    int frameIndex = 0;
    int weaponIndex = 0;
    int specialIndex = 0;
    int colorIndex = 0;

    // Current option being edited
    ConfigOption currentOption = ConfigOption::Tag;

    // Get display name
    std::string getDisplayName(int slotIndex) const;
};

// Game setup screen (character select)
class GameSetupScreen {
public:
    void enter();
    void handleInput(GameContext& ctx);
    void update(float dt);
    void render();

    // Get player slots
    PlayerSlot* getSlots() { return slots; }
    const PlayerSlot* getSlots() const { return slots; }

    // Count ready/joined players
    int countJoinedPlayers() const;
    int countReadyPlayers() const;

    // Get first player to ready (for stage select)
    int getFirstReadyPlayer() const { return firstReadyPlayer; }

    // Reset all slots
    void reset();

private:
    PlayerSlot slots[4];
    int firstReadyPlayer = -1;

    // Get available tags (not selected by others)
    std::vector<int> getAvailableTagIndices(int currentSlot) const;

    // Get available colors (not selected by others)
    std::vector<int> getAvailableColorIndices(int currentSlot) const;

    // Handle input for a single slot
    void handleSlotInput(int slotIndex, const ControllerState* controller,
                        const KeyboardState* keyboard, GameContext& ctx);

    // Render a single slot
    void renderSlot(int slotIndex, float x, float y, float width, float height);

    // Calculate bot stats from components
    BotStats calculateBotStats(const PlayerSlot& slot) const;

    // Render stats display (like reference image)
    void renderStatsDisplay(const BotStats& stats, float x, float y, float width, SDL_Color playerColor);

    // Render compact stats bar (horizontal layout for bottom of garage)
    void renderStatsCompact(const BotStats& stats, float x, float y, float width, SDL_Color playerColor);

    // Render bot preview visualization
    void renderBotPreview(const PlayerSlot& slot, float centerX, float centerY, float size);
};

// Stage select screen
class StageSelectScreen {
public:
    void enter(int selectingPlayer, int playerColorIndex);
    void handleInput(GameContext& ctx);
    void update(float dt);
    void render();

    int getSelectedStage() const { return selection; }

private:
    int selection = 0;
    int selectingPlayer = 0;
    int selectingPlayerColor = 0;
    float previewTimer = 0.0f;
};

} // namespace ScrapHeap
