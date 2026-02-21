# Last Stand

A tower defense game built with C++23, raylib, and EnTT ECS.

Control a hero who fights enemies directly to earn gold, then build and upgrade towers to defend against 30 waves of increasingly difficult enemies across three distinct biomes.

![C++23](https://img.shields.io/badge/C%2B%2B-23-blue)
![raylib](https://img.shields.io/badge/raylib-5.5-green)
![EnTT](https://img.shields.io/badge/EnTT-3.14-orange)

## Features

- **Hero combat** -- WASD movement, auto-attack nearby enemies, 3 active abilities (Fire, Heal, Lightning)
- **6 tower types** -- Arrow, Cannon, Ice, Lightning, Poison, Laser with unique effects (splash, slow, chain, DoT, beam)
- **Tower upgrades** -- 3 upgrade levels per tower with increasing stats, repair damaged towers
- **30 enemy waves** -- Grunts, Runners, Tanks, Healers, Flying units, and Bosses every 5 waves
- **3 biome maps** -- Forest, Desert, Castle with distinct visuals, decorations, and music
- **Hero progression** -- Level up from combat XP, persistent upgrades between runs
- **Difficulty modes** -- Easy, Normal, Hard with scaled gold/lives/enemy stats
- **Save/Load** -- Mid-game saves with Ctrl+S

## Biomes

| | Forest | Desert | Castle |
|--|--------|--------|--------|
| Ground | Green grass | Sandy terrain | Blue-grey stone |
| Path | Brown dirt | Sandy-tinted dirt | Hex stone |
| Decorations | Trees, bushes, flowers | Sparse rocks, scrub | Rocks, flame torches |
| Music | Plains theme | Swamp theme | Boss theme |

## Building

### Prerequisites

- C++23 compatible compiler (GCC 13+, Clang 17+)
- CMake 3.25+
- Linux: `libgl-dev`, `libx11-dev`, `libxrandr-dev`, `libxi-dev` (for raylib)

Dependencies (raylib, EnTT, nlohmann_json) are fetched automatically via CPM.

### Build & Run

```bash
make build   # Debug build
make run     # Build and run
make release # Optimized release build
```

Or manually with CMake:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
./build/LastStand
```

## Controls

| Key | Action |
|-----|--------|
| WASD | Move hero |
| Q | Fire ability (AoE damage) |
| E | Heal ability |
| R | Lightning ability (chain damage) |
| 1-6 | Select tower type to place |
| Left Click | Place tower / Select placed tower |
| Right Click | Cancel placement / Deselect |
| Space | Start next wave early |
| F | Toggle fast speed (2x) |
| P / Esc | Pause |
| M | Mute music |
| +/- | Adjust volume |
| Ctrl+S | Save game |

## Project Structure

```
src/
  core/           -- Game state, constants, types, asset paths, biome themes
  components/     -- ECS component definitions
  factory/        -- Entity creation (enemies, towers, projectiles, hero)
  managers/       -- Assets, maps, waves, saves, sound, tower registry
  states/         -- Game states (menu, map select, playing, paused, game over, victory, upgrades)
  systems/        -- Render, update, and UI systems
  main.cpp        -- Entry point
assets/
  maps/           -- JSON map definitions (forest, desert, castle)
  packs/          -- Kenney asset packs + Ninja Adventure pack
  fonts/          -- UI fonts
  sounds/         -- Sound effects
```

## Credits

- [Kenney Tower Defense Top-Down](https://kenney.nl/assets/tower-defense-top-down) -- tiles, towers, enemies
- [Kenney Particle Pack](https://kenney.nl/assets/particle-pack) -- particle effects
- [Kenney UI Pack](https://kenney.nl/assets/ui-pack) -- fonts, UI sounds
- [Ninja Adventure](https://pixel-boy.itch.io/ninja-adventure-asset-pack) -- hero sprite, music
- [raylib](https://www.raylib.com/) -- graphics/audio framework
- [EnTT](https://github.com/skypjack/entt) -- entity component system
- [nlohmann/json](https://github.com/nlohmann/json) -- JSON parsing

## License

MIT
