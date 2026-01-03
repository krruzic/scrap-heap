#include "components.h"

namespace ScrapHeap {

ComponentRegistry& ComponentRegistry::instance() {
    static ComponentRegistry registry;
    return registry;
}

void ComponentRegistry::initialize() {
    // Initialize Frames - name, weight, armor, radius, shape, description
    frames = {
        {"Standard", 50.0f, 1.0f, 30.0f, FrameShape::Square, "Balanced baseline"},
        {"Brick", 80.0f, 1.3f, 35.0f, FrameShape::Rectangle, "Tank - slow but tough"},
        {"Dart", 30.0f, 0.7f, 22.0f, FrameShape::Triangle, "Glass cannon - fast but fragile"},
        {"Disc", 50.0f, 1.0f, 28.0f, FrameShape::Circle, "Circular - no weak angles"},
        {"Slab", 60.0f, 0.9f, 32.0f, FrameShape::Hexagon, "Hex - resists lateral knockback"},
        {"Roach", 20.0f, 0.5f, 18.0f, FrameShape::Diamond, "Tiny - hard to hit, easy to kill"}
    };

    // Initialize Engines
    engines = {
        {"Standard", 150.0f, 200.0f, 10.0f, false, false, "Balanced baseline"},
        {"Torque Monster", 250.0f, 120.0f, 15.0f, false, false, "Spins fast, moves slow"},
        {"Dragster", 80.0f, 300.0f, 12.0f, false, false, "Fast straights, bad turns"},
        {"Omni-Drive", 150.0f, 150.0f, 20.0f, true, false, "Can strafe"},
        {"Gyro-Stabilized", 130.0f, 180.0f, 15.0f, false, true, "Resists knockback rotation"},
        {"Ramjet", 100.0f, 350.0f, 18.0f, false, false, "Slow accel, very high top speed"}
    };

    // Initialize Weapons
    weapons = {
        // name, type, damage, knockback, weight, cooldown, spinUpTime, grabDuration, ignoresArmor, armorPierce, description
        {"Spinner", WeaponType::Passive, 8.0f, 80.0f, 15.0f, 0.3f, 1.5f, 0.0f, false, 0.0f,
         "Continuous on contact, resets on damage"},
        {"Clamp", WeaponType::Active, 15.0f, 20.0f, 20.0f, 0.0f, 0.0f, 3.0f, false, 0.0f,
         "Grapple and drag target"},
        {"Hammer", WeaponType::Active, 35.0f, 150.0f, 25.0f, 1.5f, 0.0f, 0.0f, true, 0.5f,
         "0.8s windup, ignores 50% armor"},
        {"Battering Ram", WeaponType::Passive, 0.0f, 120.0f, 30.0f, 0.0f, 0.0f, 0.0f, false, 0.0f,
         "Damage = speed * 2, rewards momentum"},
        {"Flail", WeaponType::Passive, 12.0f, 100.0f, 20.0f, 0.0f, 0.0f, 0.0f, false, 0.0f,
         "Tethered ball, trails rotation"},
        {"Whip", WeaponType::Active, 20.0f, 60.0f, 12.0f, 2.0f, 0.0f, 0.0f, false, 0.0f,
         "Long range, slow retract"},
        {"Saw Blade", WeaponType::Passive, 6.0f, 40.0f, 18.0f, 0.2f, 0.0f, 0.0f, false, 0.0f,
         "Side-mounted arc, punishes flankers"},
        {"Thwack Bar", WeaponType::Passive, 15.0f, 90.0f, 22.0f, 0.3f, 0.0f, 0.0f, false, 0.0f,
         "Rear-mounted, attack by spinning"},
        {"Piston Punch", WeaponType::Active, 8.0f, 200.0f, 20.0f, 1.0f, 0.0f, 0.0f, false, 0.0f,
         "Short range, huge knockback"},
        {"Dual Spinners", WeaponType::Passive, 5.0f, 50.0f, 22.0f, 0.3f, 1.5f, 0.0f, false, 0.0f,
         "Side-mounted pair, covers flanks"}
    };

    // Initialize Specials
    specials = {
        {"Boost", 3.0f, 0.5f, "Burst of speed forward"},
        {"Anchor", 5.0f, 2.0f, "Cannot move, immune to knockback"},
        {"Overdrive", 8.0f, 3.0f, "1.5x weapon damage, take 1.5x damage"},
        {"Smoke", 10.0f, 4.0f, "Drop vision-obscuring cloud"},
        {"Mine", 6.0f, 0.0f, "Drop mine, arms after 1s"},
        {"EMP Pulse", 12.0f, 0.0f, "Disable nearby enemy weapons for 2s"},
        {"Repair Swarm", 15.0f, 5.0f, "Heal 5 HP/second"},
        {"Backlash", 10.0f, 3.0f, "Reflect 50% damage to attacker"},
        {"Berserk", 8.0f, 4.0f, "1.5x speed, 0.5x turn rate"}
    };
}

const FrameDef& ComponentRegistry::getFrame(int index) const {
    if (index < 0 || index >= static_cast<int>(frames.size())) {
        return frames[0];
    }
    return frames[index];
}

const EngineDef& ComponentRegistry::getEngine(int index) const {
    if (index < 0 || index >= static_cast<int>(engines.size())) {
        return engines[0];
    }
    return engines[index];
}

const WeaponDef& ComponentRegistry::getWeapon(int index) const {
    if (index < 0 || index >= static_cast<int>(weapons.size())) {
        return weapons[0];
    }
    return weapons[index];
}

const SpecialDef& ComponentRegistry::getSpecial(int index) const {
    if (index < 0 || index >= static_cast<int>(specials.size())) {
        return specials[0];
    }
    return specials[index];
}

std::vector<std::string> getFrameNames() {
    std::vector<std::string> names;
    for (const auto& frame : ComponentRegistry::instance().getFrames()) {
        names.push_back(frame.name);
    }
    return names;
}

std::vector<std::string> getEngineNames() {
    std::vector<std::string> names;
    for (const auto& engine : ComponentRegistry::instance().getEngines()) {
        names.push_back(engine.name);
    }
    return names;
}

std::vector<std::string> getWeaponNames() {
    std::vector<std::string> names;
    for (const auto& weapon : ComponentRegistry::instance().getWeapons()) {
        names.push_back(weapon.name);
    }
    return names;
}

std::vector<std::string> getSpecialNames() {
    std::vector<std::string> names;
    for (const auto& special : ComponentRegistry::instance().getSpecials()) {
        names.push_back(special.name);
    }
    return names;
}

} // namespace ScrapHeap
