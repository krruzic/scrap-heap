# Scrap Heap - Project Handoff Document

## Game Overview

**Scrap Heap** is a local multiplayer (2-4 players) Battlebots-style arena combat game. Players build custom combat robots from modular components and fight in timed deathmatches. Think "Rocket League meets Robot Wars."

### Core Loop
1. **Garage** - Players join, pick components, ready up
2. **Stage Select** - First ready player picks arena
3. **Battle** - 3-minute timed match, most kills wins
4. **Results** - Stats screen, return to menu

---

## Component System

### Frames (6) - The chassis/body
| Name | Weight | Armor | Radius | Personality |
|------|--------|-------|--------|-------------|
| Dart | 15 | 0.5 | 22 | Glass cannon, fast & fragile |
| Disc | 22 | 0.8 | 26 | Balanced spinner platform |
| Wedge | 28 | 1.0 | 28 | Low profile, gets under opponents |
| Box | 35 | 1.1 | 30 | Tanky all-rounder |
| Brick | 50 | 1.3 | 34 | Maximum armor, slow |
| Roach | 12 | 0.6 | 20 | Tiny, hard to hit |

### Engines (4) - Movement style
| Name | Power | Torque | Weight | Special |
|------|-------|--------|--------|---------|
| Standard | 180 | 150 | 15 | Baseline |
| Dragster | 350 | 80 | 18 | Fast straight, bad turning |
| Torque Monster | 120 | 250 | 22 | Slow but pivots instantly |
| Omni-Drive | 120 | 130 | 22 | **Strafe movement** - moves in stick direction |

### Weapons (10)
| Name | Type | Damage | Notes |
|------|------|--------|-------|
| Spinner | Passive | 15 | Horizontal bar, spins up over time, momentum-based |
| Clamp | Active | 8 | Grabs enemy, holds them, mash to escape |
| Hammer | Active | 25 | Overhead swing, windup time, high damage |
| Battering Ram | Passive | 12 | Front ram, damage on collision |
| Flail | Passive | 18 | Spinning ball on chain, wide arc |
| Whip | Active | 10 | Extendable lash, medium range |
| Saw Blade | Passive | 8 | Side-mounted, continuous damage |
| Thwack Bar | Passive | 20 | Rear spinner, rewards spinning in place |
| Piston Punch | Active | 15 | Extendable spike, quick jab |
| Dual Spinners | Passive | 12 | Two smaller spinners, balanced |

### Specials (9) - Cooldown abilities
| Name | Cooldown | Effect |
|------|----------|--------|
| Boost | 8s | Speed burst forward |
| Anchor | 10s | Become immovable briefly |
| Overdrive | 12s | Double damage for 3s |
| Smoke | 15s | Drop smoke cloud, obscures vision |
| Mine | 12s | Drop proximity mine behind you |
| EMP | 18s | Disable nearby enemy controls |
| Repair | 20s | Heal 30% health |
| Backlash | 10s | Reflect damage back |
| Berserk | 15s | Speed + damage boost, take more damage |

---

## Controls

### Gamepad (Tank Controls)
- **Left Stick** - Steering direction
- **RT / RB** - Throttle (forward)
- **LT / LB** - Reverse
- **B (East)** - Attack / Activate weapon
- **A (South)** - Use special ability
- **X / Y** - Use held powerup

### Omni-Drive Exception
When using Omni-Drive engine:
- Stick direction = movement direction (strafe)
- Only rotates when NOT holding throttle
- Allows aiming while stationary

### Keyboard (Player 1)
- **WASD / Arrows** - Movement
- **Space** - Attack
- **Shift** - Special
- **Tab** - Powerup
- **X** - Add AI opponent (in garage)

---

## Battle Mechanics

### Damage & Health
- Base health ~100, modified by weight
- Armor reduces incoming damage (0.5x to 1.3x)
- Knockback on hit based on damage + weapon
- **Respawn** after 3 seconds on death (infinite lives)

### Spinner Mechanics
- Spins up while driving (0% to 100%)
- Damage scales with spin speed
- Getting hit **stuns spinner** (loses momentum)
- Rewards keeping distance and building speed

### Grab/Clamp Mechanics
- Clamp grabs enemy on contact
- Grabbed bot can **mash buttons** to escape
- Grabber can drag victim around
- Auto-releases after ~3 seconds

### Storm (Shrinking Safe Zone)
- Activates 30 seconds into match
- Purple electric wall closes in phases
- Bots outside take % health damage per second
- Forces engagement, prevents camping
- **Resets on kill** - gives breathing room

### Win Condition
- 3-minute timed match
- **Most kills wins**
- Tie = draw

---

## What Works (Keep These)

