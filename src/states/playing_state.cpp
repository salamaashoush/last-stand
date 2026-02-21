#include "playing_state.hpp"
#include "core/asset_paths.hpp"
#include "core/biome_theme.hpp"
#include "core/game.hpp"
#include "factory/hero_factory.hpp"
#include "factory/tower_factory.hpp"
#include "systems/systems.hpp"
#include <cmath>
#include <format>

namespace ls {

// EnTT dispatcher with bound instance: instance is passed first, then event
static void on_enemy_death(Game& g, const EnemyDeathEvent& evt) {
    Gold reward = evt.reward;
    // Apply difficulty gold modifier
    if (g.difficulty == Difficulty::Easy)
        reward = static_cast<Gold>(reward * 1.2f);
    else if (g.difficulty == Difficulty::Hard)
        reward = static_cast<Gold>(reward * 0.8f);

    g.play.total_kills++;
    g.play.stats.total_kills++;

    // Spawn coin pickup at death position
    auto coin = g.registry.create();
    g.registry.emplace<Transform>(coin, evt.position);
    g.registry.emplace<Sprite>(coin, GOLD, 5, 20.0f, 20.0f, true, std::string(assets::COIN_SPRITE));
    g.registry.emplace<Coin>(coin, reward, 0.0f, 24.0f);
    g.registry.emplace<Lifetime>(coin, 15.0f); // coins disappear after 15 seconds

    auto heroes = g.registry.view<Hero>();
    for (auto [e, hero] : heroes.each()) {
        hero.xp += evt.reward / 2;
    }
}

static void on_enemy_reached_exit(Game& g, const EnemyReachedExitEvent& evt) {
    g.play.lives -= evt.damage;
    if (g.play.lives <= 0) {
        g.play.lives = 0;
        g.dispatcher.trigger(GameOverEvent{});
    }
}

static void on_game_over(Game& g, const GameOverEvent&) {
    g.state_machine.change_state(GameStateId::GameOver, g);
}

static void on_victory(Game& g, const VictoryEvent&) {
    g.state_machine.change_state(GameStateId::Victory, g);
}

static void on_wave_start(Game& g, const WaveStartEvent& evt) {
    g.sounds.play(g.sounds.wave_start);

    auto& wave = g.wave_manager.get_wave(evt.wave);
    if (wave.is_boss_wave) {
        g.play.banner = {"!! BOSS WAVE !!", 3.0f, RED, true};
    } else {
        g.play.banner = {std::format("Wave {}", evt.wave), 2.0f, WHITE, true};
    }

    // Tutorial: advance on wave start
    if (g.play.tutorial.active && g.play.tutorial.step == 2) {
        g.play.tutorial.step = 3;
    }
}

static void on_wave_complete(Game& g, const WaveCompleteEvent&) {
    // Tutorial: advance on wave complete
    if (g.play.tutorial.active && g.play.tutorial.step == 3) {
        g.play.tutorial.step = 4;
    }
}

static void on_enemy_death_tutorial(Game& g, const EnemyDeathEvent&) {
    // Tutorial: advance from step 0 (fight to earn gold) to step 1 (place tower)
    if (g.play.tutorial.active && g.play.tutorial.step == 0) {
        g.play.tutorial.step = 1;
    }
}

static void on_tower_placed(Game& g, const TowerPlacedEvent&) {
    // Tutorial: advance from step 1 (place tower) to step 2 (abilities)
    if (g.play.tutorial.active && g.play.tutorial.step == 1) {
        g.play.tutorial.step = 2;
    }
}

void PlayingState::enter(Game& game) {
    // Reset play state
    game.play = PlayState{};
    game.registry.clear();
    game.recalculate_path();
    game.state_machine.set_active_game(true);

    // Initialize sounds if not already done
    if (!game.sounds.initialized_) {
        game.sounds.init();
    }

    // Apply difficulty modifiers (all start 0 gold - earn by fighting)
    switch (game.difficulty) {
    case Difficulty::Easy:
        game.play.gold = 0;
        game.play.lives = 30;
        break;
    case Difficulty::Normal:
        game.play.gold = 0;
        game.play.lives = STARTING_LIVES;
        break;
    case Difficulty::Hard:
        game.play.gold = 0;
        game.play.lives = 10;
        break;
    }

    // Create hero at spawn
    auto spawn_world = game.current_map.grid_to_world(game.current_map.spawn);
    game.play.hero = create_hero(game.registry, spawn_world);

    // Apply upgrade bonuses
    if (game.upgrades.bonus_hp() > 0) {
        auto& hp = game.registry.get<Health>(game.play.hero);
        hp.max += game.upgrades.bonus_hp();
        hp.current = hp.max;
    }

    // Restore from save if available
    if (game.pending_load) {
        auto& save = *game.pending_load;
        game.play.gold = save.gold;
        game.play.lives = save.lives;
        game.play.current_wave = save.current_wave;

        // Restore hero stats
        auto& hero = game.registry.get<Hero>(game.play.hero);
        hero.level = save.hero_level;
        hero.xp = save.hero_xp;
        hero.xp_to_next = HERO_XP_PER_LEVEL * hero.level;
        auto& hp = game.registry.get<Health>(game.play.hero);
        hp.max = HERO_BASE_HP + (hero.level - 1) * 20;
        hp.current = hp.max;

        // Restore towers
        for (auto& ts : save.towers) {
            auto& stats = game.tower_registry.get(ts.type, ts.level);
            create_tower(game.registry, stats, ts.pos, game.current_map);
            game.play.tower_positions.insert(ts.pos);
        }

        game.pending_load = std::nullopt;
        // Skip tutorial on load
        game.play.tutorial.completed = true;
    }

    // Initialize camera
    game.camera.offset = {SCREEN_WIDTH / 2.0f, SCREEN_HEIGHT / 2.0f};
    game.camera.target = {spawn_world.x, spawn_world.y};
    game.camera.rotation = 0.0f;
    game.camera.zoom = 1.0f;

    // Generate decorations
    game.current_map.generate_decorations();

    setup_event_handlers(game);

    // Start gameplay music (biome-specific)
    auto& theme = get_biome_theme(game.current_map.name);
    Music* gameplay_music = game.assets.get_music(theme.music_track);
    if (gameplay_music) {
        if (game.current_music) StopMusicStream(*game.current_music);
        PlayMusicStream(*gameplay_music);
        SetMusicVolume(*gameplay_music, game.music_muted ? 0.0f : game.music_volume);
        game.current_music = gameplay_music;
    }
}

void PlayingState::exit(Game& game) {
    game.dispatcher.clear();
}

void PlayingState::setup_event_handlers(Game& game) {
    game.dispatcher.sink<EnemyDeathEvent>().connect<&on_enemy_death>(game);
    game.dispatcher.sink<EnemyReachedExitEvent>().connect<&on_enemy_reached_exit>(game);
    game.dispatcher.sink<GameOverEvent>().connect<&on_game_over>(game);
    game.dispatcher.sink<VictoryEvent>().connect<&on_victory>(game);
    game.dispatcher.sink<WaveStartEvent>().connect<&on_wave_start>(game);
    game.dispatcher.sink<WaveCompleteEvent>().connect<&on_wave_complete>(game);
    game.dispatcher.sink<EnemyDeathEvent>().connect<&on_enemy_death_tutorial>(game);
    game.dispatcher.sink<TowerPlacedEvent>().connect<&on_tower_placed>(game);
}

void PlayingState::handle_input(Game& game) {
    auto& ps = game.play;

    // Pause
    if (IsKeyPressed(KEY_P) || IsKeyPressed(KEY_ESCAPE)) {
        game.state_machine.change_state(GameStateId::Paused, game);
        return;
    }

    // Speed toggle
    if (IsKeyPressed(KEY_F)) {
        ps.game_speed_fast = !ps.game_speed_fast;
    }

    // Tutorial dismiss
    if (IsKeyPressed(KEY_TAB) && ps.tutorial.active) {
        ps.tutorial.step++;
        if (ps.tutorial.step > 4) {
            ps.tutorial.completed = true;
            ps.tutorial.active = false;
        }
    }

    // Music controls
    if (IsKeyPressed(KEY_M)) {
        game.music_muted = !game.music_muted;
        if (game.current_music) {
            SetMusicVolume(*game.current_music, game.music_muted ? 0.0f : game.music_volume);
        }
    }
    if (IsKeyPressed(KEY_EQUAL) || IsKeyPressed(KEY_KP_ADD)) {
        game.music_volume = std::min(1.0f, game.music_volume + 0.1f);
        if (game.current_music && !game.music_muted) SetMusicVolume(*game.current_music, game.music_volume);
    }
    if (IsKeyPressed(KEY_MINUS) || IsKeyPressed(KEY_KP_SUBTRACT)) {
        game.music_volume = std::max(0.0f, game.music_volume - 0.1f);
        if (game.current_music && !game.music_muted) SetMusicVolume(*game.current_music, game.music_volume);
    }

    // Start wave early
    if (IsKeyPressed(KEY_SPACE) && !ps.wave_active && ps.current_wave < MAX_WAVES) {
        ps.wave_timer = 0.0f;
    }

    // Tower placement / selection via mouse click
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        auto gp = game.mouse_grid();
        auto mp = GetMousePosition();

        // Check if clicking on the right panel or tower popover
        if (mp.x >= SCREEN_WIDTH - PANEL_WIDTH) return;
        if (mp.y < HUD_HEIGHT) return;
        if (ps.popover_rect.width > 0 && CheckCollisionPointRec(mp, ps.popover_rect)) return;

        if (ps.placing_tower.has_value()) {
            // Place tower
            if (game.can_place_tower(gp)) {
                auto& stats = game.tower_registry.get(*ps.placing_tower, 1);
                if (ps.gold >= stats.cost) {
                    ps.gold -= stats.cost;
                    ps.stats.gold_spent += stats.cost;
                    ps.stats.towers_built++;
                    auto e = create_tower(game.registry, stats, gp, game.current_map);
                    ps.tower_positions.insert(gp);
                    game.recalculate_path();
                    game.dispatcher.trigger(TowerPlacedEvent{e, *ps.placing_tower, gp});
                    ps.placing_tower = std::nullopt;
                    game.sounds.play(game.sounds.tower_place);
                }
            }
        } else {
            // Try to select a tower
            ps.selected_tower = entt::null;
            auto towers = game.registry.view<Tower, GridCell>();
            for (auto [e, tower, gc] : towers.each()) {
                if (gc.pos == gp) {
                    ps.selected_tower = e;
                    game.sounds.play(game.sounds.ui_click);
                    break;
                }
            }
        }
    }

