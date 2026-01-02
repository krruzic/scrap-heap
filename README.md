# SCRAP HEAP

A top-down arena combat game where players build custom fighting robots and battle in local multiplayer matches.

## Features

- **2-4 Player Local Multiplayer** - Couch versus combat with gamepad and keyboard support
- **Deep Customization** - Mix and match frames, engines, weapons, and special abilities
- **Multiple Stages** - Each arena has unique hazards and layouts
- **Powerups** - Collect health, speed boosts, damage buffs, and more
- **Persistent Stats** - Track wins, losses, kills, damage, and more per player tag
- **Shrinking Wall** - Battle royale-style danger zone that closes in after 30 seconds

## Controls

### Menu Navigation
- **D-PAD / Arrow Keys**: Navigate
- **A / Enter**: Select
- **B / Escape**: Back

### Battle Controls
- **D-PAD UP / W**: Forward thrust
- **D-PAD DOWN / S**: Reverse thrust
- **D-PAD LEFT / A**: Rotate counter-clockwise
- **D-PAD RIGHT / D**: Rotate clockwise
- **A / Space**: Weapon action / Mash to escape grab
- **B / Shift**: Special ability
- **X or Y / Tab**: Use held powerup
- **Start / Escape**: Return to main menu

## Building

### Requirements
- CMake 3.20+
- C++17 compatible compiler
- Internet connection (for SDL3 download on first build)

### Build Instructions

```bash
mkdir build
cd build
cmake ..
cmake --build .
```

The build process will automatically download SDL3 and SDL3_ttf.

### Running

```bash
./scrapheap
```

Note: A TrueType font file is required. The game will look for `assets/font.ttf` or fall back to system fonts.

## Components

### Frames
| Name | Weight | Armor | Notes |
|------|--------|-------|-------|
| Standard | 50 | 1.0x | Balanced baseline |
| Brick | 80 | 1.3x | Tank - slow but tough |
| Dart | 30 | 0.7x | Fast but fragile |
| Disc | 50 | 1.0x | No weak angles |
| Slab | 60 | 0.9x | Resists lateral knockback |
| Roach | 20 | 0.5x | Tiny target |

### Engines
| Name | Torque | Power | Notes |
|------|--------|-------|-------|
| Standard | 150 | 200 | Balanced |
| Torque Monster | 250 | 120 | Spins fast, moves slow |
| Dragster | 80 | 300 | Fast straights, bad turns |
| Omni-Drive | 150 | 150 | Can strafe |
| Gyro-Stabilized | 130 | 180 | Resists knockback rotation |
| Ramjet | 100 | 350 | High top speed |

### Weapons
- **Spinner** - Continuous contact damage, spins up over time
- **Clamp** - Grab and drag enemies
- **Hammer** - Heavy hit with armor pierce
- **Battering Ram** - Damage based on speed
- **Flail** - Tethered ball that trails rotation
- **Whip** - Long range attack
- **Saw Blade** - Side-mounted, punishes flankers
- **Thwack Bar** - Rear-mounted attack
- **Piston Punch** - Short range, huge knockback
- **Dual Spinners** - Side-mounted spinner pair

### Special Abilities
- **Boost** - Burst of speed
- **Anchor** - Immune to knockback
- **Overdrive** - 1.5x damage (given and taken)
- **Smoke** - Drop vision-blocking cloud
- **Mine** - Drop explosive trap
- **EMP Pulse** - Disable nearby weapons
- **Repair Swarm** - Heal over time
- **Backlash** - Reflect damage to attackers
- **Berserk** - Speed boost, reduced turning

## Stages

1. **The Pit** - Training stage, no hazards
2. **Hazard Zone** - Corner flames on timers
3. **Steel Cage** - Electrified walls
4. **Lava Arena** - Center pit (instant death)

## Shrinking Wall

After 30 seconds of combat, a translucent purple danger zone begins closing in from the edges of the arena:
- Deals damage over time to bots outside the safe zone
- Shrinks in phases, getting faster each phase
- Damage increases each phase (5 DPS base, +3 per phase)
- Forces combat toward the center
- Warning appears 5 seconds before each shrink phase

## Save Data

Player stats are stored in `scrapheap.sav` (binary format) including:
- Win/Loss/Draw records
- Kill/Death counts
- Total damage dealt and taken
- Match time played
- Win streaks
- Favorite components (most used frame, engine, weapon, special)

The game automatically migrates data from legacy text files (tags.txt, stats.txt) to the new format.

## License

This project is provided as-is for educational purposes.
