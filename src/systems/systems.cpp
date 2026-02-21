#include "systems.hpp"
#include "components/components.hpp"
#include "core/asset_paths.hpp"
#include "core/biome_theme.hpp"
#include "core/game.hpp"
#include "factory/enemy_factory.hpp"
#include "factory/hero_factory.hpp"
#include "factory/projectile_factory.hpp"
#include "factory/tower_factory.hpp"
#include <algorithm>
#include <cmath>
#include <format>
#include <raylib.h>

namespace ls::systems {

// Helper: draw a texture scaled to a destination rect with optional rotation and tint
static void draw_tex(Texture2D* tex, float x, float y, float w, float h, float rot, Color tint) {
    if (!tex) return;
    Rectangle src = {0, 0, static_cast<float>(tex->width), static_cast<float>(tex->height)};
    Rectangle dst = {x, y, w, h};
    Vector2 origin = {w / 2.0f, h / 2.0f};
    DrawTexturePro(*tex, src, dst, origin, rot, tint);
}

// Helper: draw a texture from a spritesheet sub-rect
static void draw_tex_src(Texture2D* tex, Rectangle srcRect, float x, float y, float w, float h, float rot, Color tint) {
    if (!tex) return;
    Rectangle dst = {x, y, w, h};
    Vector2 origin = {w / 2.0f, h / 2.0f};
    DrawTexturePro(*tex, srcRect, dst, origin, rot, tint);
}

// Helper: get angle in degrees from direction vector
static float angle_from_dir(Vec2 dir) {
    if (dir.length() < 0.01f) return 0.0f;
    return std::atan2(dir.y, dir.x) * RAD2DEG;
}

// Helper: DrawTextEx with the game font, falling back to DrawText
static void draw_text(AssetManager& a, const char* text, float x, float y, float size, Color color) {
    Font* font = a.get_font(assets::FONT_MAIN);
    if (font) {
        DrawTextEx(*font, text, {x, y}, size, 1.0f, color);
    } else {
        DrawText(text, static_cast<int>(x), static_cast<int>(y), static_cast<int>(size), color);
    }
}

// Helper: MeasureTextEx with font fallback
static float measure_text(AssetManager& a, const char* text, float size) {
    Font* font = a.get_font(assets::FONT_MAIN);
    if (font) {
        return MeasureTextEx(*font, text, size, 1.0f).x;
    }
    return static_cast<float>(MeasureText(text, static_cast<int>(size)));
}

// ============================================================
// 1. Hero System - WASD movement, auto-attack, abilities
// ============================================================
void hero_system(Game& game, float dt) {
    auto& reg = game.registry;
    auto view = reg.view<Hero, Transform, Health>();

    for (auto [e, hero, tf, hp] : view.each()) {
        // WASD movement - set velocity so animated_sprite_system can detect direction
        Vec2 move{};
        if (IsKeyDown(KEY_W)) move.y -= 1;
        if (IsKeyDown(KEY_S)) move.y += 1;
        if (IsKeyDown(KEY_A)) move.x -= 1;
        if (IsKeyDown(KEY_D)) move.x += 1;

        auto& vel = reg.get<Velocity>(e);
        if (move.length() > 0.01f) {
            vel.vel = move.normalized() * HERO_SPEED;
        } else {
            vel.vel = {};
        }

        // Clamp position to world bounds
        float world_w = static_cast<float>(GRID_COLS * TILE_SIZE);
        float world_h = static_cast<float>(GRID_ROWS * TILE_SIZE);
        tf.position.x = std::clamp(tf.position.x, 0.0f, world_w);
        tf.position.y = std::clamp(tf.position.y, 0.0f, world_h);

        // Auto-attack nearest enemy
        hero.attack_cooldown -= dt;
        if (hero.attack_cooldown <= 0.0f) {
            entt::entity nearest = entt::null;
            float best_dist = HERO_ATTACK_RANGE + game.upgrades.bonus_range();

            auto enemies = reg.view<Enemy, Transform, Health>();
            for (auto [ee, en, etf, ehp] : enemies.each()) {
                if (reg.all_of<Dead>(ee)) continue;
                float d = tf.position.distance_to(etf.position);
                if (d < best_dist) {
                    best_dist = d;
                    nearest = ee;
                }
            }

            if (nearest != entt::null) {
                auto& etf = reg.get<Transform>(nearest);
                int dmg = HERO_BASE_DAMAGE + game.upgrades.bonus_damage() + hero.level * 3;
                hero.attack_cooldown = std::max(0.1f, HERO_ATTACK_COOLDOWN - game.upgrades.bonus_cooldown());

                // Fire a visible projectile at the enemy
                auto proj_e = create_projectile(reg, tf.position, nearest, etf.position, dmg, DamageType::Physical,
                                                400.0f, 0, EffectType::None, 0.0f, 0, {100, 200, 255, 255});

                // Set projectile sprite
                if (reg.valid(proj_e) && reg.all_of<Sprite>(proj_e)) {
                    auto& proj_spr = reg.get<Sprite>(proj_e);
                    proj_spr.texture_name = assets::PROJ_ARROW;
                    proj_spr.width = 14.0f;
                    proj_spr.height = 14.0f;
                    proj_spr.color = {100, 200, 255, 255};
                }

                game.sounds.play(game.sounds.arrow_fire, 0.4f);
            }
        }

        // Abilities
        for (auto& ab : hero.abilities) {
            ab.timer = std::max(0.0f, ab.timer - dt);
        }

        // Q - Fireball
        if (IsKeyPressed(KEY_Q) && hero.abilities[0].ready()) {
            auto& ab = hero.abilities[0];
            ab.timer = ab.cooldown;
            game.sounds.play(game.sounds.hero_ability);
            auto enemies = reg.view<Enemy, Transform, Health>();
            for (auto [ee, en, etf, ehp] : enemies.each()) {
                if (reg.all_of<Dead>(ee)) continue;
                if (tf.position.distance_to(etf.position) <= ab.radius) {
                    int dmg = ab.damage + hero.level * 5;
                    int actual = std::max(1, dmg - ehp.armor);
                    ehp.current -= actual;
                    create_floating_text(reg, etf.position, std::to_string(actual), {255, 100, 0, 255});
                    // Fire particles
                    for (int i = 0; i < 5; ++i) {
                        float angle = static_cast<float>(GetRandomValue(0, 360)) * DEG2RAD;
                        float spd = static_cast<float>(GetRandomValue(30, 80));
                        create_particle(reg, etf.position, {std::cos(angle) * spd, std::sin(angle) * spd},
                                        {255, static_cast<unsigned char>(GetRandomValue(50, 200)), 0, 255}, 6.0f, 0.5f,
                                        assets::PART_FLAME);
                    }
                }
            }
        }

        // E - Heal Aura
        if (IsKeyPressed(KEY_E) && hero.abilities[1].ready()) {
            auto& ab = hero.abilities[1];
            ab.timer = ab.cooldown;
            game.sounds.play(game.sounds.hero_ability);
            hp.current = std::min(hp.max, hp.current + 50 + hero.level * 10);
            create_floating_text(reg, tf.position, "+" + std::to_string(50 + hero.level * 10), GREEN);
            for (int i = 0; i < 8; ++i) {
                float angle = static_cast<float>(i) / 8.0f * 2.0f * PI;
                float spd = 50.0f;
                create_particle(reg, tf.position, {std::cos(angle) * spd, std::sin(angle) * spd}, GREEN, 5.0f, 0.8f,
                                assets::PART_MAGIC);
            }
        }

        // R - Lightning Strike (at mouse)
        if (IsKeyPressed(KEY_R) && hero.abilities[2].ready()) {
            auto& ab = hero.abilities[2];
            ab.timer = ab.cooldown;
            game.sounds.play(game.sounds.hero_ability);
            Vec2 target = game.mouse_world();
            auto enemies = reg.view<Enemy, Transform, Health>();
            for (auto [ee, en, etf, ehp] : enemies.each()) {
                if (reg.all_of<Dead>(ee)) continue;
                if (target.distance_to(etf.position) <= ab.radius) {
                    int dmg = ab.damage + hero.level * 8;
                    int actual = std::max(1, dmg - ehp.armor);
                    ehp.current -= actual;
                    create_floating_text(reg, etf.position, std::to_string(actual), {255, 255, 100, 255});
                }
            }
            // Lightning particles
            for (int i = 0; i < 12; ++i) {
                float angle = static_cast<float>(GetRandomValue(0, 360)) * DEG2RAD;
                float spd = static_cast<float>(GetRandomValue(40, 100));
                create_particle(reg, target, {std::cos(angle) * spd, std::sin(angle) * spd},
                                {255, 255, static_cast<unsigned char>(GetRandomValue(100, 255)), 255}, 4.0f, 0.4f,
                                assets::PART_SPARK);
            }
        }

        // XP/Level up
        if (hero.xp >= hero.xp_to_next) {
            hero.xp -= hero.xp_to_next;
            hero.level++;
            hero.xp_to_next = HERO_XP_PER_LEVEL * hero.level;
            hp.max += 20;
            hp.current = hp.max;
            game.dispatcher.trigger(HeroLevelUpEvent{hero.level});
            create_floating_text(reg, tf.position, "LEVEL UP!", GOLD);
        }

        // Hero respawn
        if (hp.current <= 0) {
            hp.current = hp.max;
            tf.position = game.current_map.grid_to_world(game.current_map.spawn);
            create_floating_text(reg, tf.position, "RESPAWN", WHITE);
            game.play.stats.hero_deaths++;
        }
    }
}

// ============================================================
// 2. Enemy Spawn System
// ============================================================
void enemy_spawn_system(Game& game, float dt) {
    auto& ps = game.play;

    if (!ps.wave_active) {
        ps.wave_timer -= dt;
        if (ps.wave_timer <= 0.0f) {
            ps.current_wave++;
            if (ps.current_wave > game.wave_manager.total_waves()) {
                game.dispatcher.trigger(VictoryEvent{});
                return;
            }
            ps.wave_active = true;
            ps.spawn_index = 0;
            ps.spawn_sub_index = 0;
            ps.spawn_timer = 0.0f;
            game.dispatcher.trigger(WaveStartEvent{ps.current_wave});
        }
        return;
    }

    auto& wave = game.wave_manager.get_wave(ps.current_wave);
    if (ps.spawn_index >= wave.spawns.size()) {
        // All spawned for this wave — immediately start next wave
        game.dispatcher.trigger(WaveCompleteEvent{ps.current_wave});
        ps.current_wave++;
        if (ps.current_wave > game.wave_manager.total_waves()) {
            // Final victory only when all enemies dead after last wave
            if (ps.enemies_alive <= 0) {
                game.dispatcher.trigger(VictoryEvent{});
            }
            return;
        }
        ps.spawn_index = 0;
        ps.spawn_sub_index = 0;
        ps.spawn_timer = 1.5f; // Brief gap between waves
        game.dispatcher.trigger(WaveStartEvent{ps.current_wave});
        return;
    }

    ps.spawn_timer -= dt;
    if (ps.spawn_timer <= 0.0f) {
        auto& entry = wave.spawns[ps.spawn_index];
        float scaling = game.wave_manager.scaling(ps.current_wave);

        // Apply difficulty scaling
        if (game.difficulty == Difficulty::Easy)
            scaling *= 0.8f;
        else if (game.difficulty == Difficulty::Hard)
            scaling *= 1.3f;

        // Flying enemies use flying_path (shortcut)
        auto& path = (entry.type == EnemyType::Flying && !ps.flying_path.empty()) ? ps.flying_path : ps.enemy_path;

        if (!path.empty()) {
            create_enemy(game.registry, entry.type, path, scaling, ps.current_wave);
            ps.enemies_alive++;
        }

        ps.spawn_sub_index++;
        ps.spawn_timer = entry.delay;

        if (ps.spawn_sub_index >= static_cast<size_t>(entry.count)) {
            ps.spawn_index++;
            ps.spawn_sub_index = 0;
        }
    }
}

// ============================================================
// 3. Path Follow System
// ============================================================
void path_follow_system(Game& game, [[maybe_unused]] float dt) {
    auto& reg = game.registry;
    auto view = reg.view<PathFollower, Transform, Velocity>();

    for (auto [e, pf, tf, vel] : view.each()) {
        if (reg.all_of<Dead>(e)) continue;
        if (pf.current_index >= pf.path.size()) continue;

        Vec2 target = pf.path[pf.current_index];
        Vec2 dir = target - tf.position;
        float dist = dir.length();

        // Check for slow effect
        float speed = pf.speed;
        if (reg.all_of<Effect>(e)) {
            auto& eff = reg.get<Effect>(e);
            if (eff.type == EffectType::Slow) speed *= eff.slow_factor;
            if (eff.type == EffectType::Stun) speed = 0.0f;
        }

        if (dist < 4.0f) {
            pf.current_index++;
        } else {
            vel.vel = dir.normalized() * speed;
        }
    }
}

// ============================================================
// 4. Movement System
// ============================================================
void movement_system(Game& game, float dt) {
    auto view = game.registry.view<Transform, Velocity>();
    for (auto [e, tf, vel] : view.each()) {
        tf.position = tf.position + vel.vel * dt;
    }
}

// ============================================================
// 5. Tower Targeting System
// ============================================================
void tower_targeting_system(Game& game, [[maybe_unused]] float dt) {
    auto& reg = game.registry;
    auto towers = reg.view<Tower, Transform>();
    auto enemies = reg.view<Enemy, Transform, Health>();

    for (auto [te, tower, ttf] : towers.each()) {
        tower.target = entt::null;
        float best_dist = tower.range;

        for (auto [ee, en, etf, ehp] : enemies.each()) {
            if (reg.all_of<Dead>(ee)) continue;
            float d = ttf.position.distance_to(etf.position);
            if (d < best_dist) {
                best_dist = d;
                tower.target = ee;
            }
        }
    }
}

// ============================================================
// 6. Tower Attack System
// ============================================================
void tower_attack_system(Game& game, float dt) {
    auto& reg = game.registry;
    auto view = reg.view<Tower, Transform>();

    for (auto [e, tower, tf] : view.each()) {
        // Tick attack flash
        if (reg.all_of<AttackFlash>(e)) {
            auto& flash = reg.get<AttackFlash>(e);
            flash.timer -= dt;
            if (flash.timer <= 0.0f) reg.remove<AttackFlash>(e);
        }

        tower.cooldown -= dt;
        if (tower.cooldown > 0.0f) continue;
        if (tower.target == entt::null || !reg.valid(tower.target)) continue;
        if (reg.all_of<Dead>(tower.target)) continue;

        auto& target_tf = reg.get<Transform>(tower.target);

        // Laser tower does continuous damage
        if (tower.type == TowerType::Laser) {
            tower.cooldown = tower.fire_rate;
            game.sounds.play(game.sounds.laser_hum, 0.3f);
            if (reg.all_of<Health>(tower.target)) {
                auto& hp = reg.get<Health>(tower.target);
                int actual = std::max(1, tower.damage - hp.armor);
                hp.current -= actual;
                game.dispatcher.trigger(DamageDealtEvent{tower.target, actual, target_tf.position});
                // Apply burn effect
                if (tower.effect != EffectType::None) {
                    reg.emplace_or_replace<Effect>(tower.target, tower.effect, tower.effect_duration, 0.0f, 0.5f, 8,
                                                   1.0f);
                }
            }
        } else {
            tower.cooldown = 1.0f / tower.fire_rate;
            Color proj_color = WHITE;
            DamageType dtype = DamageType::Physical;

            // Play tower-specific fire sound
            switch (tower.type) {
            case TowerType::Arrow:
                proj_color = {200, 150, 50, 255};
                game.sounds.play(game.sounds.arrow_fire);
                break;
            case TowerType::Cannon:
                proj_color = {80, 80, 80, 255};
                dtype = DamageType::Physical;
                game.sounds.play(game.sounds.cannon_fire);
                break;
            case TowerType::Ice:
                proj_color = {100, 200, 255, 255};
                dtype = DamageType::Magic;
                game.sounds.play(game.sounds.ice_fire);
                break;
            case TowerType::Lightning:
                proj_color = {255, 255, 100, 255};
                dtype = DamageType::Magic;
                game.sounds.play(game.sounds.lightning_fire);
                break;
            case TowerType::Poison:
                proj_color = {100, 200, 50, 255};
                dtype = DamageType::Magic;
                game.sounds.play(game.sounds.poison_fire);
                break;
            default:
                break;
            }

            auto proj_e = create_projectile(reg, tf.position, tower.target, target_tf.position, tower.damage, dtype,
                                            PROJECTILE_SPEED, tower.aoe_radius, tower.effect, tower.effect_duration,
                                            tower.chain_count, proj_color);

            // Set projectile texture name
            if (reg.valid(proj_e) && reg.all_of<Sprite>(proj_e)) {
                auto& proj_spr = reg.get<Sprite>(proj_e);
                switch (tower.type) {
                case TowerType::Arrow:
                    proj_spr.texture_name = assets::PROJ_ARROW;
                    break;
                case TowerType::Cannon:
                    proj_spr.texture_name = assets::PROJ_CANNON;
                    break;
                case TowerType::Ice:
                    proj_spr.texture_name = assets::PROJ_ICE;
                    break;
                case TowerType::Lightning:
                    proj_spr.texture_name = assets::PROJ_LIGHTNING;
                    break;
                case TowerType::Poison:
                    proj_spr.texture_name = assets::PROJ_POISON;
                    break;
                default:
                    break;
                }
                // Make projectile sprites a bit bigger for visibility
                proj_spr.width = 12.0f;
                proj_spr.height = 12.0f;
            }

            // Attack flash
            reg.emplace_or_replace<AttackFlash>(e, 0.15f);
        }
    }
}

// ============================================================
// 7. Projectile System
// ============================================================
void projectile_system(Game& game, [[maybe_unused]] float dt) {
    auto& reg = game.registry;
    auto view = reg.view<Projectile, Transform, Velocity>();

    std::vector<entt::entity> to_destroy;

    for (auto [e, proj, tf, vel] : view.each()) {
        // Update target position if target still alive
        Vec2 target_pos = proj.target_pos;
        if (proj.target != entt::null && reg.valid(proj.target) && reg.all_of<Transform>(proj.target)) {
            target_pos = reg.get<Transform>(proj.target).position;
            proj.target_pos = target_pos;
        }

        Vec2 dir = target_pos - tf.position;
        float dist = dir.length();

        // Spawn trail particle
        create_particle(reg, tf.position, {0, 0}, proj.trail_color, 3.0f, 0.15f);

        if (dist < 12.0f) {
            // Hit!
            if (proj.aoe_radius > 0) {
                // AoE damage
                auto enemies = reg.view<Enemy, Transform, Health>();
                for (auto [ee, en, etf, ehp] : enemies.each()) {
                    if (reg.all_of<Dead>(ee)) continue;
                    if (tf.position.distance_to(etf.position) <= proj.aoe_radius) {
                        int actual = std::max(1, proj.damage - ehp.armor);
                        ehp.current -= actual;
                        create_floating_text(reg, etf.position, std::to_string(actual), RED);
                        if (proj.effect != EffectType::None) {
                            reg.emplace_or_replace<Effect>(
                                ee, proj.effect, proj.effect_duration, 0.0f, 0.5f,
                                proj.effect == EffectType::Poison ? 5 : (proj.effect == EffectType::Burn ? 8 : 0),
                                proj.effect == EffectType::Slow ? 0.5f : 1.0f);
                        }
                    }
                }
                // Explosion particles
                for (int i = 0; i < 8; ++i) {
                    float angle = static_cast<float>(GetRandomValue(0, 360)) * DEG2RAD;
                    float spd = static_cast<float>(GetRandomValue(30, 80));
                    create_particle(reg, tf.position, {std::cos(angle) * spd, std::sin(angle) * spd}, proj.trail_color,
                                    5.0f, 0.4f, assets::PART_FLAME);
                }
                // Screen shake for AoE
                game.play.shake_intensity = 3.0f;
                game.play.shake_timer = 0.15f;
                game.sounds.play(game.sounds.enemy_hit);
            } else {
                // Single target
                if (proj.target != entt::null && reg.valid(proj.target) && reg.all_of<Health>(proj.target)) {
                    auto& hp = reg.get<Health>(proj.target);
                    int actual = std::max(1, proj.damage - hp.armor);
                    hp.current -= actual;
                    auto& etf = reg.get<Transform>(proj.target);
                    create_floating_text(reg, etf.position, std::to_string(actual), RED);

                    if (proj.effect != EffectType::None) {
                        reg.emplace_or_replace<Effect>(
                            proj.target, proj.effect, proj.effect_duration, 0.0f, 0.5f,
                            proj.effect == EffectType::Poison ? 5 : (proj.effect == EffectType::Burn ? 8 : 0),
                            proj.effect == EffectType::Slow ? 0.5f : 1.0f);
                    }
                }

                // Chain lightning
                if (proj.chain_count > 0 && proj.target != entt::null) {
                    float chain_range = 100.0f;
                    entt::entity chain_target = entt::null;
                    float best = chain_range;
                    auto enemies = reg.view<Enemy, Transform, Health>();
                    for (auto [ee, en, etf, ehp] : enemies.each()) {
                        if (ee == proj.target || reg.all_of<Dead>(ee)) continue;
                        float d = tf.position.distance_to(etf.position);
                        if (d < best) {
                            best = d;
                            chain_target = ee;
                        }
                    }
                    if (chain_target != entt::null) {
                        auto& ctf = reg.get<Transform>(chain_target);
                        create_projectile(reg, tf.position, chain_target, ctf.position, proj.damage * 3 / 4,
                                          proj.damage_type, proj.speed * 1.5f, 0, proj.effect, proj.effect_duration,
                                          proj.chain_count - 1, proj.trail_color);
                    }
                }
                game.sounds.play(game.sounds.enemy_hit, 0.5f);
            }
            to_destroy.push_back(e);
        } else {
            vel.vel = dir.normalized() * proj.speed;
        }
    }

    for (auto e : to_destroy) {
        if (reg.valid(e)) reg.destroy(e);
    }
}

// ============================================================
// 8. Aura System
// ============================================================
void aura_system(Game& game, float dt) {
    auto& reg = game.registry;
    auto view = reg.view<Aura, Transform>();

    for (auto [e, aura, tf] : view.each()) {
        if (reg.all_of<Dead>(e)) continue;

        if (aura.heal_per_sec > 0) {
            // Heal nearby allies
            auto allies = reg.view<Enemy, Transform, Health>();
            for (auto [ae, an, atf, ahp] : allies.each()) {
                if (ae == e || reg.all_of<Dead>(ae)) continue;
                if (tf.position.distance_to(atf.position) <= aura.radius) {
                    ahp.current = std::min(ahp.max, ahp.current + static_cast<int>(aura.heal_per_sec * dt));
                }
            }
        }
    }
}

// ============================================================
// 9. Effect System
// ============================================================
void effect_system(Game& game, float dt) {
    auto& reg = game.registry;
    auto view = reg.view<Effect, Health, Transform>();

    std::vector<entt::entity> to_remove;

    for (auto [e, eff, hp, tf] : view.each()) {
        if (reg.all_of<Dead>(e)) continue;

        eff.duration -= dt;
        if (eff.duration <= 0.0f) {
            to_remove.push_back(e);
            continue;
        }

        // DoT ticks
        if (eff.tick_damage > 0) {
            eff.tick_timer -= dt;
            if (eff.tick_timer <= 0.0f) {
                eff.tick_timer = eff.tick_interval;
                hp.current -= eff.tick_damage;
                Color c = eff.type == EffectType::Poison ? Color{100, 200, 50, 255} : Color{255, 100, 0, 255};
                create_floating_text(reg, tf.position, std::to_string(eff.tick_damage), c);
            }
        }
    }

    for (auto e : to_remove) {
        if (reg.valid(e)) reg.remove<Effect>(e);
    }
}

// ============================================================
// 10. Damage System (event-driven floating text)
// ============================================================
void damage_system([[maybe_unused]] Game& game, [[maybe_unused]] float dt) {
    // Damage is applied directly in other systems
}

// ============================================================
// 11. Health System - Mark dead entities
// ============================================================
void health_system(Game& game, [[maybe_unused]] float dt) {
    auto& reg = game.registry;
    auto view = reg.view<Health>(entt::exclude<Dead, Hero, Tower>);

    for (auto [e, hp] : view.each()) {
        if (hp.current <= 0) {
            reg.emplace<Dead>(e);
            if (reg.all_of<Enemy>(e)) {
                auto& en = reg.get<Enemy>(e);
                auto& tf = reg.get<Transform>(e);
                game.dispatcher.trigger(EnemyDeathEvent{e, en.type, en.reward, tf.position});
                game.play.enemies_alive--;

                // Death particles
                auto& spr = reg.get<Sprite>(e);
                int count = (en.type == EnemyType::Boss) ? 20 : 8;
                for (int i = 0; i < count; ++i) {
                    float angle = static_cast<float>(GetRandomValue(0, 360)) * DEG2RAD;
                    float spd = static_cast<float>(GetRandomValue(40, 120));
                    create_particle(reg, tf.position, {std::cos(angle) * spd, std::sin(angle) * spd}, spr.color,
                                    (en.type == EnemyType::Boss) ? 6.0f : 4.0f, 0.6f, assets::PART_SMOKE);
                }
                // Gold text
                create_floating_text(reg, tf.position, "+" + std::to_string(en.reward) + "g", GOLD);

                // Sounds and shake for deaths
                if (en.type == EnemyType::Boss) {
                    game.sounds.play(game.sounds.boss_death);
                    game.play.shake_intensity = 8.0f;
                    game.play.shake_timer = 0.4f;
                    game.play.stats.boss_kills++;
                } else {
                    game.sounds.play(game.sounds.enemy_death, 0.5f);
                }
            }
        }
    }
}

// ============================================================
// 12. Economy System
// ============================================================
void economy_system([[maybe_unused]] Game& game, [[maybe_unused]] float dt) {}

// ============================================================
// 13. Collision System - Enemies reaching exit
// ============================================================
void collision_system(Game& game, [[maybe_unused]] float dt) {
    auto& reg = game.registry;
    auto view = reg.view<PathFollower, Transform, Enemy>();

    for (auto [e, pf, tf, en] : view.each()) {
        if (reg.all_of<Dead>(e)) continue;
        if (pf.current_index >= pf.path.size()) {
            game.dispatcher.trigger(EnemyReachedExitEvent{e, 1});
            reg.emplace_or_replace<Dead>(e);
            game.play.enemies_alive--;
        }
    }
}

// ============================================================
// 14. Lifetime System
// ============================================================
void lifetime_system(Game& game, float dt) {
    auto& reg = game.registry;
    auto view = reg.view<Lifetime>();

    std::vector<entt::entity> to_destroy;
    for (auto [e, lt] : view.each()) {
        lt.remaining -= dt;
        if (lt.remaining <= 0.0f) {
            to_destroy.push_back(e);
        }
    }

    for (auto e : to_destroy) {
        if (reg.valid(e)) reg.destroy(e);
    }
}

// ============================================================
// 15. Particle System
// ============================================================
void particle_system(Game& game, float dt) {
    auto& reg = game.registry;
    auto view = reg.view<Particle, Lifetime>();

    for (auto [e, p, lt] : view.each()) {
        p.decay = lt.remaining / 1.0f;
        p.size *= (1.0f - dt * 2.0f);
        if (p.size < 0.5f) p.size = 0.5f;
    }
}

// ============================================================
// Boss System - Boss abilities
// ============================================================
void boss_system(Game& game, float dt) {
    auto& reg = game.registry;
    auto view = reg.view<Boss, Enemy, Transform, Health>();

    for (auto [e, boss, en, tf, hp] : view.each()) {
        if (reg.all_of<Dead>(e)) continue;

        // Tick ability duration
        if (boss.ability_active) {
            boss.ability_duration -= dt;
            if (boss.ability_duration <= 0.0f) {
                boss.ability_active = false;
                // End SpeedBurst: restore speed
                if (boss.boss_ability == AbilityType::SpeedBurst && reg.all_of<PathFollower>(e)) {
                    auto& pf = reg.get<PathFollower>(e);
                    pf.speed = pf.base_speed;
                }
            }
            // DamageAura: continuous damage to hero
            if (boss.boss_ability == AbilityType::DamageAura) {
                auto heroes = reg.view<Hero, Transform, Health>();
                for (auto [he, hero, htf, hhp] : heroes.each()) {
                    if (tf.position.distance_to(htf.position) <= 120.0f) {
                        hhp.current -= static_cast<int>(5.0f * dt);
                    }
                }
            }
        }

        boss.ability_timer -= dt;
        if (boss.ability_timer <= 0.0f && !boss.ability_active) {
            boss.ability_timer = boss.ability_cooldown;

            switch (boss.boss_ability) {
            case AbilityType::SpeedBurst: {
                boss.ability_active = true;
                boss.ability_duration = 2.0f;
                if (reg.all_of<PathFollower>(e)) {
                    auto& pf = reg.get<PathFollower>(e);
                    pf.speed = pf.base_speed * 2.5f;
                }
                create_floating_text(reg, tf.position, "SPEED!", RED);
                // Red particles
                for (int i = 0; i < 6; ++i) {
                    float angle = static_cast<float>(GetRandomValue(0, 360)) * DEG2RAD;
                    create_particle(reg, tf.position, {std::cos(angle) * 40.0f, std::sin(angle) * 40.0f}, RED, 4.0f,
                                    0.5f, assets::PART_FLAME);
                }
                break;
            }
            case AbilityType::SpawnMinions: {
                create_floating_text(reg, tf.position, "SUMMON!", {255, 200, 50, 255});
                float scaling = game.wave_manager.scaling(game.play.current_wave);
                for (int i = 0; i < 3; ++i) {
                    // Create minions at boss position - need a small path from boss to exit
                    auto& pf = reg.get<PathFollower>(e);
                    std::vector<Vec2> minion_path;
                    minion_path.push_back(tf.position);
                    // Add remaining waypoints from boss's current path
                    for (size_t pi = pf.current_index; pi < pf.path.size(); ++pi) {
                        minion_path.push_back(pf.path[pi]);
                    }
                    if (minion_path.size() > 1) {
                        create_enemy(reg, EnemyType::Grunt, minion_path, scaling * 0.5f);
                        game.play.enemies_alive++;
                    }
                }
                break;
            }
            case AbilityType::DamageAura: {
                boss.ability_active = true;
                boss.ability_duration = 3.0f;
                create_floating_text(reg, tf.position, "AURA!", {255, 50, 50, 255});
                break;
            }
            }
        }
    }
}

// ============================================================
// Enemy Combat System - Enemies attack hero and towers
// ============================================================
void enemy_combat_system(Game& game, float dt) {
    auto& reg = game.registry;
    auto enemies = reg.view<Enemy, Transform>();

    for (auto [e, en, tf] : enemies.each()) {
        if (reg.all_of<Dead>(e)) continue;

        en.attack_timer -= dt;
        if (en.attack_timer > 0.0f) continue;

        // Attack hero if in range
        auto heroes = reg.view<Hero, Transform, Health>();
        for (auto [he, hero, htf, hhp] : heroes.each()) {
            float dist = tf.position.distance_to(htf.position);
            if (dist < en.attack_range + 12.0f) {
                int actual = std::max(1, en.attack_damage - hhp.armor);
                hhp.current -= actual;
                en.attack_timer = en.attack_cooldown;
                create_floating_text(reg, htf.position, "-" + std::to_string(actual), RED);
                // Hit particles
                float angle = static_cast<float>(GetRandomValue(0, 360)) * DEG2RAD;
                create_particle(reg, htf.position, {std::cos(angle) * 30.0f, std::sin(angle) * 30.0f}, RED, 3.0f, 0.2f,
                                assets::PART_SPARK);
                break; // Only attack one target per tick
            }
        }

        if (en.attack_timer > 0.0f) continue; // Already attacked hero

        // Tanks and bosses also attack towers in range
        if (en.type == EnemyType::Tank || en.type == EnemyType::Boss) {
            auto towers = reg.view<Tower, Transform, Health>();
            float best_dist = en.attack_range + 20.0f;
            entt::entity nearest_tower = entt::null;
            for (auto [te, tower, ttf, thp] : towers.each()) {
                if (reg.all_of<Dead>(te)) continue;
                float d = tf.position.distance_to(ttf.position);
                if (d < best_dist) {
                    best_dist = d;
                    nearest_tower = te;
                }
            }
            if (nearest_tower != entt::null) {
                auto& thp = reg.get<Health>(nearest_tower);
                auto& ttf = reg.get<Transform>(nearest_tower);
                int actual = en.attack_damage;
                thp.current -= actual;
                en.attack_timer = en.attack_cooldown;
                create_floating_text(reg, ttf.position, "-" + std::to_string(actual), {255, 100, 100, 255});
                // Spark
                create_particle(
                    reg, ttf.position,
                    {static_cast<float>(GetRandomValue(-30, 30)), static_cast<float>(GetRandomValue(-30, 30))},
                    {255, 200, 50, 255}, 3.0f, 0.3f);
            }
        }
    }
}

// ============================================================
// Body Collision System - Push hero and enemies apart
// ============================================================
void body_collision_system(Game& game, [[maybe_unused]] float dt) {
    auto& reg = game.registry;

    // Hero vs enemies
    auto heroes = reg.view<Hero, Transform, Sprite>();
    auto enemies = reg.view<Enemy, Transform, Sprite>();

    for (auto [he, hero, htf, hspr] : heroes.each()) {
        float hero_radius = 10.0f; // small collision radius for hero

        for (auto [ee, en, etf, espr] : enemies.each()) {
            if (reg.all_of<Dead>(ee)) continue;
            // Flying enemies don't collide with hero
            if (reg.all_of<Flying>(ee)) continue;

            float enemy_radius = en.collision_radius;
            float min_dist = hero_radius + enemy_radius;
            Vec2 diff = htf.position - etf.position;
            float dist = diff.length();

            if (dist < min_dist && dist > 0.01f) {
                // Push apart
                Vec2 push = diff.normalized() * ((min_dist - dist) * 0.5f);
                htf.position = htf.position + push;
                etf.position = etf.position - push;
            }
        }
    }

    // Enemy vs enemy (very light push - prevent exact overlap but don't disrupt path)
    auto all_enemies = reg.view<Enemy, Transform, Velocity>();
    for (auto [e1, en1, tf1, vel1] : all_enemies.each()) {
        if (reg.all_of<Dead>(e1)) continue;
        if (reg.all_of<Flying>(e1)) continue;
        for (auto [e2, en2, tf2, vel2] : all_enemies.each()) {
            if (e1 >= e2) continue; // avoid double-checking
            if (reg.all_of<Dead>(e2)) continue;
            if (reg.all_of<Flying>(e2)) continue;

            // Use smaller effective radius so enemies can pass each other
            float min_dist = (en1.collision_radius + en2.collision_radius) * 0.5f;
            Vec2 diff = tf1.position - tf2.position;
            float dist = diff.length();

            if (dist < min_dist && dist > 0.01f) {
                // Push perpendicular to average movement direction to avoid disrupting path
                Vec2 avg_dir = (vel1.vel + vel2.vel);
                Vec2 push_dir = diff.normalized();
                if (avg_dir.length() > 0.1f) {
                    avg_dir = avg_dir.normalized();
                    // Remove component along path direction
                    float along = push_dir.x * avg_dir.x + push_dir.y * avg_dir.y;
                    push_dir.x -= along * avg_dir.x;
                    push_dir.y -= along * avg_dir.y;
                    if (push_dir.length() > 0.01f)
                        push_dir = push_dir.normalized();
                    else
                        push_dir = diff.normalized();
                }
                Vec2 push = push_dir * ((min_dist - dist) * 0.15f);
                tf1.position = tf1.position + push;
                tf2.position = tf2.position - push;
            }
        }
    }
}

// ============================================================
// Tower Health System - Destroy towers when HP reaches 0
// ============================================================
void tower_health_system(Game& game, [[maybe_unused]] float dt) {
    auto& reg = game.registry;
    auto view = reg.view<Tower, Health, Transform>();

    std::vector<entt::entity> to_destroy;

    for (auto [e, tower, hp, tf] : view.each()) {
        if (hp.current <= 0) {
            to_destroy.push_back(e);
            // Destruction particles
            auto& spr = reg.get<Sprite>(e);
            for (int i = 0; i < 10; ++i) {
                float angle = static_cast<float>(GetRandomValue(0, 360)) * DEG2RAD;
                float spd = static_cast<float>(GetRandomValue(30, 80));
                create_particle(reg, tf.position, {std::cos(angle) * spd, std::sin(angle) * spd}, spr.color, 5.0f, 0.5f,
                                assets::PART_SMOKE);
            }
            create_floating_text(reg, tf.position, "DESTROYED!", RED);
            game.play.shake_intensity = 4.0f;
            game.play.shake_timer = 0.2f;
        }
    }

    for (auto e : to_destroy) {
        if (reg.valid(e)) {
            if (reg.all_of<GridCell>(e)) {
                auto& gc = reg.get<GridCell>(e);
                game.play.tower_positions.erase(gc.pos);
            }
            if (game.play.selected_tower == e) {
                game.play.selected_tower = entt::null;
            }
            reg.destroy(e);
            game.recalculate_path();
        }
    }
}

// ============================================================
// Animated Sprite System - Update animation frames
// ============================================================
void animated_sprite_system(Game& game, float dt) {
    auto& reg = game.registry;
    auto view = reg.view<AnimatedSprite, Transform>();

    for (auto [e, anim, tf] : view.each()) {
        if (!anim.playing || anim.anim_frames.empty()) continue;

        // Determine direction from velocity
        if (reg.all_of<Velocity>(e)) {
            auto& vel = reg.get<Velocity>(e);
            float vx = vel.vel.x, vy = vel.vel.y;
            if (std::abs(vx) > 1.0f || std::abs(vy) > 1.0f) {
                // Pick dominant direction
                if (std::abs(vy) >= std::abs(vx)) {
                    anim.direction = (vy > 0) ? 0 : 1; // down : up
                } else {
                    anim.direction = (vx < 0) ? 2 : 3; // left : right
                }
                // Advance animation
                anim.frame_timer += dt;
                float interval = 1.0f / anim.frame_speed;
                if (anim.frame_timer >= interval) {
                    anim.frame_timer -= interval;
                    anim.current_frame = (anim.current_frame + 1) % static_cast<int>(anim.anim_frames.size());
                }
            } else {
                // Idle: reset to first frame
                anim.current_frame = 0;
                anim.frame_timer = 0.0f;
            }
        }
    }
}

// ============================================================
// Coin Pickup System - Hero collects coins
// ============================================================
void coin_system(Game& game, float dt) {
    auto& reg = game.registry;
    auto heroes = reg.view<Hero, Transform>();
    auto coins = reg.view<Coin, Transform>();

    for (auto [he, hero, htf] : heroes.each()) {
        float pickup_radius = 60.0f + game.upgrades.bonus_pickup();
        float magnet_radius = pickup_radius * 2.0f;
        float pull_speed = 200.0f;

        // Magnet pull: coins within magnet_radius move toward hero
        for (auto [ce, coin, ctf] : coins.each()) {
            float dist = htf.position.distance_to(ctf.position);
            if (dist < magnet_radius && dist > pickup_radius) {
                Vec2 dir = (htf.position - ctf.position).normalized();
                ctf.position = ctf.position + dir * pull_speed * dt;
            }
        }

        std::vector<entt::entity> to_collect;
        for (auto [ce, coin, ctf] : coins.each()) {
            float dist = htf.position.distance_to(ctf.position);
            if (dist < pickup_radius) {
                to_collect.push_back(ce);
            }
        }
        for (auto ce : to_collect) {
            if (reg.valid(ce)) {
                auto& coin = reg.get<Coin>(ce);
                game.play.gold += coin.value;
                game.play.stats.gold_earned += coin.value;
                create_floating_text(reg, reg.get<Transform>(ce).position, "+" + std::to_string(coin.value) + "g",
                                     GOLD);
                game.sounds.play(game.sounds.ui_click, 0.6f);
                reg.destroy(ce);
            }
        }
    }

    // Bob animation for coins
    for (auto [ce, coin, ctf] : coins.each()) {
        coin.bob_timer += dt * 3.0f;
    }
}

// ============================================================
// 16. Render System
// ============================================================
void render_system(Game& game) {
    auto& reg = game.registry;
    auto& map = game.current_map;

    // Draw tiles (biome-aware)
    auto& theme = get_biome_theme(map.name);
    for (int y = 0; y < map.rows; ++y) {
        for (int x = 0; x < map.cols; ++x) {
            auto tile = map.tiles[y][x];
            const char* tex_name = nullptr;
            Color fallback;
            Color tint = WHITE;
            switch (tile) {
            case TileType::Grass:
                tex_name = theme.ground_tex;
                fallback = theme.ground_fallback;
                tint = theme.ground_tint;
                break;
            case TileType::Buildable:
                tex_name = assets::TILE_BUILDABLE;
                fallback = theme.ground_fallback;
                tint = theme.marker_tint;
                break;
            case TileType::Path:
                tex_name = theme.path_tex;
                fallback = theme.path_fallback;
                tint = theme.path_tint;
                break;
            case TileType::Spawn:
                tex_name = assets::TILE_SPAWN;
                fallback = theme.ground_fallback;
                tint = theme.marker_tint;
                break;
            case TileType::Exit:
                tex_name = assets::TILE_EXIT;
                fallback = theme.ground_fallback;
                tint = theme.marker_tint;
                break;
            case TileType::Blocked:
                tex_name = theme.blocked_tex;
                fallback = theme.blocked_fallback;
                tint = theme.blocked_tint;
                break;
            }
            float dx = static_cast<float>(GRID_OFFSET_X + x * TILE_SIZE) + TILE_SIZE / 2.0f;
            float dy = static_cast<float>(GRID_OFFSET_Y + y * TILE_SIZE) + TILE_SIZE / 2.0f;
            float ts = static_cast<float>(TILE_SIZE);
            // Draw ground base under overlay tiles (buildable, spawn, exit)
            if (tile == TileType::Buildable || tile == TileType::Spawn || tile == TileType::Exit) {
                Texture2D* ground_tex = game.assets.get_texture(theme.ground_tex);
                if (ground_tex) {
                    draw_tex(ground_tex, dx, dy, ts, ts, 0, theme.ground_tint);
                }
            }
            Texture2D* tex = tex_name ? game.assets.get_texture(tex_name) : nullptr;
            if (tex) {
                // Markers (buildable, spawn, exit) are semi-transparent overlays
                Color draw_tint = tint;
                if (tile == TileType::Buildable || tile == TileType::Spawn || tile == TileType::Exit) {
                    draw_tint.a = 100;
                }
                draw_tex(tex, dx, dy, ts, ts, 0, draw_tint);
            } else {
                DrawRectangle(GRID_OFFSET_X + x * TILE_SIZE, GRID_OFFSET_Y + y * TILE_SIZE, TILE_SIZE - 1,
                              TILE_SIZE - 1, fallback);
            }
        }
    }

    // Draw decorations
    {
        const char* deco_names[] = {assets::DECO_TREE_BIG, assets::DECO_BUSH,    assets::DECO_LEAF,
                                    assets::DECO_FLOWER,   assets::DECO_ROCK_SM, assets::DECO_ROCK_MD,
                                    assets::DECO_ROCK_LG,  assets::DECO_FLAME};
        for (auto& deco : map.decorations) {
            float dx = static_cast<float>(GRID_OFFSET_X + deco.pos.x * TILE_SIZE) + TILE_SIZE / 2.0f;
            float dy = static_cast<float>(GRID_OFFSET_Y + deco.pos.y * TILE_SIZE) + TILE_SIZE / 2.0f;
            Texture2D* tex = game.assets.get_texture(deco_names[deco.texture_index]);
            if (tex) {
                draw_tex(tex, dx, dy, 40.0f, 40.0f, 0, WHITE);
            }
        }
    }

    // Grid overlay for placement
    if (game.play.placing_tower.has_value()) {
        auto gp = game.mouse_grid();
        if (map.in_bounds(gp)) {
            float tx = static_cast<float>(GRID_OFFSET_X + gp.x * TILE_SIZE);
            float ty = static_cast<float>(GRID_OFFSET_Y + gp.y * TILE_SIZE);
            float ts = static_cast<float>(TILE_SIZE);
            bool valid = game.can_place_tower(gp);

            // Draw biome ground base tile
            Texture2D* ground_tex = game.assets.get_texture(theme.ground_tex);
            if (ground_tex) {
                draw_tex(ground_tex, tx + ts / 2.0f, ty + ts / 2.0f, ts, ts, 0, theme.ground_tint);
            }

            // Pulsing semi-transparent border
            float pulse_alpha = 0.4f + 0.3f * std::sin(static_cast<float>(GetTime()) * 4.0f);
            Color border_color = valid ? Color{0, 255, 0, static_cast<unsigned char>(255 * pulse_alpha)}
                                       : Color{255, 0, 0, static_cast<unsigned char>(255 * pulse_alpha)};
            DrawRectangleLinesEx({tx, ty, ts, ts}, 2.0f, border_color);

            // Tower weapon preview at 50% alpha when valid
            if (valid) {
                const char* weapon_tex_name = nullptr;
                switch (*game.play.placing_tower) {
                case TowerType::Arrow:
                    weapon_tex_name = assets::TOWER_ARROW;
                    break;
                case TowerType::Cannon:
                    weapon_tex_name = assets::TOWER_CANNON;
                    break;
                case TowerType::Ice:
                    weapon_tex_name = assets::TOWER_ICE;
                    break;
                case TowerType::Lightning:
                    weapon_tex_name = assets::TOWER_LIGHTNING;
                    break;
                case TowerType::Poison:
                    weapon_tex_name = assets::TOWER_POISON;
                    break;
                case TowerType::Laser:
                    weapon_tex_name = assets::TOWER_LASER;
                    break;
                }
                if (weapon_tex_name) {
                    Texture2D* weapon_tex = game.assets.get_texture(weapon_tex_name);
                    if (weapon_tex) {
                        draw_tex(weapon_tex, tx + ts / 2.0f, ty + ts / 2.0f, ts * 0.7f, ts * 0.7f, 0,
                                 {255, 255, 255, 128});
                    }
                }

                // Range indicator
                auto& stats = game.tower_registry.get(*game.play.placing_tower, 1);
                auto world = map.grid_to_world(gp);
                DrawCircleLines(static_cast<int>(world.x), static_cast<int>(world.y), stats.range, {255, 255, 255, 80});
            }
        }
    }

    // Selected tower range
    if (game.play.selected_tower != entt::null && reg.valid(game.play.selected_tower)) {
        auto& tower = reg.get<Tower>(game.play.selected_tower);
        auto& tf = reg.get<Transform>(game.play.selected_tower);
        DrawCircleLines(static_cast<int>(tf.position.x), static_cast<int>(tf.position.y), tower.range,
                        {255, 255, 255, 100});
    }

    // Enhanced Laser beams (3-layer beam)
    {
        auto view = reg.view<Tower, Transform>();
        for (auto [e, tower, tf] : view.each()) {
            if (tower.type == TowerType::Laser && tower.target != entt::null && reg.valid(tower.target) &&
                reg.all_of<Transform>(tower.target) && !reg.all_of<Dead>(tower.target)) {
                auto& etf = reg.get<Transform>(tower.target);
                // 3-layer beam: thick dark, medium red, thin white core
                DrawLineEx(tf.position.to_raylib(), etf.position.to_raylib(), 6.0f, {100, 0, 0, 150});
                DrawLineEx(tf.position.to_raylib(), etf.position.to_raylib(), 3.0f, RED);
                DrawLineEx(tf.position.to_raylib(), etf.position.to_raylib(), 1.0f, WHITE);
                // Spark particles at impact
                if (GetRandomValue(0, 2) == 0) {
                    float angle = static_cast<float>(GetRandomValue(0, 360)) * DEG2RAD;
                    float spd = static_cast<float>(GetRandomValue(20, 50));
                    create_particle(reg, etf.position, {std::cos(angle) * spd, std::sin(angle) * spd},
                                    {255, 200, 100, 255}, 3.0f, 0.2f, assets::PART_SPARK);
                }
            }
        }
    }

    // Draw entities sorted by layer
    // Particles
    {
        auto view = reg.view<Particle, Transform, Lifetime>();
        for (auto [e, p, tf, lt] : view.each()) {
            float alpha = std::clamp(p.decay, 0.0f, 1.0f);
            Texture2D* tex = nullptr;
            if (!p.particle_texture.empty()) {
                tex = game.assets.get_texture(p.particle_texture);
            }
            if (tex) {
                Color tint = ColorAlpha(WHITE, alpha);
                draw_tex(tex, tf.position.x, tf.position.y, p.size * 2.0f, p.size * 2.0f, 0, tint);
            } else {
                auto c = p.color;
                c.a = static_cast<unsigned char>(255.0f * alpha);
                DrawCircleV(tf.position.to_raylib(), p.size, c);
            }
        }
    }

    // Enemies with distinct visuals
    {
        auto view = reg.view<Enemy, Transform, Sprite>();
        for (auto [e, en, tf, spr] : view.each()) {
            if (reg.all_of<Dead>(e) || !spr.visible) continue;
            float hw = spr.width / 2, hh = spr.height / 2;

            // Display size for textures - large enough to be clearly visible
            float display_size;
            switch (en.type) {
            case EnemyType::Runner:
                display_size = 38.0f;
                break;
            case EnemyType::Grunt:
                display_size = 40.0f;
                break;
            case EnemyType::Healer:
                display_size = 38.0f;
                break;
            case EnemyType::Flying:
                display_size = 38.0f;
                break;
            case EnemyType::Tank:
                display_size = 46.0f;
                break;
            case EnemyType::Boss:
                display_size = 54.0f;
                break;
            }

            // Effect visuals (tint)
            Color tint = WHITE;
            if (reg.all_of<Effect>(e)) {
                auto& eff = reg.get<Effect>(e);
                if (eff.type == EffectType::Slow)
                    tint = {100, 200, 255, 255};
                else if (eff.type == EffectType::Poison)
                    tint = {100, 200, 50, 255};
                else if (eff.type == EffectType::Burn)
                    tint = {255, 150, 50, 255};
                else if (eff.type == EffectType::Stun)
                    tint = {200, 200, 200, 255};
            }

            // Compute rotation from velocity direction
            // Most Kenney vehicle sprites face RIGHT by default
            // Boss rocket faces UP, so needs +90 offset
            float rot = 0.0f;
            if (reg.all_of<Velocity>(e)) {
                auto& vel = reg.get<Velocity>(e);
                if (vel.vel.length() > 0.1f) {
                    rot = angle_from_dir(vel.vel);
                    if (en.type == EnemyType::Boss) rot += 90.0f; // rocket faces up
                }
            }

            // Get enemy texture
            const char* tex_name = nullptr;
            switch (en.type) {
            case EnemyType::Grunt:
                tex_name = assets::ENEMY_GRUNT;
                break;
            case EnemyType::Runner:
                tex_name = assets::ENEMY_RUNNER;
                break;
            case EnemyType::Tank:
                tex_name = assets::ENEMY_TANK;
                break;
            case EnemyType::Healer:
                tex_name = assets::ENEMY_HEALER;
                break;
            case EnemyType::Flying:
                tex_name = assets::ENEMY_FLYING;
                break;
            case EnemyType::Boss:
                tex_name = assets::ENEMY_BOSS;
                break;
            }

            Texture2D* tex = tex_name ? game.assets.get_texture(tex_name) : nullptr;
            if (tex) {
                draw_tex(tex, tf.position.x, tf.position.y, display_size, display_size, rot, tint);
            } else {
                // Procedural fallback
                Color c = spr.color;
                if (tint.r != 255 || tint.g != 255 || tint.b != 255) c = tint; // use effect color
                switch (en.type) {
                case EnemyType::Runner: {
                    Vec2 dir = {1, 0};
                    if (reg.all_of<Velocity>(e)) {
                        auto& vel = reg.get<Velocity>(e);
                        if (vel.vel.length() > 0.1f) dir = vel.vel.normalized();
                    }
                    Vec2 tip = tf.position + dir * hw;
                    Vec2 perp = {-dir.y, dir.x};
                    Vec2 left = tf.position - dir * (hw * 0.5f) + perp * (hh * 0.6f);
                    Vec2 right = tf.position - dir * (hw * 0.5f) - perp * (hh * 0.6f);
                    DrawTriangle(tip.to_raylib(), left.to_raylib(), right.to_raylib(), c);
                    break;
                }
                case EnemyType::Tank: {
                    DrawRectangle(static_cast<int>(tf.position.x - hw), static_cast<int>(tf.position.y - hh),
                                  static_cast<int>(spr.width), static_cast<int>(spr.height), c);
                    Color inner = {static_cast<unsigned char>(c.r * 0.6f), static_cast<unsigned char>(c.g * 0.6f),
                                   static_cast<unsigned char>(c.b * 0.6f), 255};
                    float pad = 4.0f;
                    DrawRectangle(static_cast<int>(tf.position.x - hw + pad),
                                  static_cast<int>(tf.position.y - hh + pad), static_cast<int>(spr.width - pad * 2),
                                  static_cast<int>(spr.height - pad * 2), inner);
                    break;
                }
                case EnemyType::Healer: {
                    DrawCircleV(tf.position.to_raylib(), hw, c);
                    Color cross = {50, 255, 50, 255};
                    float cs = hw * 0.5f;
                    DrawLineEx({tf.position.x - cs, tf.position.y}, {tf.position.x + cs, tf.position.y}, 2.0f, cross);
                    DrawLineEx({tf.position.x, tf.position.y - cs}, {tf.position.x, tf.position.y + cs}, 2.0f, cross);
                    break;
                }
                case EnemyType::Flying: {
                    Vector2 pts[] = {{tf.position.x, tf.position.y - hh},
                                     {tf.position.x + hw, tf.position.y},
                                     {tf.position.x, tf.position.y + hh},
                                     {tf.position.x - hw, tf.position.y}};
                    DrawTriangle(pts[0], pts[2], pts[1], c);
                    DrawTriangle(pts[0], pts[3], pts[2], c);
                    break;
                }
                default: {
                    DrawRectangle(static_cast<int>(tf.position.x - hw), static_cast<int>(tf.position.y - hh),
                                  static_cast<int>(spr.width), static_cast<int>(spr.height), c);
                    break;
                }
                }
            }

            // Boss glow ring (always drawn, textured or not)
            if (en.type == EnemyType::Boss) {
                float boss_r = display_size / 2.0f;
                float pulse = 0.5f + 0.5f * std::sin(static_cast<float>(GetTime()) * 4.0f);
                auto glow_alpha = static_cast<unsigned char>(60 + 100 * pulse);
                DrawCircleV(tf.position.to_raylib(), boss_r + 6, {255, 200, 50, glow_alpha});
                DrawCircleLines(static_cast<int>(tf.position.x), static_cast<int>(tf.position.y), boss_r + 3, GOLD);
                if (reg.all_of<Boss>(e)) {
                    auto& boss = reg.get<Boss>(e);
                    if (boss.ability_active && boss.boss_ability == AbilityType::DamageAura) {
                        auto aura_alpha = static_cast<unsigned char>(40 + 40 * pulse);
                        DrawCircleV(tf.position.to_raylib(), 120.0f, {255, 0, 0, aura_alpha});
                        DrawCircleLines(static_cast<int>(tf.position.x), static_cast<int>(tf.position.y), 120.0f,
                                        {255, 50, 50, static_cast<unsigned char>(100 + 100 * pulse)});
                    }
                }
            }

            // Healer aura ring
            if (en.type == EnemyType::Healer && reg.all_of<Aura>(e)) {
                auto& aura = reg.get<Aura>(e);
                DrawCircleLines(static_cast<int>(tf.position.x), static_cast<int>(tf.position.y), aura.radius,
                                {50, 255, 50, 60});
            }

            // Health bar - sized to match display_size
            if (reg.all_of<Health>(e)) {
                auto& hp = reg.get<Health>(e);
                if (hp.current < hp.max) {
                    float bar_w = display_size;
                    float bx = tf.position.x - bar_w / 2;
                    float by = tf.position.y - display_size / 2 - 6;
                    DrawRectangle(static_cast<int>(bx), static_cast<int>(by), static_cast<int>(bar_w), 3, DARKGRAY);
                    DrawRectangle(static_cast<int>(bx), static_cast<int>(by), static_cast<int>(bar_w * hp.ratio()), 3,
                                  GREEN);
                }
            }
        }
    }

    // Projectiles
    {
        auto view = reg.view<Projectile, Transform, Sprite>();
        for (auto [e, proj, tf, spr] : view.each()) {
            Texture2D* tex = nullptr;
            if (!spr.texture_name.empty()) {
                tex = game.assets.get_texture(spr.texture_name);
            }
            if (tex) {
                // Rotate projectile toward velocity direction
                float rot = 0.0f;
                if (reg.all_of<Velocity>(e)) {
                    auto& vel = reg.get<Velocity>(e);
                    if (vel.vel.length() > 0.1f) rot = angle_from_dir(vel.vel);
                }
                draw_tex(tex, tf.position.x, tf.position.y, spr.width, spr.height, rot, spr.color);
            } else {
                DrawCircleV(tf.position.to_raylib(), spr.width / 2, spr.color);
            }
        }
    }

    // Towers with distinct shapes
    {
        auto view = reg.view<Tower, Transform, Sprite>();
        for (auto [e, tower, tf, spr] : view.each()) {
            float hw = spr.width / 2, hh = spr.height / 2;
            float r = hw * 0.85f;

            // Get tower base and weapon textures
            const char* base_name = nullptr;
            switch (tower.level) {
            case 1:
                base_name = assets::TOWER_BASE_L1;
                break;
            case 2:
                base_name = assets::TOWER_BASE_L2;
                break;
            default:
                base_name = assets::TOWER_BASE_L3;
                break;
            }
            const char* weapon_name = nullptr;
            switch (tower.type) {
            case TowerType::Arrow:
                weapon_name = assets::TOWER_ARROW;
                break;
            case TowerType::Cannon:
                weapon_name = assets::TOWER_CANNON;
                break;
            case TowerType::Ice:
                weapon_name = assets::TOWER_ICE;
                break;
            case TowerType::Lightning:
                weapon_name = assets::TOWER_LIGHTNING;
                break;
            case TowerType::Poison:
                weapon_name = assets::TOWER_POISON;
                break;
            case TowerType::Laser:
                weapon_name = assets::TOWER_LASER;
                break;
            }

            Texture2D* base_tex = base_name ? game.assets.get_texture(base_name) : nullptr;
            Texture2D* weapon_tex = weapon_name ? game.assets.get_texture(weapon_name) : nullptr;

            if (base_tex && weapon_tex) {
                // Draw base platform at full tile size
                float ts = static_cast<float>(TILE_SIZE);
                draw_tex(base_tex, tf.position.x, tf.position.y, ts, ts, 0, WHITE);

                // Calculate weapon rotation toward target
                // Kenney TD weapon sprites face UP (north) by default, so no offset needed
                // atan2 gives 0 for east, -90 for north; we want 0 when pointing up
                float weapon_rot = 0.0f;
                if (tower.target != entt::null && reg.valid(tower.target) && reg.all_of<Transform>(tower.target)) {
                    auto& target_tf = reg.get<Transform>(tower.target);
                    Vec2 dir = target_tf.position - tf.position;
                    // Sprite faces up (north), atan2(0,-1) = -90deg, so add 90 to make north=0
                    weapon_rot = angle_from_dir(dir) + 90.0f;
                }

                // Draw weapon on top (90% of tile size for better visibility)
                float ws = ts * 0.9f;
                draw_tex(weapon_tex, tf.position.x, tf.position.y, ws, ws, weapon_rot, WHITE);
            } else {
                // Procedural fallback
                switch (tower.type) {
                case TowerType::Arrow: {
                    Vector2 top = {tf.position.x, tf.position.y - hh};
                    Vector2 bl = {tf.position.x - hw, tf.position.y + hh};
                    Vector2 br = {tf.position.x + hw, tf.position.y + hh};
                    DrawTriangle(top, bl, br, spr.color);
                    break;
                }
                case TowerType::Cannon: {
                    DrawCircleV(tf.position.to_raylib(), r, spr.color);
                    DrawCircleV({tf.position.x, tf.position.y - r * 0.6f}, r * 0.3f, {50, 50, 50, 255});
                    break;
                }
                case TowerType::Ice:
                    DrawPoly(tf.position.to_raylib(), 6, r, 0, spr.color);
                    break;
                case TowerType::Lightning:
                    DrawPoly(tf.position.to_raylib(), 4, r, 45, spr.color);
                    break;
                case TowerType::Poison: {
                    DrawCircleV(tf.position.to_raylib(), r, spr.color);
                    DrawLineEx({tf.position.x, tf.position.y + r}, {tf.position.x, tf.position.y + r + 6}, 3.0f,
                               spr.color);
                    break;
                }
                case TowerType::Laser:
                    DrawPoly(tf.position.to_raylib(), 4, r, 0, spr.color);
                    break;
                }
            }

            // Attack flash: white pulsing outline
            if (reg.all_of<AttackFlash>(e)) {
                auto& flash = reg.get<AttackFlash>(e);
                auto alpha = static_cast<unsigned char>(255 * (flash.timer / 0.15f));
                Color flashColor = {255, 255, 255, alpha};
                DrawCircleLinesV(tf.position.to_raylib(), r + 3, flashColor);
            }

            // Tower level indicator
            if (tower.level > 1) {
                for (int i = 0; i < tower.level - 1; ++i) {
                    DrawCircleV({tf.position.x - 8.0f + i * 10.0f, tf.position.y + hh - 4}, 3, GOLD);
                }
            }
            // Selection highlight
            if (e == game.play.selected_tower) {
                DrawRectangleLinesEx({tf.position.x - hw - 2, tf.position.y - hh - 2, spr.width + 4, spr.height + 4}, 2,
                                     WHITE);
            }

            // Tower health bar
            if (reg.all_of<Health>(e)) {
                auto& hp = reg.get<Health>(e);
                if (hp.current < hp.max) {
                    float bw = 40;
                    float bx = tf.position.x - bw / 2;
                    float by = tf.position.y - hh - 8;
                    DrawRectangle(static_cast<int>(bx), static_cast<int>(by), static_cast<int>(bw), 3, DARKGRAY);
                    Color hpc = hp.ratio() > 0.5f ? LIME : (hp.ratio() > 0.25f ? YELLOW : RED);
                    DrawRectangle(static_cast<int>(bx), static_cast<int>(by), static_cast<int>(bw * hp.ratio()), 3,
                                  hpc);
                }
            }
        }
    }

    // Coins
    {
        auto view = reg.view<Coin, Transform, Sprite>();
        for (auto [e, coin, tf, spr] : view.each()) {
            float bob_y = std::sin(coin.bob_timer) * 3.0f;
            Texture2D* tex = nullptr;
            if (!spr.texture_name.empty()) {
                tex = game.assets.get_texture(spr.texture_name);
            }
            float sz = 18.0f;
            if (tex) {
                draw_tex(tex, tf.position.x, tf.position.y + bob_y, sz, sz, 0, WHITE);
            } else {
                DrawCircleV({tf.position.x, tf.position.y + bob_y}, sz / 2, GOLD);
            }
            // Gold value text
            auto val_text = std::format("{}g", coin.value);
            draw_text(game.assets, val_text.c_str(), tf.position.x - 8, tf.position.y + bob_y - 14, 10, GOLD);
        }
    }

    // Hero
    {
        auto view = reg.view<Hero, Transform, Sprite, Health>();
        for (auto [e, hero, tf, spr, hp] : view.each()) {
            bool drew_sprite = false;

            if (reg.all_of<AnimatedSprite>(e)) {
                auto& anim = reg.get<AnimatedSprite>(e);
                Texture2D* tex = game.assets.get_texture(anim.texture_name);
                if (tex) {
                    // Extract the correct frame from spritesheet
                    int row = anim.anim_frames.empty() ? 0 : anim.anim_frames[anim.current_frame];
                    int col = anim.direction;
                    Rectangle srcRect = {static_cast<float>(col * anim.frame_width),
                                         static_cast<float>(row * anim.frame_height),
                                         static_cast<float>(anim.frame_width), static_cast<float>(anim.frame_height)};
                    float ds = anim.display_size;
                    draw_tex_src(tex, srcRect, tf.position.x, tf.position.y, ds, ds, 0, WHITE);
                    drew_sprite = true;
                }
            }

            if (!drew_sprite) {
                // Procedural fallback
                DrawCircleV(tf.position.to_raylib(), spr.width / 2, spr.color);
                DrawCircleLinesV(tf.position.to_raylib(), spr.width / 2 + 2, WHITE);
            }

            // Health bar
            float display_half = 17.0f;
            float bw = 40;
            float bx = tf.position.x - bw / 2;
            float by = tf.position.y - display_half - 8;
            DrawRectangle(static_cast<int>(bx), static_cast<int>(by), static_cast<int>(bw), 4, DARKGRAY);
            DrawRectangle(static_cast<int>(bx), static_cast<int>(by), static_cast<int>(bw * hp.ratio()), 4, LIME);

            // Level text
            auto lvl_text = std::format("Lv{}", hero.level);
            draw_text(game.assets, lvl_text.c_str(), tf.position.x - 8, tf.position.y + display_half + 2, 10, WHITE);
        }
    }

    // Floating text
    {
        auto view = reg.view<FloatingText, Transform, Lifetime>();
        for (auto [e, ft, tf, lt] : view.each()) {
            float alpha = std::clamp(lt.remaining / ft.max_time, 0.0f, 1.0f);
            auto c = ft.color;
            c.a = static_cast<unsigned char>(255 * alpha);
            float y_off = (ft.max_time - lt.remaining) * ft.speed;
            float tw = measure_text(game.assets, ft.text.c_str(), 14);
            draw_text(game.assets, ft.text.c_str(), tf.position.x - tw / 2, tf.position.y - y_off, 14, c);
        }
    }
}

// ============================================================
// 17. UI System
// ============================================================
void ui_system(Game& game) {
    auto& ps = game.play;
    auto& a = game.assets;

    // Clear popover rect each frame (will be set if popover is visible)
    ps.popover_rect = {};

    // Play UI click sound from asset pack if available
    auto play_ui_click = [&]() {
        Sound* click = a.get_sound(assets::SND_CLICK);
        if (click) {
            PlaySound(*click);
        } else {
            game.sounds.play(game.sounds.ui_click);
        }
    };

    // Top HUD bar
    DrawRectangle(0, 0, SCREEN_WIDTH, HUD_HEIGHT, {30, 30, 40, 240});

    draw_text(a, std::format("Gold: {}", ps.gold).c_str(), 10, 14, 20, GOLD);
    draw_text(a, std::format("Lives: {}", ps.lives).c_str(), 180, 14, 20, ps.lives > 5 ? GREEN : RED);
    draw_text(a, std::format("Wave: {}/{}", ps.current_wave, MAX_WAVES).c_str(), 340, 14, 20, WHITE);
    draw_text(a, std::format("Kills: {}", ps.total_kills).c_str(), 520, 14, 20, LIGHTGRAY);

    if (ps.game_speed_fast) {
        draw_text(a, ">> FAST", 680, 14, 20, YELLOW);
    }

    // Difficulty indicator
    const char* diff_names[] = {"EASY", "NORMAL", "HARD"};
    Color diff_colors[] = {GREEN, WHITE, RED};
    int di = static_cast<int>(game.difficulty);
    draw_text(a, diff_names[di], 680, 30, 12, diff_colors[di]);

    // Hero info
    auto heroes = game.registry.view<Hero, Health>();
    for (auto [e, hero, hp] : heroes.each()) {
        draw_text(a, std::format("Hero HP: {}/{}", hp.current, hp.max).c_str(), 780, 4, 16, LIME);
        draw_text(a, std::format("XP: {}/{} Lv{}", hero.xp, hero.xp_to_next, hero.level).c_str(), 780, 22, 14, SKYBLUE);

        // Ability cooldowns
        const char* ability_keys[] = {"Q", "E", "R"};
        const char* ability_names[] = {"Fireball", "Heal", "Lightning"};
        for (int i = 0; i < 3; ++i) {
            int ax = 980 + i * 100;
            Color ac = hero.abilities[i].ready() ? GREEN : DARKGRAY;
            DrawRectangle(ax, 4, 90, 38, {40, 40, 50, 200});
            DrawRectangleLinesEx({static_cast<float>(ax), 4, 90, 38}, 1, ac);
            draw_text(a, std::format("[{}] {}", ability_keys[i], ability_names[i]).c_str(), static_cast<float>(ax + 4),
                      8, 12, ac);
            if (!hero.abilities[i].ready()) {
                draw_text(a, std::format("{:.1f}s", hero.abilities[i].timer).c_str(), static_cast<float>(ax + 20), 24,
                          12, RED);
            } else {
                draw_text(a, "Ready", static_cast<float>(ax + 20), 24, 12, GREEN);
            }
        }
    }

    // Right panel - tower build menu
    int px = SCREEN_WIDTH - PANEL_WIDTH;
    DrawRectangle(px, HUD_HEIGHT, PANEL_WIDTH, SCREEN_HEIGHT - HUD_HEIGHT, {30, 30, 40, 220});
    draw_text(a, "TOWERS", static_cast<float>(px + 70), static_cast<float>(HUD_HEIGHT + 8), 18, WHITE);

    const TowerType tower_types[] = {TowerType::Arrow,     TowerType::Cannon, TowerType::Ice,
                                     TowerType::Lightning, TowerType::Poison, TowerType::Laser};

    const char* tower_descs[] = {
        "Reliable single-target damage",    "Slow but deals AoE splash damage",      "Slows enemies in range",
        "Chains lightning between enemies", "Poisons enemies with damage over time", "Continuous laser beam with burn"};

    const char* effect_descs[] = {"", "AoE splash", "Slow 50%", "Chain x2", "Poison DoT", "Burn DoT"};

    for (int i = 0; i < 6; ++i) {
        auto& stats = game.tower_registry.get(tower_types[i], 1);
        int by = HUD_HEIGHT + 35 + i * 55;
        Rectangle btn = {static_cast<float>(px + 10), static_cast<float>(by), PANEL_WIDTH - 20.0f, 50.0f};

        bool affordable = ps.gold >= stats.cost;
        bool is_placing = ps.placing_tower.has_value() && *ps.placing_tower == tower_types[i];
        Color bg =
            is_placing ? Color{60, 100, 60, 255} : (affordable ? Color{50, 50, 60, 255} : Color{40, 30, 30, 255});
        Color fg = affordable ? WHITE : DARKGRAY;

        DrawRectangleRec(btn, bg);
        DrawRectangleLinesEx(btn, 1, fg);

        // Tower color preview — use weapon texture if available
        const char* weapon_names[] = {assets::TOWER_ARROW,     assets::TOWER_CANNON, assets::TOWER_ICE,
                                      assets::TOWER_LIGHTNING, assets::TOWER_POISON, assets::TOWER_LASER};
        Texture2D* preview_tex = a.get_texture(weapon_names[i]);
        if (preview_tex) {
            draw_tex(preview_tex, static_cast<float>(px + 30), static_cast<float>(by + 25), 30, 30, 0, WHITE);
        } else {
            DrawRectangle(px + 15, by + 10, 30, 30, stats.color);
        }

        draw_text(a, stats.name.c_str(), static_cast<float>(px + 52), static_cast<float>(by + 5), 16, fg);
        draw_text(a, std::format("{}g  Dmg:{}", stats.cost, stats.damage).c_str(), static_cast<float>(px + 52),
                  static_cast<float>(by + 22), 12, fg);
        float dps =
            (tower_types[i] == TowerType::Laser) ? stats.damage / stats.fire_rate : stats.damage * stats.fire_rate;
        draw_text(a, std::format("Rng:{:.0f} DPS:{:.0f}", stats.range, dps).c_str(), static_cast<float>(px + 52),
                  static_cast<float>(by + 35), 10, GRAY);

        // Hover tooltip
        bool hovered = CheckCollisionPointRec(GetMousePosition(), btn);
        if (hovered && !IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            int tx = px - 210;
            int ty = by;
            DrawRectangle(tx, ty, 200, 90, {20, 20, 30, 240});
            DrawRectangleLinesEx({static_cast<float>(tx), static_cast<float>(ty), 200, 90}, 1, GOLD);
            draw_text(a, stats.name.c_str(), static_cast<float>(tx + 8), static_cast<float>(ty + 5), 16, GOLD);
            draw_text(a, tower_descs[i], static_cast<float>(tx + 8), static_cast<float>(ty + 24), 10, LIGHTGRAY);
            draw_text(a, std::format("Damage: {}  Range: {:.0f}", stats.damage, stats.range).c_str(),
                      static_cast<float>(tx + 8), static_cast<float>(ty + 40), 11, WHITE);
            draw_text(a, std::format("DPS: {:.1f}  Rate: {:.2f}/s", dps, stats.fire_rate).c_str(),
                      static_cast<float>(tx + 8), static_cast<float>(ty + 54), 11, WHITE);
            if (i > 0)
                draw_text(a, effect_descs[i], static_cast<float>(tx + 8), static_cast<float>(ty + 70), 11,
                          {200, 200, 100, 255});
        }

        // Click handler
        if (affordable && hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            ps.placing_tower = tower_types[i];
            ps.selected_tower = entt::null;
            play_ui_click();
        }
    }

    // Selected tower popover — anchored to the tower's world position
    if (ps.selected_tower != entt::null && game.registry.valid(ps.selected_tower)) {
        auto& tower = game.registry.get<Tower>(ps.selected_tower);
        auto& tf = game.registry.get<Transform>(ps.selected_tower);
        auto& stats_ref = game.tower_registry.get(tower.type, tower.level);

        // Convert tower world pos to screen pos
        Vector2 screen_pos = GetWorldToScreen2D(tf.position.to_raylib(), game.camera);

        // Popover dimensions
        float pop_w = 210;
        float pop_h = 170;
        bool has_hp = game.registry.all_of<Health>(ps.selected_tower);
        if (has_hp) pop_h += 18;
        // Extra space for repair button when tower is damaged
        if (has_hp) {
            auto& thp_check = game.registry.get<Health>(ps.selected_tower);
            if (thp_check.current < thp_check.max) pop_h += 32;
        }

        // Position popover above-right of tower, clamp to screen
        float pop_x = screen_pos.x + TILE_SIZE * 0.6f;
        float pop_y = screen_pos.y - pop_h - TILE_SIZE * 0.3f;

        // Clamp so popover stays on screen
        if (pop_x + pop_w > SCREEN_WIDTH - 10) pop_x = screen_pos.x - pop_w - TILE_SIZE * 0.6f;
        if (pop_y < HUD_HEIGHT + 5) pop_y = screen_pos.y + TILE_SIZE * 0.6f;
        if (pop_x < 5) pop_x = 5;
        if (pop_y + pop_h > SCREEN_HEIGHT - 5) pop_y = SCREEN_HEIGHT - pop_h - 5;

        // Store popover rect so handle_input can avoid deselecting when clicking inside it
        ps.popover_rect = {pop_x, pop_y, pop_w, pop_h};

        // Draw connector line from tower to popover
        float line_start_x = screen_pos.x;
        float line_start_y = screen_pos.y;
        float line_end_x = (pop_x < screen_pos.x) ? pop_x + pop_w : pop_x;
        float line_end_y = pop_y + pop_h / 2;
        DrawLineEx({line_start_x, line_start_y}, {line_end_x, line_end_y}, 1.5f, {255, 255, 255, 60});

        // Background with rounded corners effect (draw slightly overlapping rects + border)
        DrawRectangle(static_cast<int>(pop_x), static_cast<int>(pop_y), static_cast<int>(pop_w),
                      static_cast<int>(pop_h), {22, 24, 32, 235});
        DrawRectangleLinesEx({pop_x, pop_y, pop_w, pop_h}, 1.5f, {80, 85, 100, 200});

        // Header bar with tower name + level
        DrawRectangle(static_cast<int>(pop_x), static_cast<int>(pop_y), static_cast<int>(pop_w), 28, {35, 38, 50, 255});
        DrawLineEx({pop_x, pop_y + 28}, {pop_x + pop_w, pop_y + 28}, 1.0f, {80, 85, 100, 200});

        // Tower weapon icon in header
        const char* weapon_names[] = {assets::TOWER_ARROW,     assets::TOWER_CANNON, assets::TOWER_ICE,
                                      assets::TOWER_LIGHTNING, assets::TOWER_POISON, assets::TOWER_LASER};
        int type_idx = static_cast<int>(tower.type);
        Texture2D* icon_tex = (type_idx >= 0 && type_idx < 6) ? a.get_texture(weapon_names[type_idx]) : nullptr;
        if (icon_tex) {
            draw_tex(icon_tex, pop_x + 16, pop_y + 14, 22, 22, 0, WHITE);
        }

        // Name and level
        draw_text(a, stats_ref.name.c_str(), pop_x + 30, pop_y + 5, 16, WHITE);

        // Level pips
        float pip_x = pop_x + pop_w - 12 - TowerRegistry::MAX_LEVEL * 14;
        for (int p = 0; p < TowerRegistry::MAX_LEVEL; ++p) {
            Color pip_col = (p < tower.level) ? GOLD : Color{50, 52, 60, 255};
            DrawCircle(static_cast<int>(pip_x + p * 14 + 5), static_cast<int>(pop_y + 14), 5.0f, pip_col);
            DrawCircleLines(static_cast<int>(pip_x + p * 14 + 5), static_cast<int>(pop_y + 14), 5.0f,
                            {80, 85, 100, 255});
        }

        // Stats section
        float sy = pop_y + 34;
        float label_x = pop_x + 12;
        float val_x = pop_x + 90;

        // DPS calculation
        float dps = (tower.type == TowerType::Laser) ? tower.damage / tower.fire_rate : tower.damage * tower.fire_rate;

        draw_text(a, "Damage", label_x, sy, 13, {160, 165, 180, 255});
        draw_text(a, std::format("{}", tower.damage).c_str(), val_x, sy, 13, WHITE);
        sy += 17;

        draw_text(a, "Range", label_x, sy, 13, {160, 165, 180, 255});
        draw_text(a, std::format("{:.0f}", tower.range).c_str(), val_x, sy, 13, WHITE);
        sy += 17;

        draw_text(a, "DPS", label_x, sy, 13, {160, 165, 180, 255});
        draw_text(a, std::format("{:.1f}", dps).c_str(), val_x, sy, 13, {100, 255, 100, 255});
        sy += 17;

        // Effect info
        if (tower.effect != EffectType::None) {
            const char* eff_names[] = {"", "Slow", "Poison", "Burn", "Stun"};
            int ei = static_cast<int>(tower.effect);
            Color eff_col;
            switch (tower.effect) {
            case EffectType::Slow:
                eff_col = {100, 180, 255, 255};
                break;
            case EffectType::Poison:
                eff_col = {100, 220, 50, 255};
                break;
            case EffectType::Burn:
                eff_col = {255, 140, 50, 255};
                break;
            case EffectType::Stun:
                eff_col = {255, 255, 100, 255};
                break;
            default:
                eff_col = WHITE;
                break;
            }
            draw_text(a, "Effect", label_x, sy, 13, {160, 165, 180, 255});
            draw_text(a, std::format("{} {:.1f}s", eff_names[ei], tower.effect_duration).c_str(), val_x, sy, 13,
                      eff_col);
            sy += 17;
        }

        // HP bar if applicable
        if (has_hp) {
            auto& thp = game.registry.get<Health>(ps.selected_tower);
            draw_text(a, "HP", label_x, sy, 13, {160, 165, 180, 255});
            // HP bar
            float bar_x = val_x;
            float bar_w = pop_w - val_x + pop_x - 12;
            float bar_h = 10;
            DrawRectangle(static_cast<int>(bar_x), static_cast<int>(sy + 2), static_cast<int>(bar_w),
                          static_cast<int>(bar_h), {40, 40, 50, 255});
            Color hp_col = thp.ratio() > 0.5f ? GREEN : (thp.ratio() > 0.25f ? YELLOW : RED);
            DrawRectangle(static_cast<int>(bar_x), static_cast<int>(sy + 2), static_cast<int>(bar_w * thp.ratio()),
                          static_cast<int>(bar_h), hp_col);
            draw_text(a, std::format("{}/{}", thp.current, thp.max).c_str(), bar_x + 2, sy, 11, WHITE);
            sy += 17;
        }

        sy += 4;

        // Buttons
        float btn_h = 26;
        float btn_gap = 6;
        float btn_margin = 10;
        float btn_area_w = pop_w - btn_margin * 2;

        // Repair button row (only if tower is damaged)
        if (has_hp) {
            auto& thp = game.registry.get<Health>(ps.selected_tower);
            if (thp.current < thp.max) {
                int missing = thp.max - thp.current;
                int repair_cost = std::max(1, missing / 4); // 4 HP per gold
                bool can_repair = ps.gold >= repair_cost;
                Rectangle rbtn = {pop_x + btn_margin, sy, btn_area_w, btn_h};
                bool r_hover = CheckCollisionPointRec(GetMousePosition(), rbtn);

                Color rbg = can_repair ? (r_hover ? Color{50, 110, 140, 255} : Color{35, 80, 110, 255})
                                       : Color{50, 50, 55, 255};
                DrawRectangleRec(rbtn, rbg);
                DrawRectangleLinesEx(rbtn, 1.0f, can_repair ? Color{70, 160, 200, 200} : Color{70, 70, 80, 200});

                auto repair_label = std::format("Repair {}g  ({} HP)", repair_cost, missing);
                float rl_w = measure_text(a, repair_label.c_str(), 12);
                draw_text(a, repair_label.c_str(), rbtn.x + (rbtn.width - rl_w) / 2, rbtn.y + 7, 12,
                          can_repair ? WHITE : Color{100, 100, 110, 255});

                if (can_repair && r_hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                    ps.gold -= repair_cost;
                    ps.stats.gold_spent += repair_cost;
                    thp.current = thp.max;
                    create_floating_text(game.registry, tf.position, "REPAIRED!", {70, 200, 255, 255});
                    play_ui_click();
                }
                sy += btn_h + btn_gap;
            }
        }

        // Upgrade + Sell row
        if (tower.level < TowerRegistry::MAX_LEVEL) {
            int ucost = game.tower_registry.upgrade_cost(tower.type, tower.level);
            bool can_upgrade = ps.gold >= ucost;
            float ubtn_w = btn_area_w * 0.58f;
            Rectangle ubtn = {pop_x + btn_margin, sy, ubtn_w, btn_h};
            bool u_hover = CheckCollisionPointRec(GetMousePosition(), ubtn);

            Color ubg =
                can_upgrade ? (u_hover ? Color{60, 130, 60, 255} : Color{45, 100, 45, 255}) : Color{50, 50, 55, 255};
            DrawRectangleRec(ubtn, ubg);
            DrawRectangleLinesEx(ubtn, 1.0f, can_upgrade ? Color{80, 180, 80, 200} : Color{70, 70, 80, 200});

            auto upgrade_label = std::format("Upgrade {}g", ucost);
            float ul_w = measure_text(a, upgrade_label.c_str(), 12);
            draw_text(a, upgrade_label.c_str(), ubtn.x + (ubtn.width - ul_w) / 2, ubtn.y + 7, 12,
                      can_upgrade ? WHITE : Color{100, 100, 110, 255});

            if (can_upgrade && u_hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                ps.gold -= ucost;
                ps.stats.gold_spent += ucost;
                tower.level++;
                auto& new_stats = game.tower_registry.get(tower.type, tower.level);
                tower.damage = new_stats.damage;
                tower.range = new_stats.range;
                tower.fire_rate = new_stats.fire_rate;
                tower.aoe_radius = new_stats.aoe_radius;
                tower.chain_count = new_stats.chain_count;
                tower.effect = new_stats.effect;
                tower.effect_duration = new_stats.effect_duration;
                // Also heal tower to new max HP on upgrade
                if (game.registry.all_of<Health>(ps.selected_tower)) {
                    auto& thp = game.registry.get<Health>(ps.selected_tower);
                    int new_max_hp = tower_max_hp(tower.type, tower.level);
                    thp.max = new_max_hp;
                    thp.current = new_max_hp;
                }
                auto& spr = game.registry.get<Sprite>(ps.selected_tower);
                spr.color = new_stats.color;
                play_ui_click();
            }

            // Sell button
            float sbtn_w = btn_area_w - ubtn_w - btn_gap;
            Rectangle sbtn = {pop_x + btn_margin + ubtn_w + btn_gap, sy, sbtn_w, btn_h};
            bool s_hover = CheckCollisionPointRec(GetMousePosition(), sbtn);

            DrawRectangleRec(sbtn, s_hover ? Color{140, 50, 50, 255} : Color{100, 40, 40, 255});
            DrawRectangleLinesEx(sbtn, 1.0f, {180, 80, 80, 200});

            int sell_val = tower.cost / 2;
            auto sell_label = std::format("Sell +{}g", sell_val);
            float sl_w = measure_text(a, sell_label.c_str(), 12);
            draw_text(a, sell_label.c_str(), sbtn.x + (sbtn.width - sl_w) / 2, sbtn.y + 7, 12, WHITE);

            if (s_hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                ps.gold += sell_val;
                ps.stats.towers_sold++;
                auto& gc = game.registry.get<GridCell>(ps.selected_tower);
                ps.tower_positions.erase(gc.pos);
                game.registry.destroy(ps.selected_tower);
                ps.selected_tower = entt::null;
                game.recalculate_path();
                play_ui_click();
            }
        } else {
            // Max level — MAXED badge + sell only
            draw_text(a, "MAX LEVEL", pop_x + btn_margin, sy + 6, 13, GOLD);

            float sbtn_w = btn_area_w * 0.45f;
            Rectangle sbtn = {pop_x + pop_w - btn_margin - sbtn_w, sy, sbtn_w, btn_h};
            bool s_hover = CheckCollisionPointRec(GetMousePosition(), sbtn);

            DrawRectangleRec(sbtn, s_hover ? Color{140, 50, 50, 255} : Color{100, 40, 40, 255});
            DrawRectangleLinesEx(sbtn, 1.0f, {180, 80, 80, 200});

            int sell_val = tower.cost / 2;
            auto sell_label = std::format("Sell +{}g", sell_val);
            float sl_w = measure_text(a, sell_label.c_str(), 12);
            draw_text(a, sell_label.c_str(), sbtn.x + (sbtn.width - sl_w) / 2, sbtn.y + 7, 12, WHITE);

            if (s_hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                ps.gold += sell_val;
                ps.stats.towers_sold++;
                auto& gc = game.registry.get<GridCell>(ps.selected_tower);
                ps.tower_positions.erase(gc.pos);
                game.registry.destroy(ps.selected_tower);
                ps.selected_tower = entt::null;
                game.recalculate_path();
                play_ui_click();
            }
        }
    }

    // Wave countdown + preview
    if (!ps.wave_active && ps.current_wave < MAX_WAVES) {
        // Pre-game countdown before first wave
        auto wave_text = std::format("First wave in {:.1f}s", std::max(0.0f, ps.wave_timer));
        float wtw = measure_text(a, wave_text.c_str(), 18);
        draw_text(a, wave_text.c_str(), SCREEN_WIDTH / 2.0f - wtw / 2, static_cast<float>(SCREEN_HEIGHT - 30), 18,
                  YELLOW);
        auto space_text = "Press SPACE to start early";
        float stw = measure_text(a, space_text, 14);
        draw_text(a, space_text, SCREEN_WIDTH / 2.0f - stw / 2, static_cast<float>(SCREEN_HEIGHT - 50), 14, GRAY);
    } else if (ps.wave_active) {
        // Show current wave and enemies alive
        auto rem_text = std::format("Wave {}/{}  -  {} enemies alive", ps.current_wave, MAX_WAVES, ps.enemies_alive);
        float rw = measure_text(a, rem_text.c_str(), 14);
        draw_text(a, rem_text.c_str(), SCREEN_WIDTH / 2.0f - rw / 2, static_cast<float>(SCREEN_HEIGHT - 30), 14,
                  {200, 200, 200, 200});
    }

    // Wave announcement banner
    if (ps.banner.active) {
        float alpha = std::clamp(ps.banner.timer / 0.5f, 0.0f, 1.0f);
        if (ps.banner.timer > 2.5f) alpha = std::clamp((3.0f - ps.banner.timer) / 0.5f, 0.0f, 1.0f);
        float font_size = 48;
        float tw = measure_text(a, ps.banner.text.c_str(), font_size);
        Color c = ps.banner.color;
        c.a = static_cast<unsigned char>(255 * alpha);
        draw_text(a, ps.banner.text.c_str(), SCREEN_WIDTH / 2.0f - tw / 2, SCREEN_HEIGHT / 2.0f - 80, font_size, c);
    }

    // Tutorial overlay
    if (ps.tutorial.active && !ps.tutorial.completed) {
        const char* hints[] = {"Move near enemies to attack them and earn gold for towers!",
                               "Once you have gold, click a tower then click a green tile to place it.",
                               "WASD to move hero. Q/E/R for abilities. Stay close to fight!",
                               "Click a placed tower to upgrade it. Sell for 50% refund.",
                               "P to pause, F for fast-forward. Good luck!"};
        int step = std::clamp(ps.tutorial.step, 0, 4);
        float tw = measure_text(a, hints[step], 16);
        float tx = SCREEN_WIDTH / 2.0f - tw / 2.0f - 10;
        float ty = SCREEN_HEIGHT / 2.0f + 40;
        DrawRectangle(static_cast<int>(tx - 5), static_cast<int>(ty - 5), static_cast<int>(tw + 20), 30,
                      {0, 0, 0, 180});
        DrawRectangleLinesEx({tx - 5, ty - 5, tw + 20, 30.0f}, 1, GOLD);
        draw_text(a, hints[step], tx + 5, ty + 2, 16, GOLD);
        auto dismiss_text = "[TAB to dismiss]";
        float dw = measure_text(a, dismiss_text, 10);
        draw_text(a, dismiss_text, SCREEN_WIDTH / 2.0f - dw / 2, ty + 28, 10, GRAY);
    }

    // Controls help
    draw_text(a, "WASD:Move  Q:Fire  E:Heal  R:Lightning  P:Pause  F:Speed  M:Mute  ESC:Menu", 10,
              static_cast<float>(SCREEN_HEIGHT - 18), 12, {150, 150, 150, 180});
}

} // namespace ls::systems
