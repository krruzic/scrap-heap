#include "components.h"

namespace ScrapHeap {

ComponentRegistry& ComponentRegistry::instance() {
    static ComponentRegistry registry;
    return registry;
}

void ComponentRegistry::initialize() {
    // Initialize Frames - name, weight, armor, radius, shape, description
    // Weight affects speed/accel, Armor is damage multiplier (higher = tougher)
    frames = {
        {"Standard", 55.0f, 1.0f, 28.0f, FrameShape::Square, "Balanced baseline"},
        {"Brick", 95.0f, 1.6f, 38.0f, FrameShape::Rectangle, "Heavy tank - very slow but tough"},
        {"Dart", 25.0f, 0.6f, 20.0f, FrameShape::Triangle, "Glass cannon - fast but fragile"},
        {"Disc", 50.0f, 1.1f, 26.0f, FrameShape::Circle, "Circular - no weak angles"},
        {"Slab", 70.0f, 1.3f, 32.0f, FrameShape::Hexagon, "Armored hex - good defense"},
        {"Roach", 18.0f, 0.4f, 15.0f, FrameShape::Diamond, "Tiny - hard to hit, very fragile"}
    };

    // Initialize Engines - torque, power, weight, omniDrive, gyroStabilized
    // Torque = turn speed, Power = top speed/accel
    engines = {
        {"Standard", 140.0f, 180.0f, 12.0f, false, false, "Balanced baseline"},
        {"Torque Monster", 280.0f, 90.0f, 18.0f, false, false, "Extreme spin, crawls forward"},
        {"Dragster", 60.0f, 320.0f, 14.0f, false, false, "Blazing fast, terrible turning"},
        {"Omni-Drive", 120.0f, 130.0f, 22.0f, true, false, "Can strafe, lower stats"},
        {"Gyro-Stabilized", 110.0f, 160.0f, 16.0f, false, true, "Resists knockback spin"},
        {"Ramjet", 70.0f, 400.0f, 20.0f, false, false, "Insane top speed, very slow turning"}
    };

    // Initialize Weapons
    // damage, knockback, weight, cooldown, spinUpTime, grabDuration, ignoresArmor, armorPierce
    weapons = {
        {"Spinner", WeaponType::Passive, 6.0f, 180.0f, 12.0f, 0.25f, 2.0f, 0.0f, false, 0.0f,
         "Spin to charge, B to release"},
        {"Clamp", WeaponType::Active, 12.0f, 15.0f, 18.0f, 0.0f, 0.0f, 2.5f, false, 0.0f,
         "Grab and crush enemy"},
        {"Hammer", WeaponType::Active, 45.0f, 120.0f, 28.0f, 2.0f, 0.0f, 0.0f, true, 0.6f,
         "Windup overhead slam"},
        {"Battering Ram", WeaponType::Passive, 0.0f, 200.0f, 35.0f, 0.0f, 0.0f, 0.0f, false, 0.0f,
         "Damage scales with speed"},
        {"Flail", WeaponType::Passive, 10.0f, 80.0f, 16.0f, 0.3f, 0.0f, 0.0f, false, 0.0f,
         "Spin-based damage"},
        {"Whip", WeaponType::Active, 18.0f, 50.0f, 10.0f, 1.8f, 0.0f, 0.0f, false, 0.0f,
         "Long range strike"},
        {"Saw Blade", WeaponType::Passive, 5.0f, 30.0f, 14.0f, 0.15f, 0.0f, 0.0f, false, 0.2f,
         "Side-mounted continuous"},
        {"Thwack Bar", WeaponType::Passive, 14.0f, 70.0f, 20.0f, 0.35f, 0.0f, 0.0f, false, 0.0f,
         "Rear attack by spinning"},
        {"Piston Punch", WeaponType::Active, 10.0f, 280.0f, 22.0f, 0.8f, 0.0f, 0.0f, false, 0.0f,
         "Massive knockback burst"},
        {"Dual Spinners", WeaponType::Passive, 4.0f, 140.0f, 20.0f, 0.25f, 2.0f, 0.0f, false, 0.0f,
         "Side coverage, lower damage"}
    };

    // Initialize Specials - cooldown, duration
    specials = {
        {"Boost", 4.0f, 0.8f, "Quick speed burst"},
        {"Anchor", 6.0f, 2.5f, "Immovable, immune to knockback"},
        {"Overdrive", 10.0f, 4.0f, "2x damage dealt and taken"},
        {"Smoke", 8.0f, 5.0f, "Vision-blocking cloud"},
        {"Mine", 5.0f, 0.0f, "Drop explosive mine"},
        {"EMP Pulse", 15.0f, 0.0f, "Disable nearby weapons 2.5s"},
        {"Repair Swarm", 18.0f, 6.0f, "Heal 4 HP/second"},
        {"Backlash", 12.0f, 4.0f, "Reflect 60% damage"},
        {"Berserk", 10.0f, 5.0f, "1.8x speed, 0.4x turn rate"}
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