    // Cancel placement
    if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
        ps.placing_tower = std::nullopt;
        ps.selected_tower = entt::null;
    }

    // Tower hotkeys (1-6)
    TowerType hotkey_towers[] = {TowerType::Arrow,     TowerType::Cannon, TowerType::Ice,
                                 TowerType::Lightning, TowerType::Poison, TowerType::Laser};
    for (int i = 0; i < 6; ++i) {
        if (IsKeyPressed(KEY_ONE + i)) {
            auto& stats = game.tower_registry.get(hotkey_towers[i], 1);
            if (ps.gold >= stats.cost) {
                ps.placing_tower = hotkey_towers[i];
                ps.selected_tower = entt::null;
                game.sounds.play(game.sounds.ui_click);
            }
        }
    }

    // Save game
    if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_S)) {
        SaveData data;
        data.map_name = game.current_map.name;
        data.gold = ps.gold;
        data.lives = ps.lives;
        data.current_wave = ps.current_wave;

        auto heroes = game.registry.view<Hero>();
        for (auto [e, hero] : heroes.each()) {
            data.hero_level = hero.level;
            data.hero_xp = hero.xp;
        }

        auto towers = game.registry.view<Tower, GridCell>();
        for (auto [e, tower, gc] : towers.each()) {
            data.towers.push_back({tower.type, tower.level, gc.pos});
        }

        game.save_manager.save(data, game.save_path);
    }
}

