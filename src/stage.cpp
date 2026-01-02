#include "stage.h"

namespace ScrapHeap {

StageRegistry& StageRegistry::instance() {
    static StageRegistry registry;
    return registry;
}

void StageRegistry::initialize() {
    stages.clear();

    // The Pit - Training stage, no hazards
    {
        StageDef stage;
        stage.name = "The Pit";
        stage.width = 500.0f;
        stage.height = 500.0f;
        stage.backgroundColor = {40, 40, 50, 255};
        stage.wallColor = {100, 100, 110, 255};
        stage.description = "Training stage, pure fundamentals";

        // Spawn points for 4 players (corners)
        stage.spawnPoints = {
            {100.0f, 100.0f},
            {400.0f, 100.0f},
            {100.0f, 400.0f},
            {400.0f, 400.0f}
        };

        // Powerup spawn locations
        stage.powerupSpawns = {
            {250.0f, 250.0f},  // Center
            {150.0f, 250.0f},  // Left
            {350.0f, 250.0f}   // Right
        };

        stages.push_back(stage);
    }

    // Hazard Zone - Corner flames
    {
        StageDef stage;
        stage.name = "Hazard Zone";
        stage.width = 550.0f;
        stage.height = 550.0f;
        stage.backgroundColor = {50, 35, 35, 255};
        stage.wallColor = {120, 80, 80, 255};
        stage.description = "Corner flames activate on timer";

        // Corner flame hazards
        float flameSize = 60.0f;
        stage.hazards = {
            {HazardType::Flames, 0.0f, 0.0f, flameSize, flameSize, 0.0f, 10.0f, 0.0f, 2.0f, 3.0f, true, false},
            {HazardType::Flames, stage.width - flameSize, 0.0f, flameSize, flameSize, 0.0f, 10.0f, 1.5f, 2.0f, 3.0f, false, false},
            {HazardType::Flames, 0.0f, stage.height - flameSize, flameSize, flameSize, 0.0f, 10.0f, 3.0f, 2.0f, 3.0f, false, false},
            {HazardType::Flames, stage.width - flameSize, stage.height - flameSize, flameSize, flameSize, 0.0f, 10.0f, 4.5f, 2.0f, 3.0f, false, false}
        };

        stage.spawnPoints = {
            {150.0f, 150.0f},
            {400.0f, 150.0f},
            {150.0f, 400.0f},
            {400.0f, 400.0f}
        };

        stage.powerupSpawns = {
            {275.0f, 275.0f},
            {275.0f, 150.0f},
            {275.0f, 400.0f}
        };

        stages.push_back(stage);
    }

    // Steel Cage - Electrified walls
    {
        StageDef stage;
        stage.name = "Steel Cage";
        stage.width = 450.0f;
        stage.height = 450.0f;
        stage.backgroundColor = {35, 40, 50, 255};
        stage.wallColor = {80, 100, 150, 255};
        stage.description = "Wall contact deals 5 damage";

        // The walls themselves are electrified, handled specially
        // Add thin hazard strips along walls
        float wallThickness = 10.0f;
        stage.hazards = {
            {HazardType::ElectrifiedWall, 0.0f, 0.0f, stage.width, wallThickness, 0.0f, 5.0f, 0.0f, 0.0f, 0.0f, true, false},
            {HazardType::ElectrifiedWall, 0.0f, stage.height - wallThickness, stage.width, wallThickness, 0.0f, 5.0f, 0.0f, 0.0f, 0.0f, true, false},
            {HazardType::ElectrifiedWall, 0.0f, 0.0f, wallThickness, stage.height, 0.0f, 5.0f, 0.0f, 0.0f, 0.0f, true, false},
            {HazardType::ElectrifiedWall, stage.width - wallThickness, 0.0f, wallThickness, stage.height, 0.0f, 5.0f, 0.0f, 0.0f, 0.0f, true, false}
        };

        stage.spawnPoints = {
            {120.0f, 120.0f},
            {330.0f, 120.0f},
            {120.0f, 330.0f},
            {330.0f, 330.0f}
        };

        stage.powerupSpawns = {
            {225.0f, 225.0f},
            {225.0f, 120.0f},
            {225.0f, 330.0f}
        };

        stages.push_back(stage);
    }

    // Lava Arena - Center pit
    {
        StageDef stage;
        stage.name = "Lava Arena";
        stage.width = 500.0f;
        stage.height = 500.0f;
        stage.backgroundColor = {60, 40, 30, 255};
        stage.wallColor = {130, 90, 60, 255};
        stage.description = "Center pit = instant death";

        // Center pit
        Hazard pit;
        pit.type = HazardType::Pit;
        pit.x = stage.width / 2.0f;
        pit.y = stage.height / 2.0f;
        pit.radius = 50.0f;
        pit.isCircular = true;
        pit.isActive = true;
        stage.hazards.push_back(pit);

        stage.spawnPoints = {
            {80.0f, 80.0f},
            {420.0f, 80.0f},
            {80.0f, 420.0f},
            {420.0f, 420.0f}
        };

        stage.powerupSpawns = {
            {100.0f, 250.0f},
            {400.0f, 250.0f},
            {250.0f, 100.0f},
            {250.0f, 400.0f}
        };

        stages.push_back(stage);
    }
}

const StageDef& StageRegistry::getStage(int index) const {
    if (index < 0 || index >= static_cast<int>(stages.size())) {
        return stages[0];
    }
    return stages[index];
}

std::vector<std::string> StageRegistry::getStageNames() const {
    std::vector<std::string> names;
    for (const auto& stage : stages) {
        names.push_back(stage.name);
    }
    return names;
}

std::vector<Vec2> getSpawnPositions(const StageDef& stage, int playerCount) {
    std::vector<Vec2> positions;

    // Use defined spawn points if available
    for (int i = 0; i < playerCount && i < static_cast<int>(stage.spawnPoints.size()); ++i) {
        positions.push_back(Vec2(stage.spawnPoints[i].x, stage.spawnPoints[i].y));
    }

    // Generate remaining positions if needed
    while (static_cast<int>(positions.size()) < playerCount) {
        float angle = (positions.size() / static_cast<float>(playerCount)) * 2.0f * PI;
        float radius = std::min(stage.width, stage.height) * 0.35f;
        float x = stage.width / 2.0f + std::cos(angle) * radius;
        float y = stage.height / 2.0f + std::sin(angle) * radius;
        positions.push_back(Vec2(x, y));
    }

    return positions;
}

bool isInHazard(const StageDef& stage, float x, float y, float radius, HazardType* outType) {
    for (const auto& hazard : stage.hazards) {
        if (!hazard.isActive) continue;

        bool inHazard = false;
        if (hazard.isCircular) {
            float dist = distance(x, y, hazard.x, hazard.y);
            inHazard = dist < (hazard.radius + radius);
        } else {
            // AABB check with circle
            float closestX = clamp(x, hazard.x, hazard.x + hazard.width);
            float closestY = clamp(y, hazard.y, hazard.y + hazard.height);
            float dist = distance(x, y, closestX, closestY);
            inHazard = dist < radius;
        }

        if (inHazard) {
            if (outType) *outType = hazard.type;
            return true;
        }
    }
    return false;
}

bool isInPit(const StageDef& stage, float x, float y) {
    for (const auto& hazard : stage.hazards) {
        if (hazard.type != HazardType::Pit) continue;

        if (hazard.isCircular) {
            float dist = distance(x, y, hazard.x, hazard.y);
            if (dist < hazard.radius * 0.5f) return true;  // Must be mostly in pit
        } else {
            if (x > hazard.x && x < hazard.x + hazard.width &&
                y > hazard.y && y < hazard.y + hazard.height) {
                return true;
            }
        }
    }
    return false;
}

} // namespace ScrapHeap
