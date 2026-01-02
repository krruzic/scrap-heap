#pragma once

#include "utils.h"
#include <SDL3/SDL.h>
#include <string>
#include <vector>

namespace ScrapHeap {

// Hazard types
enum class HazardType {
    None,
    Flames,         // Corner flames, deal DPS
    ElectrifiedWall,// Wall contact deals damage
    Pit             // Instant death
};

// Hazard definition
struct Hazard {
    HazardType type = HazardType::None;
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
    float radius = 0.0f;  // For circular hazards
    float damage = 0.0f;
    float activationTimer = 0.0f;  // For timed hazards
    float activeTime = 0.0f;
    float inactiveTime = 0.0f;
    bool isActive = true;
    bool isCircular = false;
};

// Spawn point for powerups
struct SpawnPoint {
    float x = 0.0f;
    float y = 0.0f;
};

// Stage definition
struct StageDef {
    std::string name;
    float width = 500.0f;
    float height = 500.0f;
    std::vector<Hazard> hazards;
    std::vector<SpawnPoint> spawnPoints;      // Player spawn points
    std::vector<SpawnPoint> powerupSpawns;    // Powerup spawn locations
    SDL_Color backgroundColor = {40, 40, 50, 255};
    SDL_Color wallColor = {100, 100, 110, 255};
    std::string description;
};

// Stage registry
class StageRegistry {
public:
    static StageRegistry& instance();

    void initialize();

    const std::vector<StageDef>& getStages() const { return stages; }
    const StageDef& getStage(int index) const;
    int getStageCount() const { return static_cast<int>(stages.size()); }

    std::vector<std::string> getStageNames() const;

private:
    StageRegistry() = default;

    std::vector<StageDef> stages;
};

// Get spawn positions for a given number of players
std::vector<Vec2> getSpawnPositions(const StageDef& stage, int playerCount);

// Check if a point is inside a hazard
bool isInHazard(const StageDef& stage, float x, float y, float radius, HazardType* outType = nullptr);

// Check if a point is inside the pit (for instant death)
bool isInPit(const StageDef& stage, float x, float y);

} // namespace ScrapHeap
