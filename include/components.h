#pragma once

#include <string>
#include <vector>
#include <functional>

namespace ScrapHeap {

// Forward declarations
struct Bot;
struct BattleState;

// Frame definition - defines bot body
struct FrameDef {
    std::string name;
    float weight;
    float armor;      // Damage multiplier (1.0 = normal, higher = more resistant)
    float radius;
    std::string description;
};

// Engine definition - defines mobility
struct EngineDef {
    std::string name;
    float torque;     // Turn speed factor
    float power;      // Acceleration/top speed factor
    float weight;
    bool omniDrive;   // Can strafe
    bool gyroStabilized; // Resists knockback rotation
    std::string description;
};

// Weapon types
enum class WeaponType {
    Passive,    // Always active
    Active      // Requires button press
};

// Weapon definition
struct WeaponDef {
    std::string name;
    WeaponType type;
    float damage;
    float knockback;
    float weight;
    float cooldown;       // For active weapons
    float spinUpTime;     // For spinners
    float grabDuration;   // For clamp
    bool ignoresArmor;    // Armor piercing percentage
    float armorPierce;    // 0.0 to 1.0
    std::string description;
};

// Special ability definition
struct SpecialDef {
    std::string name;
    float cooldown;
    float duration;
    std::string description;
};

// Component registry - holds all component definitions
class ComponentRegistry {
public:
    static ComponentRegistry& instance();

    void initialize();

    // Getters
    const std::vector<FrameDef>& getFrames() const { return frames; }
    const std::vector<EngineDef>& getEngines() const { return engines; }
    const std::vector<WeaponDef>& getWeapons() const { return weapons; }
    const std::vector<SpecialDef>& getSpecials() const { return specials; }

    const FrameDef& getFrame(int index) const;
    const EngineDef& getEngine(int index) const;
    const WeaponDef& getWeapon(int index) const;
    const SpecialDef& getSpecial(int index) const;

    int getFrameCount() const { return static_cast<int>(frames.size()); }
    int getEngineCount() const { return static_cast<int>(engines.size()); }
    int getWeaponCount() const { return static_cast<int>(weapons.size()); }
    int getSpecialCount() const { return static_cast<int>(specials.size()); }

private:
    ComponentRegistry() = default;

    std::vector<FrameDef> frames;
    std::vector<EngineDef> engines;
    std::vector<WeaponDef> weapons;
    std::vector<SpecialDef> specials;
};

// Helper to get component names as string vectors (for menus)
std::vector<std::string> getFrameNames();
std::vector<std::string> getEngineNames();
std::vector<std::string> getWeaponNames();
std::vector<std::string> getSpecialNames();

} // namespace ScrapHeap