void PlayingState::update(Game& game, float dt) {
    handle_input(game);

    float speed = game.play.game_speed_fast ? 2.0f : 1.0f;
    float scaled_dt = dt * speed;

    // Update stats timer
    game.play.stats.time_elapsed += scaled_dt;

    // Update screen shake
    if (game.play.shake_timer > 0) {
        game.play.shake_timer -= dt; // real-time, not scaled
        float intensity = game.play.shake_intensity * (game.play.shake_timer / 0.4f);
        game.play.shake_offset.x = static_cast<float>(GetRandomValue(-100, 100)) / 100.0f * intensity;
        game.play.shake_offset.y = static_cast<float>(GetRandomValue(-100, 100)) / 100.0f * intensity;
    } else {
        game.play.shake_offset = {};
    }

    // Update wave banner
    if (game.play.banner.active) {
        game.play.banner.timer -= dt;
        if (game.play.banner.timer <= 0) {
            game.play.banner.active = false;
        }
    }

    // Clean up dead entities
    {
        auto view = game.registry.view<Dead>();
        std::vector<entt::entity> dead;
        for (auto e : view) {
            dead.push_back(e);
        }
        for (auto e : dead) {
            if (game.registry.valid(e)) {
                // Don't destroy hero
                if (!game.registry.all_of<Hero>(e)) {
                    game.registry.destroy(e);
                }
            }
        }
    }

    // Update music stream
    if (game.current_music) UpdateMusicStream(*game.current_music);

    // Switch to boss music during boss waves (only if biome doesn't already use boss music)
    {
        auto& biome = get_biome_theme(game.current_map.name);
        Music* biome_music = game.assets.get_music(biome.music_track);
        Music* boss_music = game.assets.get_music(assets::MUSIC_BOSS);
        bool is_boss_wave =
            game.play.wave_active && game.play.current_wave > 0 && (game.play.current_wave % BOSS_WAVE_INTERVAL == 0);
        // Only switch to boss music if the biome's normal track is different from boss
        if (biome_music != boss_music) {
            if (is_boss_wave && boss_music && game.current_music != boss_music) {
                if (game.current_music) StopMusicStream(*game.current_music);
                PlayMusicStream(*boss_music);
                SetMusicVolume(*boss_music, game.music_muted ? 0.0f : game.music_volume);
                game.current_music = boss_music;
            } else if (!is_boss_wave && biome_music && game.current_music != biome_music &&
                       game.current_music == boss_music) {
                StopMusicStream(*boss_music);
                PlayMusicStream(*biome_music);
                SetMusicVolume(*biome_music, game.music_muted ? 0.0f : game.music_volume);
                game.current_music = biome_music;
            }
        }
    }

    // Camera follow hero
    if (game.play.hero != entt::null && game.registry.valid(game.play.hero)) {
        auto& htf = game.registry.get<Transform>(game.play.hero);
        float world_w = static_cast<float>(GRID_COLS * TILE_SIZE);
        float world_h = static_cast<float>(GRID_ROWS * TILE_SIZE);
        float half_w = SCREEN_WIDTH / 2.0f;
        float half_h = SCREEN_HEIGHT / 2.0f;
        game.camera.target.x = std::clamp(htf.position.x, half_w, world_w - half_w);
        game.camera.target.y = std::clamp(htf.position.y, half_h, world_h - half_h);
    }

    systems::hero_system(game, scaled_dt);
    systems::enemy_spawn_system(game, scaled_dt);
    systems::path_follow_system(game, scaled_dt);
    systems::boss_system(game, scaled_dt);
    systems::movement_system(game, scaled_dt);
    systems::body_collision_system(game, scaled_dt);
    systems::enemy_combat_system(game, scaled_dt);
    systems::tower_targeting_system(game, scaled_dt);
    systems::tower_attack_system(game, scaled_dt);
    systems::projectile_system(game, scaled_dt);
    systems::aura_system(game, scaled_dt);
    systems::effect_system(game, scaled_dt);
    systems::health_system(game, scaled_dt);
    systems::tower_health_system(game, scaled_dt);
    systems::collision_system(game, scaled_dt);
    systems::lifetime_system(game, scaled_dt);
    systems::particle_system(game, scaled_dt);
    systems::coin_system(game, scaled_dt);
    systems::animated_sprite_system(game, scaled_dt);
}

void PlayingState::render(Game& game) {
    ClearBackground(get_biome_theme(game.current_map.name).bg_color);

    // Camera2D follows hero with shake offset
    Camera2D cam = game.camera;
    cam.target.x += game.play.shake_offset.x;
    cam.target.y += game.play.shake_offset.y;

    BeginMode2D(cam);
    systems::render_system(game);
    EndMode2D();

    // UI renders outside camera (screen space)
    systems::ui_system(game);
}

} // namespace ls