1. **Component variety** - Lots of viable builds
2. **Storm mechanic** - Forces action, creates tension
3. **Respawn system** - Everyone stays engaged
4. **Tank controls** - Feels like driving a machine
5. **Omni-Drive** - Fun alternative movement style
6. **Spinner momentum** - Skill-based weapon timing
7. **Grab escape** - Interactive counter-play

## What Doesn't Work (Fix These)

1. **No juice** - Hits feel weak, need:
   - Screen shake on impact
   - Hit pause (2-3 frame freeze)
   - Sparks and debris particles
   - Camera zoom on big hits

2. **No sound** - Silent combat is boring

3. **Flat arenas** - Need hazards:
   - Spinning saw blades
   - Fire jets (timed)
   - Pit traps / out-of-bounds
   - Bumpers / pneumatic rams

4. **AI too simple** - Just rushes, needs:
   - Weapon-appropriate behavior
   - Dodging
   - Hazard awareness

5. **Combat feels same-y** - Needs:
   - More dramatic knockback
   - Juggling / combo potential
   - Environmental kills

---

## Bevy 3D Development Prompt

```
Build "Scrap Heap" - a 3D local multiplayer (2-4 players) robot combat arena game in Bevy.

CONCEPT: Battlebots meets Rocket League. Players build modular combat robots and fight in timed arena deathmatches.

CORE SYSTEMS:
1. Component-based bot builder (Frame + Engine + Weapon + Special)
2. Physics-driven combat with knockback and momentum
3. Tank controls (triggers for throttle/reverse, stick for steering)
4. Shrinking storm zone that forces engagement
5. 3-minute timed matches, most kills wins, infinite respawns

CAMERA: Top-down or isometric, shows full arena. Optional: dynamic zoom on action.

GAME FLOW:
- Main Menu → Garage (build bots) → Stage Select → Battle → Results → Menu

COMPONENT STATS:
Frames affect: weight, armor, hitbox size, visual shape
Engines affect: acceleration, top speed, turn rate, movement style
Weapons affect: damage, range, attack pattern (active vs passive)
Specials: cooldown abilities (boost, anchor, mines, EMP, etc.)

CRITICAL FEEL ELEMENTS:
- Screen shake on hits (intensity scales with damage)
- Hit pause (2-3 frame freeze on impact)
- Particle bursts (sparks, debris, smoke)
- Camera punch toward impact
- Slow-mo on kills (0.5 second)

PHYSICS:
- Rigid body bots with mass based on components
- Collision damage based on relative velocity
- Knockback force = damage × direction
- Spinner weapons have angular momentum (more spin = more damage)
- Friction affects acceleration and drift

CONTROLS (per player):
- Left stick: steering
- Right trigger: throttle
- Left trigger: reverse
- East button (B): attack
- South button (A): special ability

AI OPPONENTS:
- Press X to add CPU player with random build
- AI behaviors: chase, flee, circle-strafe, use abilities
- Difficulty affects reaction time and accuracy

ARENA HAZARDS:
- Spinning saw blades (instant high damage)
- Fire jets (timed bursts)
- Pit edges (fall = death)
- Bumper walls (bounce bots)
- Destructible pillars

STORM SYSTEM:
- Purple electric wall shrinks arena over time
- Deals % health damage to bots outside
- Visual: crackling lightning, ominous glow
- Resets position when a bot is killed

UI:
- Garage: 4 quadrants, each player builds independently
- Battle HUD: health bars in corners, timer top center, kill feed
- Minimal, arcade-style, high contrast colors
```

---

## Asset List

### 3D Models

**Bot Frames (6 models + damaged variants)**
- `frame_dart.glb` - Sleek arrow/wedge shape
- `frame_disc.glb` - Circular/UFO shape
- `frame_wedge.glb` - Low-profile ramp
- `frame_box.glb` - Rectangular tank
- `frame_brick.glb` - Heavy reinforced cube
- `frame_roach.glb` - Small compact dome

**Weapons (10 models, need idle + active states)**
- `weapon_spinner.glb` - Horizontal spinning bar
- `weapon_clamp.glb` - Pincer/grabber arms
- `weapon_hammer.glb` - Overhead pneumatic hammer
- `weapon_ram.glb` - Reinforced front plate
- `weapon_flail.glb` - Ball and chain
- `weapon_whip.glb` - Segmented lash (rigged)
- `weapon_saw.glb` - Circular saw blade
- `weapon_thwack.glb` - Rear-mounted bar
- `weapon_piston.glb` - Extendable spike
- `weapon_dual_spinners.glb` - Twin small discs

**Arena Elements**
- `arena_floor.glb` - Base arena floor with grid texture
- `arena_wall.glb` - Boundary walls (modular)
- `hazard_sawblade.glb` - Spinning floor saw
- `hazard_firejet.glb` - Fire vent grate
- `hazard_pit.glb` - Danger pit with grating
- `hazard_bumper.glb` - Pneumatic bumper wall
- `hazard_pillar.glb` - Destructible pillar (+ destroyed version)

