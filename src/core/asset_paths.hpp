#pragma once
#include <string_view>

namespace ls::assets {

// Base paths
inline constexpr const char* TD_BASE = "assets/packs/kenney-tower-defense-top-down/PNG/Default size/";
inline constexpr const char* PARTICLE_BASE = "assets/packs/kenney-particle-pack/PNG (Transparent)/";
inline constexpr const char* UI_BASE = "assets/packs/kenney-ui-pack/";
inline constexpr const char* NINJA_BASE = "assets/packs/ninja-adventure/";

// === Terrain Tiles ===
inline constexpr const char* TILE_GRASS = "tile_grass";
inline constexpr const char* TILE_BUILDABLE = "tile_buildable";
inline constexpr const char* TILE_PATH = "tile_path";
inline constexpr const char* TILE_SPAWN = "tile_spawn";
inline constexpr const char* TILE_EXIT = "tile_exit";
inline constexpr const char* TILE_BLOCKED = "tile_blocked";

// === Decorations ===
inline constexpr const char* DECO_TREE_BIG = "deco_tree_big";
inline constexpr const char* DECO_BUSH = "deco_bush";
inline constexpr const char* DECO_LEAF = "deco_leaf";
inline constexpr const char* DECO_FLOWER = "deco_flower";
inline constexpr const char* DECO_ROCK_SM = "deco_rock_sm";
inline constexpr const char* DECO_ROCK_MD = "deco_rock_md";
inline constexpr const char* DECO_ROCK_LG = "deco_rock_lg";
inline constexpr const char* DECO_FLAME = "deco_flame";

// === Biome-specific Tiles ===
inline constexpr const char* BIOME_DESERT_GROUND = "biome_desert_ground";
inline constexpr const char* BIOME_CASTLE_GROUND = "biome_castle_ground";
inline constexpr const char* BIOME_CASTLE_PATH = "biome_castle_path";

// === Tower Bases ===
inline constexpr const char* TOWER_BASE_L1 = "tower_base_l1";
inline constexpr const char* TOWER_BASE_L2 = "tower_base_l2";
inline constexpr const char* TOWER_BASE_L3 = "tower_base_l3";

// === Tower Weapons ===
inline constexpr const char* TOWER_ARROW = "tower_arrow";
inline constexpr const char* TOWER_CANNON = "tower_cannon";
inline constexpr const char* TOWER_ICE = "tower_ice";
inline constexpr const char* TOWER_LIGHTNING = "tower_lightning";
inline constexpr const char* TOWER_POISON = "tower_poison";
inline constexpr const char* TOWER_LASER = "tower_laser";

// === Enemies ===
inline constexpr const char* ENEMY_GRUNT = "enemy_grunt";
inline constexpr const char* ENEMY_RUNNER = "enemy_runner";
inline constexpr const char* ENEMY_TANK = "enemy_tank";
inline constexpr const char* ENEMY_HEALER = "enemy_healer";
inline constexpr const char* ENEMY_FLYING = "enemy_flying";
inline constexpr const char* ENEMY_BOSS = "enemy_boss";

// === Hero ===
inline constexpr const char* HERO_SPRITE = "hero_sprite";

// === Projectiles ===
inline constexpr const char* PROJ_ARROW = "proj_arrow";
inline constexpr const char* PROJ_CANNON = "proj_cannon";
inline constexpr const char* PROJ_ICE = "proj_ice";
inline constexpr const char* PROJ_LIGHTNING = "proj_lightning";
inline constexpr const char* PROJ_POISON = "proj_poison";

// === Pickups ===
inline constexpr const char* COIN_SPRITE = "coin_sprite";

// === Particles ===
inline constexpr const char* PART_FLAME = "part_flame";
inline constexpr const char* PART_SMOKE = "part_smoke";
inline constexpr const char* PART_SPARK = "part_spark";
inline constexpr const char* PART_MAGIC = "part_magic";
inline constexpr const char* PART_MUZZLE = "part_muzzle";
inline constexpr const char* PART_CIRCLE = "part_circle";

// === Font ===
inline constexpr const char* FONT_MAIN = "font_main";

// === UI Sounds ===
inline constexpr const char* SND_CLICK = "snd_click";

// === Music ===
inline constexpr const char* MUSIC_MENU = "music_menu";
inline constexpr const char* MUSIC_PLAIN = "music_plain";
inline constexpr const char* MUSIC_SWAMP = "music_swamp";
inline constexpr const char* MUSIC_BOSS = "music_boss";

} // namespace ls::assets
