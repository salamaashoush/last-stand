#pragma once
#include <entt/entt.hpp>
#include <unordered_set>
#include <optional>
#include <string>
#include "types.hpp"
#include "constants.hpp"
#include "event_bus.hpp"
#include "state_machine.hpp"
#include "managers/asset_manager.hpp"
#include "managers/map_manager.hpp"
#include "managers/wave_manager.hpp"
#include "managers/tower_registry.hpp"
#include "managers/save_manager.hpp"
#include "core/hero_upgrades.hpp"
#include "managers/sound_manager.hpp"
#include "ai/pathfinding.hpp"
#include "core/asset_paths.hpp"

namespace ls {

struct WaveBanner {
    std::string text;
    float timer{0.0f};
    Color color{WHITE};
    bool active{false};
};

struct GameStats {
    int total_kills{};
    int gold_earned{};
    int gold_spent{};
    int towers_built{};
    int towers_sold{};
    int boss_kills{};
    float time_elapsed{};
    int hero_deaths{};
};

struct Tutorial {
    bool active{true};
    int step{0};
    float timer{0.0f};
    bool completed{false};
};

struct PlayState {
    Gold gold{STARTING_GOLD};
    int lives{STARTING_LIVES};
    WaveNum current_wave{0};
    bool wave_active{false};
    float wave_timer{WAVE_DELAY};
    float spawn_timer{};
    size_t spawn_index{};
    size_t spawn_sub_index{};
    int enemies_alive{};
    int total_kills{};
    entt::entity hero{entt::null};
    entt::entity selected_tower{entt::null};
    std::optional<TowerType> placing_tower;
    std::unordered_set<GridPos, GridPosHash> tower_positions;
    std::vector<Vec2> enemy_path;
    std::vector<Vec2> flying_path;
    bool game_speed_fast{false};

    // Screen shake
    float shake_intensity{0};
    float shake_timer{0};
    Vec2 shake_offset{};

    // Wave banner
    WaveBanner banner;

    // Stats
    GameStats stats;

    // Tutorial
    Tutorial tutorial;

    // Tower popover hit area (set by UI each frame, used to block input)
    Rectangle popover_rect{};
};

struct Game {
    entt::registry registry;
    EventDispatcher dispatcher;
    StateMachine state_machine;
    AssetManager assets;
    MapManager map_manager;
    WaveManager wave_manager;
    TowerRegistry tower_registry;
    SaveManager save_manager;
    SoundManager sounds;
    MapData current_map;
    PlayState play;
    HeroUpgrades upgrades;
    Difficulty difficulty{Difficulty::Normal};
    bool running{true};
    std::string save_path{"save.json"};
    std::optional<SaveData> pending_load;

    // Camera
    Camera2D camera{};

    // Music state
    Music* current_music{nullptr};
    float music_volume{0.5f};
    bool music_muted{false};

    void recalculate_path() {
        // Use map waypoints as the canonical enemy path
        play.enemy_path.clear();
        for (auto& wp : current_map.path_waypoints) {
            play.enemy_path.push_back(current_map.grid_to_world(wp));
        }
        // Flying path: straight line from spawn to exit
        play.flying_path.clear();
        play.flying_path.push_back(current_map.grid_to_world(current_map.spawn));
        play.flying_path.push_back(current_map.grid_to_world(current_map.exit_pos));
    }

    bool can_place_tower(GridPos pos) const {
        if (!current_map.is_buildable(pos)) return false;
        if (play.tower_positions.contains(pos)) return false;
        return true;
    }

    Vec2 mouse_world() const {
        auto wp = GetScreenToWorld2D(GetMousePosition(), camera);
        return Vec2::from_raylib(wp);
    }

