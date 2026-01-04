#pragma once

#include "bot.h"
#include "stage.h"
#include "combat.h"
#include <vector>

namespace ScrapHeap {

// Shrinking wall (storm) state
struct Storm {
    // Wall boundaries (safe zone is inside these)
    float left = 0.0f;
    float right = 0.0f;
    float top = 0.0f;
    float bottom = 0.0f;

    // Target boundaries (what we're shrinking towards)
    float targetLeft = 0.0f;
    float targetRight = 0.0f;
    float targetTop = 0.0f;
    float targetBottom = 0.0f;

    // Shrink phases
    int currentPhase = 0;
    float phaseTimer = 0.0f;

    // Wall properties
    bool active = false;
    float damagePerSecond = 5.0f;

    // Animation state for electrical effect
    float animTimer = 0.0f;
    float pulseTimer = 0.0f;
    float arcOffsets[16] = {0};  // Random offsets for lightning arcs

    // Timing constants
    static constexpr float INITIAL_DELAY = 30.0f;       // Wait 30s before wall appears
    static constexpr float PHASE_DURATION = 20.0f;      // Each phase lasts 20s
    static constexpr float FINAL_RADIUS = 60.0f;        // Minimum safe zone radius
    static constexpr int MAX_PHASES = 5;

    // Get current shrink speed (faster in later phases)
    float getShrinkSpeed() const {
        return (0.5f + currentPhase * 0.4f) * 3.0f;
    }

    // Reset storm to stage boundaries
    void reset(float stageWidth, float stageHeight);

    // Check if position is outside safe zone
    bool isOutside(float x, float y) const;

    // Get damage percent per second based on current phase (1%, 3%, 5%)
    float getDamagePercent() const;

    // Get fraction of bot overlapping with storm (0 = safe, 1 = fully in storm)
    float getOverlapFraction(float botX, float botY, float botRadius) const;
};

// Storm manager handles updates and rendering
class StormManager {
public:
    // Update storm state
    static void update(Storm& storm, const StageDef& stage, float matchTimer, float dt);

    // Apply damage to bots outside safe zone, returns player index of storm victim (-1 if none)
    static int applyDamage(Storm& storm, std::vector<Bot>& bots,
                           std::vector<CombatEvent>& events, float dt);

    // Render the storm effect
    static void render(const Storm& storm, const StageDef& stage,
                      float cameraOffsetX, float cameraOffsetY);

private:
    // Helper to draw lightning bolts in a zone
    static void drawStormLightning(float zoneX, float zoneY, float zoneW, float zoneH,
                                   float animTimer);
};

} // namespace ScrapHeap
