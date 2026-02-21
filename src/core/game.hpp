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
#include "managers/sound_manager.hpp"
#include "ai/pathfinding.hpp"

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
    Difficulty difficulty{Difficulty::Normal};
    bool running{true};
    std::string save_path{"save.json"};
    std::optional<SaveData> pending_load;

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
        auto mp = GetMousePosition();
        return Vec2::from_raylib(mp);
    }

    GridPos mouse_grid() const {
        return current_map.world_to_grid(mouse_world());
    }
};

} // namespace ls