    GridPos mouse_grid() const {
        return current_map.world_to_grid(mouse_world());
    }
};

inline void load_assets(Game& game) {
    auto& a = game.assets;
    using namespace assets;
    auto td = [](const char* file) { return std::string(TD_BASE) + file; };
    auto pt = [](const char* file) { return std::string(PARTICLE_BASE) + file; };

    // Helper that logs errors on failure
    auto load_tex = [&](const char* name, const std::string& path) {
        auto result = a.load_texture(name, path);
        if (!result) {
            TraceLog(LOG_ERROR, "ASSET: Failed to load texture '%s' from '%s'", name, path.c_str());
        }
    };
    auto load_snd = [&](const char* name, const std::string& path) {
        auto result = a.load_sound(name, path);
        if (!result) {
            TraceLog(LOG_ERROR, "ASSET: Failed to load sound '%s' from '%s'", name, path.c_str());
        }
    };
    auto load_fnt = [&](const char* name, const std::string& path) {
        auto result = a.load_font(name, path);
        if (!result) {
            TraceLog(LOG_ERROR, "ASSET: Failed to load font '%s' from '%s'", name, path.c_str());
        }
    };
    auto load_mus = [&](const char* name, const std::string& path) {
        auto result = a.load_music(name, path);
        if (!result) {
            TraceLog(LOG_ERROR, "ASSET: Failed to load music '%s' from '%s'", name, path.c_str());
        }
    };

    // Terrain tiles
    load_tex(TILE_GRASS,     td("towerDefense_tile024.png"));
    load_tex(TILE_BUILDABLE, td("towerDefense_tile133.png"));
    load_tex(TILE_PATH,      td("towerDefense_tile050.png"));
    load_tex(TILE_SPAWN,     td("towerDefense_tile044.png"));
    load_tex(TILE_EXIT,      td("towerDefense_tile045.png"));
    load_tex(TILE_BLOCKED,   td("towerDefense_tile256.png"));

    // Decorations
    load_tex(DECO_TREE_BIG,  td("towerDefense_tile130.png"));
    load_tex(DECO_BUSH,      td("towerDefense_tile131.png"));
    load_tex(DECO_LEAF,      td("towerDefense_tile132.png"));
    load_tex(DECO_FLOWER,    td("towerDefense_tile134.png"));
    load_tex(DECO_ROCK_SM,   td("towerDefense_tile135.png"));
    load_tex(DECO_ROCK_MD,   td("towerDefense_tile136.png"));
    load_tex(DECO_ROCK_LG,   td("towerDefense_tile137.png"));
    load_tex(DECO_FLAME,     td("towerDefense_tile295.png"));

    // Biome-specific tiles
    load_tex(BIOME_DESERT_GROUND, td("towerDefense_tile160.png"));
    load_tex(BIOME_CASTLE_GROUND, td("towerDefense_tile159.png"));
    load_tex(BIOME_CASTLE_PATH,   td("towerDefense_tile158.png"));

    // Tower bases
    load_tex(TOWER_BASE_L1,  td("towerDefense_tile180.png"));
    load_tex(TOWER_BASE_L2,  td("towerDefense_tile181.png"));
    load_tex(TOWER_BASE_L3,  td("towerDefense_tile183.png"));

    // Tower weapons
    load_tex(TOWER_ARROW,    td("towerDefense_tile249.png"));
    load_tex(TOWER_CANNON,   td("towerDefense_tile204.png"));
    load_tex(TOWER_ICE,      td("towerDefense_tile246.png"));
    load_tex(TOWER_LIGHTNING, td("towerDefense_tile206.png"));
    load_tex(TOWER_POISON,   td("towerDefense_tile291.png"));
    load_tex(TOWER_LASER,    td("towerDefense_tile250.png"));

    // Enemies - using vehicle/unit sprites from TD pack
    load_tex(ENEMY_GRUNT,    td("towerDefense_tile245.png"));  // green armored vehicle
    load_tex(ENEMY_RUNNER,   td("towerDefense_tile270.png"));  // green plane (fast)
    load_tex(ENEMY_TANK,     td("towerDefense_tile247.png"));  // brown heavy vehicle
    load_tex(ENEMY_HEALER,   td("towerDefense_tile248.png"));  // grey support vehicle
    load_tex(ENEMY_FLYING,   td("towerDefense_tile271.png"));  // grey plane
    load_tex(ENEMY_BOSS,     td("towerDefense_tile252.png"));  // red rocket

    // Hero
    load_tex(HERO_SPRITE, std::string(NINJA_BASE) + "content/character/ninja_blue/sprite.png");

    // Pickups
    load_tex(COIN_SPRITE, td("towerDefense_tile272.png"));

    // Projectiles
    load_tex(PROJ_ARROW,     td("towerDefense_tile272.png"));
    load_tex(PROJ_CANNON,    td("towerDefense_tile272.png"));
    load_tex(PROJ_ICE,       pt("circle_02.png"));
    load_tex(PROJ_LIGHTNING, pt("spark_05.png"));
    load_tex(PROJ_POISON,    pt("circle_01.png"));

    // Particles
    load_tex(PART_FLAME,     pt("flame_01.png"));
    load_tex(PART_SMOKE,     pt("smoke_04.png"));
    load_tex(PART_SPARK,     pt("spark_01.png"));
    load_tex(PART_MAGIC,     pt("magic_01.png"));
    load_tex(PART_MUZZLE,    pt("muzzle_01.png"));
    load_tex(PART_CIRCLE,    pt("circle_01.png"));

    // Font
    load_fnt(FONT_MAIN, std::string(UI_BASE) + "Font/Kenney Future.ttf");

    // UI Sounds
    load_snd(SND_CLICK, std::string(UI_BASE) + "Sounds/click-a.ogg");

    // Music
    load_mus(MUSIC_MENU,  std::string(NINJA_BASE) + "audio/music/theme_dream.ogg");
    load_mus(MUSIC_PLAIN, std::string(NINJA_BASE) + "audio/music/theme_plain.ogg");
    load_mus(MUSIC_SWAMP, std::string(NINJA_BASE) + "audio/music/theme_swamp.ogg");
    load_mus(MUSIC_BOSS,  std::string(NINJA_BASE) + "audio/music/theme_lost_village.ogg");
}

} // namespace ls