**Effects**
- `particle_spark.glb` - Metal spark
- `particle_debris.glb` - Metal chunks (multiple sizes)
- `powerup_orb.glb` - Collectible powerup base
- `mine.glb` - Deployed mine model
- `smoke_cloud.glb` - Volumetric smoke (or use particles)

**UI Elements**
- `ui_healthbar.png` - Health bar frame
- `ui_cooldown_ring.png` - Ability cooldown indicator
- `ui_kill_icon.png` - Skull/explosion icon

### Textures

**Materials (PBR: albedo, normal, roughness, metallic)**
- `metal_brushed` - Base bot material
- `metal_damaged` - Scratched/dented variant
- `metal_chrome` - Shiny spinner material
- `rubber_tire` - Wheel texture
- `concrete_floor` - Arena floor
- `hazard_stripe` - Yellow/black warning pattern
- `electric_storm` - Storm wall effect (emissive)
- `fire_emissive` - Fire jet glow

**Team Colors (tintable)**
- Red, Blue, Yellow, Green, Purple, Orange, Cyan, Pink

---

## Sound Effects List

### Combat
- `hit_metal_light.wav` - Small impact
- `hit_metal_heavy.wav` - Big impact
- `hit_metal_scrape.wav` - Grinding contact
- `spinner_whoosh.wav` - Spinner passing by (loopable)
- `spinner_impact.wav` - Spinner hitting target
- `spinner_spinup.wav` - Spinner accelerating
- `spinner_spindown.wav` - Spinner losing momentum
- `clamp_grab.wav` - Grabber catching
- `clamp_release.wav` - Grabber releasing
- `hammer_windup.wav` - Pneumatic charge
- `hammer_strike.wav` - Hammer impact
- `saw_cutting.wav` - Saw blade grinding (loop)
- `piston_fire.wav` - Piston extending
- `whip_crack.wav` - Whip snap

### Specials
- `boost_activate.wav` - Speed boost whoosh
- `anchor_clang.wav` - Heavy anchor drop
- `overdrive_powerup.wav` - Power surge
- `smoke_deploy.wav` - Smoke grenade pop
- `mine_deploy.wav` - Mine placement beep
- `mine_explode.wav` - Mine detonation
- `emp_pulse.wav` - Electronic disruption
- `repair_heal.wav` - Repair nanobots sound
- `berserk_rage.wav` - Aggressive powerup

### Arena
- `hazard_saw_loop.wav` - Floor saw spinning
- `hazard_fire_burst.wav` - Fire jet ignition
- `hazard_fire_loop.wav` - Fire burning
- `hazard_bumper_hit.wav` - Pneumatic bounce
- `pillar_crack.wav` - Pillar taking damage
- `pillar_collapse.wav` - Pillar destroyed
- `pit_fall.wav` - Bot falling into pit
- `storm_ambience.wav` - Electric crackling (loop)
- `storm_damage.wav` - Taking storm damage

### Bot Movement
- `engine_idle.wav` - Idle engine hum (loop)
- `engine_drive.wav` - Driving engine (loop, pitch with speed)
- `engine_reverse.wav` - Reverse beeping
- `tire_squeal.wav` - Sharp turn
- `bot_spawn.wav` - Respawn teleport
- `bot_death.wav` - Explosion/destruction

### UI
- `ui_select.wav` - Menu selection
- `ui_confirm.wav` - Menu confirm
- `ui_back.wav` - Menu back
- `ui_ready.wav` - Player ready up
- `match_countdown.wav` - 3-2-1 beeps
- `match_start.wav` - Fight horn
- `match_end.wav` - Final buzzer
- `kill_announce.wav` - Kill notification sting
- `powerup_collect.wav` - Powerup pickup

### Music
- `music_menu.ogg` - Menu theme (industrial/electronic)
- `music_garage.ogg` - Garage building music
- `music_battle_01.ogg` - Combat track 1 (intense)
- `music_battle_02.ogg` - Combat track 2 (variety)
- `music_victory.ogg` - Winner fanfare
- `music_draw.ogg` - Draw/stalemate jingle

---

## Final Notes

**Priority order for 3D version:**
1. Get one bot driving with physics
2. Add combat with one weapon (spinner)
3. Add juice (shake, particles, sound)
4. Expand to full component system
5. Add arena hazards
6. Polish UI and menus

**The game lives or dies on feel.** A simple spinner hitting another bot should feel CRUNCHY - screen shake, sparks flying, metal scraping sound, brief slowdown. Without that, it's just shapes bumping.

Good luck!
