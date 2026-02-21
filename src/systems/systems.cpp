#include "systems.hpp"
#include "core/game.hpp"
#include "components/components.hpp"
#include "factory/enemy_factory.hpp"
#include "factory/projectile_factory.hpp"
#include "factory/tower_factory.hpp"
#include "factory/hero_factory.hpp"
#include <algorithm>
#include <cmath>
#include <format>
#include <raylib.h>

namespace ls::systems {

// ============================================================
// 1. Hero System - WASD movement, auto-attack, abilities
// ============================================================
void hero_system(Game& game, float dt) {
    auto& reg = game.registry;
    auto view = reg.view<Hero, Transform, Health>();

    for (auto [e, hero, tf, hp] : view.each()) {
        // WASD movement
        Vec2 move{};
        if (IsKeyDown(KEY_W)) move.y -= 1;
        if (IsKeyDown(KEY_S)) move.y += 1;
        if (IsKeyDown(KEY_A)) move.x -= 1;
        if (IsKeyDown(KEY_D)) move.x += 1;

        if (move.length() > 0.01f) {
            move = move.normalized() * (HERO_SPEED * dt);
            tf.position = tf.position + move;
            // Clamp to screen
            tf.position.x = std::clamp(tf.position.x, 0.0f, static_cast<float>(SCREEN_WIDTH));
            tf.position.y = std::clamp(tf.position.y, static_cast<float>(GRID_OFFSET_Y),
                                       static_cast<float>(SCREEN_HEIGHT));
        }

        // Auto-attack nearest enemy
        hero.attack_cooldown -= dt;
        if (hero.attack_cooldown <= 0.0f) {
            entt::entity nearest = entt::null;
            float best_dist = HERO_ATTACK_RANGE;

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
                auto& ehp = reg.get<Health>(nearest);
                int dmg = HERO_BASE_DAMAGE + hero.level * 3;
                int actual = std::max(1, dmg - ehp.armor);
                ehp.current -= actual;
                hero.attack_cooldown = HERO_ATTACK_COOLDOWN;
                create_floating_text(reg, etf.position, std::to_string(actual), YELLOW);
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
                        create_particle(reg, etf.position,
                            {std::cos(angle) * spd, std::sin(angle) * spd},
                            {255, static_cast<unsigned char>(GetRandomValue(50, 200)), 0, 255},
                            6.0f, 0.5f);
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
                create_particle(reg, tf.position,
                    {std::cos(angle) * spd, std::sin(angle) * spd},
                    GREEN, 5.0f, 0.8f);
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
                create_particle(reg, target,
                    {std::cos(angle) * spd, std::sin(angle) * spd},
                    {255, 255, static_cast<unsigned char>(GetRandomValue(100, 255)), 255},
                    4.0f, 0.4f);
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
        // All spawned, wait for enemies to die
        if (ps.enemies_alive <= 0) {
            ps.wave_active = false;
            ps.wave_timer = WAVE_DELAY;
            game.dispatcher.trigger(WaveCompleteEvent{ps.current_wave});
        }
        return;
    }

    ps.spawn_timer -= dt;
    if (ps.spawn_timer <= 0.0f) {
        auto& entry = wave.spawns[ps.spawn_index];
        float scaling = game.wave_manager.scaling(ps.current_wave);

        // Apply difficulty scaling
        if (game.difficulty == Difficulty::Easy) scaling *= 0.8f;
        else if (game.difficulty == Difficulty::Hard) scaling *= 1.3f;

        // Flying enemies use flying_path (shortcut)
        auto& path = (entry.type == EnemyType::Flying && !ps.flying_path.empty())
            ? ps.flying_path : ps.enemy_path;

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
                    reg.emplace_or_replace<Effect>(tower.target, tower.effect, tower.effect_duration, 0.0f, 0.5f, 8, 1.0f);
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
                default: break;
            }

            create_projectile(reg, tf.position, tower.target, target_tf.position,
                tower.damage, dtype, PROJECTILE_SPEED,
                tower.aoe_radius, tower.effect, tower.effect_duration,
                tower.chain_count, proj_color);

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
                            reg.emplace_or_replace<Effect>(ee, proj.effect, proj.effect_duration, 0.0f, 0.5f,
                                proj.effect == EffectType::Poison ? 5 : (proj.effect == EffectType::Burn ? 8 : 0),
                                proj.effect == EffectType::Slow ? 0.5f : 1.0f);
                        }
                    }
                }
                // Explosion particles
                for (int i = 0; i < 8; ++i) {
                    float angle = static_cast<float>(GetRandomValue(0, 360)) * DEG2RAD;
                    float spd = static_cast<float>(GetRandomValue(30, 80));
                    create_particle(reg, tf.position,
                        {std::cos(angle) * spd, std::sin(angle) * spd},
                        proj.trail_color, 5.0f, 0.4f);
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
                        reg.emplace_or_replace<Effect>(proj.target, proj.effect, proj.effect_duration, 0.0f, 0.5f,
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
                        create_projectile(reg, tf.position, chain_target, ctf.position,
                            proj.damage * 3 / 4, proj.damage_type, proj.speed * 1.5f,
                            0, proj.effect, proj.effect_duration,
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
                    ahp.current = std::min(ahp.max,
                        ahp.current + static_cast<int>(aura.heal_per_sec * dt));
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
    auto view = reg.view<Health>(entt::exclude<Dead, Hero>);

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
                    create_particle(reg, tf.position,
                        {std::cos(angle) * spd, std::sin(angle) * spd},
                        spr.color, (en.type == EnemyType::Boss) ? 6.0f : 4.0f, 0.6f);
                }
                // Gold text
                create_floating_text(reg, tf.position,
                    "+" + std::to_string(en.reward) + "g", GOLD);

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
void economy_system([[maybe_unused]] Game& game, [[maybe_unused]] float dt) {
}

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
                        create_particle(reg, tf.position,
                            {std::cos(angle) * 40.0f, std::sin(angle) * 40.0f},
                            RED, 4.0f, 0.5f);
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
// 16. Render System
// ============================================================
void render_system(Game& game) {
    auto& reg = game.registry;
    auto& map = game.current_map;

    // Draw tiles
    for (int y = 0; y < map.rows; ++y) {
        for (int x = 0; x < map.cols; ++x) {
            auto tile = map.tiles[y][x];
            Color c;
            switch (tile) {
                case TileType::Grass:    c = {60, 120, 40, 255}; break;
                case TileType::Buildable:c = {70, 130, 50, 255}; break;
                case TileType::Path:     c = {160, 140, 100, 255}; break;
                case TileType::Spawn:    c = {200, 50, 50, 255}; break;
                case TileType::Exit:     c = {50, 50, 200, 255}; break;
                case TileType::Blocked:  c = {80, 80, 80, 255}; break;
            }
            DrawRectangle(GRID_OFFSET_X + x * TILE_SIZE, GRID_OFFSET_Y + y * TILE_SIZE,
                         TILE_SIZE - 1, TILE_SIZE - 1, c);
        }
    }

    // Grid overlay for placement
    if (game.play.placing_tower.has_value()) {
        auto gp = game.mouse_grid();
        if (map.in_bounds(gp)) {
            Color c = game.can_place_tower(gp) ? Color{0, 255, 0, 60} : Color{255, 0, 0, 60};
            DrawRectangle(GRID_OFFSET_X + gp.x * TILE_SIZE, GRID_OFFSET_Y + gp.y * TILE_SIZE,
                         TILE_SIZE, TILE_SIZE, c);

            // Range indicator
            if (game.can_place_tower(gp)) {
                auto& stats = game.tower_registry.get(*game.play.placing_tower, 1);
                auto world = map.grid_to_world(gp);
                DrawCircleLines(static_cast<int>(world.x), static_cast<int>(world.y),
                               stats.range, {255, 255, 255, 80});
            }
        }
    }

    // Selected tower range
    if (game.play.selected_tower != entt::null && reg.valid(game.play.selected_tower)) {
        auto& tower = reg.get<Tower>(game.play.selected_tower);
        auto& tf = reg.get<Transform>(game.play.selected_tower);
        DrawCircleLines(static_cast<int>(tf.position.x), static_cast<int>(tf.position.y),
                       tower.range, {255, 255, 255, 100});
    }

    // Enhanced Laser beams (3-layer beam)
    {
        auto view = reg.view<Tower, Transform>();
        for (auto [e, tower, tf] : view.each()) {
            if (tower.type == TowerType::Laser && tower.target != entt::null
                && reg.valid(tower.target) && reg.all_of<Transform>(tower.target)
                && !reg.all_of<Dead>(tower.target)) {
                auto& etf = reg.get<Transform>(tower.target);
                // 3-layer beam: thick dark, medium red, thin white core
                DrawLineEx(tf.position.to_raylib(), etf.position.to_raylib(), 6.0f, {100, 0, 0, 150});
                DrawLineEx(tf.position.to_raylib(), etf.position.to_raylib(), 3.0f, RED);
                DrawLineEx(tf.position.to_raylib(), etf.position.to_raylib(), 1.0f, WHITE);
                // Spark particles at impact
                if (GetRandomValue(0, 2) == 0) {
                    float angle = static_cast<float>(GetRandomValue(0, 360)) * DEG2RAD;
                    float spd = static_cast<float>(GetRandomValue(20, 50));
                    create_particle(reg, etf.position,
                        {std::cos(angle) * spd, std::sin(angle) * spd},
                        {255, 200, 100, 255}, 3.0f, 0.2f);
                }
            }
        }
    }

    // Draw entities sorted by layer
    // Particles
    {
        auto view = reg.view<Particle, Transform, Lifetime>();
        for (auto [e, p, tf, lt] : view.each()) {
            auto c = p.color;
            c.a = static_cast<unsigned char>(255.0f * std::clamp(p.decay, 0.0f, 1.0f));
            DrawCircleV(tf.position.to_raylib(), p.size, c);
        }
    }

    // Enemies with distinct visuals
    {
        auto view = reg.view<Enemy, Transform, Sprite>();
        for (auto [e, en, tf, spr] : view.each()) {
            if (reg.all_of<Dead>(e) || !spr.visible) continue;
            float hw = spr.width / 2, hh = spr.height / 2;

            // Effect visuals
            Color c = spr.color;
            if (reg.all_of<Effect>(e)) {
                auto& eff = reg.get<Effect>(e);
                if (eff.type == EffectType::Slow) c = {100, 200, 255, 255};
                if (eff.type == EffectType::Poison) c = {100, 200, 50, 255};
                if (eff.type == EffectType::Burn) c = {255, 150, 50, 255};
                if (eff.type == EffectType::Stun) c = {200, 200, 200, 255};
            }

            switch (en.type) {
                case EnemyType::Runner: {
                    // Small triangle pointing in movement direction
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
                    // Double rectangle (outer + darker inner for armor)
                    DrawRectangle(static_cast<int>(tf.position.x - hw), static_cast<int>(tf.position.y - hh),
                                 static_cast<int>(spr.width), static_cast<int>(spr.height), c);
                    Color inner = {static_cast<unsigned char>(c.r * 0.6f), static_cast<unsigned char>(c.g * 0.6f),
                                   static_cast<unsigned char>(c.b * 0.6f), 255};
                    float pad = 4.0f;
                    DrawRectangle(static_cast<int>(tf.position.x - hw + pad), static_cast<int>(tf.position.y - hh + pad),
                                 static_cast<int>(spr.width - pad * 2), static_cast<int>(spr.height - pad * 2), inner);
                    break;
                }
                case EnemyType::Healer: {
                    // Circle with green "+" cross
                    DrawCircleV(tf.position.to_raylib(), hw, c);
                    Color cross = {50, 255, 50, 255};
                    float cs = hw * 0.5f;
                    DrawLineEx({tf.position.x - cs, tf.position.y}, {tf.position.x + cs, tf.position.y}, 2.0f, cross);
                    DrawLineEx({tf.position.x, tf.position.y - cs}, {tf.position.x, tf.position.y + cs}, 2.0f, cross);
                    break;
                }
                case EnemyType::Flying: {
                    // Diamond shape
                    Vector2 pts[] = {
                        {tf.position.x, tf.position.y - hh},
                        {tf.position.x + hw, tf.position.y},
                        {tf.position.x, tf.position.y + hh},
                        {tf.position.x - hw, tf.position.y}
                    };
                    DrawTriangle(pts[0], pts[2], pts[1], c);
                    DrawTriangle(pts[0], pts[3], pts[2], c);
                    break;
                }
                case EnemyType::Boss: {
                    // Circle with pulsing glow ring
                    DrawCircleV(tf.position.to_raylib(), hw, c);
                    float pulse = 0.5f + 0.5f * std::sin(static_cast<float>(GetTime()) * 4.0f);
                    auto glow_alpha = static_cast<unsigned char>(60 + 100 * pulse);
                    DrawCircleV(tf.position.to_raylib(), hw + 6, {255, 200, 50, glow_alpha});
                    DrawCircleLines(static_cast<int>(tf.position.x), static_cast<int>(tf.position.y), hw + 3, GOLD);
                    // DamageAura visual
                    if (reg.all_of<Boss>(e)) {
                        auto& boss = reg.get<Boss>(e);
                        if (boss.ability_active && boss.boss_ability == AbilityType::DamageAura) {
                            auto aura_alpha = static_cast<unsigned char>(40 + 40 * pulse);
                            DrawCircleV(tf.position.to_raylib(), 120.0f, {255, 0, 0, aura_alpha});
                            DrawCircleLines(static_cast<int>(tf.position.x), static_cast<int>(tf.position.y), 120.0f,
                                           {255, 50, 50, static_cast<unsigned char>(100 + 100 * pulse)});
                        }
                    }
                    break;
                }
                default: {
                    // Grunt: standard rectangle
                    DrawRectangle(static_cast<int>(tf.position.x - hw), static_cast<int>(tf.position.y - hh),
                                 static_cast<int>(spr.width), static_cast<int>(spr.height), c);
                    break;
                }
            }

            // Healer aura ring
            if (en.type == EnemyType::Healer && reg.all_of<Aura>(e)) {
                auto& aura = reg.get<Aura>(e);
                DrawCircleLines(static_cast<int>(tf.position.x), static_cast<int>(tf.position.y),
                               aura.radius, {50, 255, 50, 60});
            }

            // Health bar
            if (reg.all_of<HealthBarComp>(e) && reg.all_of<Health>(e)) {
                auto& hp = reg.get<Health>(e);
                auto& hb = reg.get<HealthBarComp>(e);
                if (hp.current < hp.max) {
                    float bx = tf.position.x - hb.width / 2;
                    float by = tf.position.y + hb.offset_y;
                    DrawRectangle(static_cast<int>(bx), static_cast<int>(by),
                                 static_cast<int>(hb.width), static_cast<int>(hb.height), DARKGRAY);
                    DrawRectangle(static_cast<int>(bx), static_cast<int>(by),
                                 static_cast<int>(hb.width * hp.ratio()), static_cast<int>(hb.height), GREEN);
                }
            }
        }
    }

    // Projectiles
    {
        auto view = reg.view<Projectile, Transform, Sprite>();
        for (auto [e, proj, tf, spr] : view.each()) {
            DrawCircleV(tf.position.to_raylib(), spr.width / 2, spr.color);
        }
    }

    // Towers with distinct shapes
    {
        auto view = reg.view<Tower, Transform, Sprite>();
        for (auto [e, tower, tf, spr] : view.each()) {
            float hw = spr.width / 2, hh = spr.height / 2;
            float r = hw * 0.85f; // radius for shapes

            switch (tower.type) {
                case TowerType::Arrow: {
                    // Triangle pointing up
                    Vector2 top = {tf.position.x, tf.position.y - hh};
                    Vector2 bl = {tf.position.x - hw, tf.position.y + hh};
                    Vector2 br = {tf.position.x + hw, tf.position.y + hh};
                    DrawTriangle(top, bl, br, spr.color);
                    break;
                }
                case TowerType::Cannon: {
                    // Circle + barrel dot
                    DrawCircleV(tf.position.to_raylib(), r, spr.color);
                    // Barrel: small dark circle on top
                    DrawCircleV({tf.position.x, tf.position.y - r * 0.6f}, r * 0.3f, {50, 50, 50, 255});
                    break;
                }
                case TowerType::Ice: {
                    // Hexagon
                    DrawPoly(tf.position.to_raylib(), 6, r, 0, spr.color);
                    break;
                }
                case TowerType::Lightning: {
                    // 4-point star
                    DrawPoly(tf.position.to_raylib(), 4, r, 45, spr.color);
                    break;
                }
                case TowerType::Poison: {
                    // Circle (organic blob) + drip line
                    DrawCircleV(tf.position.to_raylib(), r, spr.color);
                    DrawLineEx({tf.position.x, tf.position.y + r},
                              {tf.position.x, tf.position.y + r + 6}, 3.0f, spr.color);
                    break;
                }
                case TowerType::Laser: {
                    // Diamond
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
                    DrawCircleV({tf.position.x - 8.0f + i * 10.0f, tf.position.y + hh - 4},
                               3, GOLD);
                }
            }
            // Selection highlight
            if (e == game.play.selected_tower) {
                DrawRectangleLinesEx(
                    {tf.position.x - hw - 2, tf.position.y - hh - 2, spr.width + 4, spr.height + 4},
                    2, WHITE);
            }
        }
    }

    // Hero
    {
        auto view = reg.view<Hero, Transform, Sprite, Health>();
        for (auto [e, hero, tf, spr, hp] : view.each()) {
            // Body
            DrawCircleV(tf.position.to_raylib(), spr.width / 2, spr.color);
            DrawCircleLinesV(tf.position.to_raylib(), spr.width / 2 + 2, WHITE);

            // Health bar
            float bw = 40;
            float bx = tf.position.x - bw / 2;
            float by = tf.position.y - spr.height / 2 - 12;
            DrawRectangle(static_cast<int>(bx), static_cast<int>(by), static_cast<int>(bw), 4, DARKGRAY);
            DrawRectangle(static_cast<int>(bx), static_cast<int>(by),
                         static_cast<int>(bw * hp.ratio()), 4, LIME);

            // Level text
            DrawText(std::format("Lv{}", hero.level).c_str(),
                    static_cast<int>(tf.position.x - 8), static_cast<int>(tf.position.y + spr.height / 2 + 2),
                    10, WHITE);
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
            DrawText(ft.text.c_str(),
                    static_cast<int>(tf.position.x - ft.text.size() * 3),
                    static_cast<int>(tf.position.y - y_off),
                    14, c);
        }
    }
}

// ============================================================
// 17. UI System
// ============================================================
void ui_system(Game& game) {
    auto& ps = game.play;

    // Top HUD bar
    DrawRectangle(0, 0, SCREEN_WIDTH, HUD_HEIGHT, {30, 30, 40, 240});

    DrawText(std::format("Gold: {}", ps.gold).c_str(), 10, 14, 20, GOLD);
    DrawText(std::format("Lives: {}", ps.lives).c_str(), 180, 14, 20,
             ps.lives > 5 ? GREEN : RED);
    DrawText(std::format("Wave: {}/{}", ps.current_wave, MAX_WAVES).c_str(), 340, 14, 20, WHITE);
    DrawText(std::format("Kills: {}", ps.total_kills).c_str(), 520, 14, 20, LIGHTGRAY);

    if (ps.game_speed_fast) {
        DrawText(">> FAST", 680, 14, 20, YELLOW);
    }

    // Difficulty indicator
    const char* diff_names[] = {"EASY", "NORMAL", "HARD"};
    Color diff_colors[] = {GREEN, WHITE, RED};
    int di = static_cast<int>(game.difficulty);
    DrawText(diff_names[di], 680, 30, 12, diff_colors[di]);

    // Hero info
    auto heroes = game.registry.view<Hero, Health>();
    for (auto [e, hero, hp] : heroes.each()) {
        DrawText(std::format("Hero HP: {}/{}", hp.current, hp.max).c_str(), 780, 4, 16, LIME);
        DrawText(std::format("XP: {}/{} Lv{}", hero.xp, hero.xp_to_next, hero.level).c_str(), 780, 22, 14, SKYBLUE);

        // Ability cooldowns
        const char* ability_keys[] = {"Q", "E", "R"};
        const char* ability_names[] = {"Fireball", "Heal", "Lightning"};
        for (int i = 0; i < 3; ++i) {
            int ax = 980 + i * 100;
            Color ac = hero.abilities[i].ready() ? GREEN : DARKGRAY;
            DrawRectangle(ax, 4, 90, 38, {40, 40, 50, 200});
            DrawRectangleLinesEx({static_cast<float>(ax), 4, 90, 38}, 1, ac);
            DrawText(std::format("[{}] {}", ability_keys[i], ability_names[i]).c_str(), ax + 4, 8, 12, ac);
            if (!hero.abilities[i].ready()) {
                DrawText(std::format("{:.1f}s", hero.abilities[i].timer).c_str(), ax + 20, 24, 12, RED);
            } else {
                DrawText("Ready", ax + 20, 24, 12, GREEN);
            }
        }
    }

    // Right panel - tower build menu
    int px = SCREEN_WIDTH - PANEL_WIDTH;
    DrawRectangle(px, HUD_HEIGHT, PANEL_WIDTH, SCREEN_HEIGHT - HUD_HEIGHT, {30, 30, 40, 220});
    DrawText("TOWERS", px + 70, HUD_HEIGHT + 8, 18, WHITE);

    const TowerType tower_types[] = {
        TowerType::Arrow, TowerType::Cannon, TowerType::Ice,
        TowerType::Lightning, TowerType::Poison, TowerType::Laser
    };

    const char* tower_descs[] = {
        "Reliable single-target damage",
        "Slow but deals AoE splash damage",
        "Slows enemies in range",
        "Chains lightning between enemies",
        "Poisons enemies with damage over time",
        "Continuous laser beam with burn"
    };

    const char* effect_descs[] = {
        "",
        "AoE splash",
        "Slow 50%",
        "Chain x2",
        "Poison DoT",
        "Burn DoT"
    };

    for (int i = 0; i < 6; ++i) {
        auto& stats = game.tower_registry.get(tower_types[i], 1);
        int by = HUD_HEIGHT + 35 + i * 55;
        Rectangle btn = {static_cast<float>(px + 10), static_cast<float>(by), PANEL_WIDTH - 20.0f, 50.0f};

        bool affordable = ps.gold >= stats.cost;
        bool is_placing = ps.placing_tower.has_value() && *ps.placing_tower == tower_types[i];
        Color bg = is_placing ? Color{60, 100, 60, 255} : (affordable ? Color{50, 50, 60, 255} : Color{40, 30, 30, 255});
        Color fg = affordable ? WHITE : DARKGRAY;

        DrawRectangleRec(btn, bg);
        DrawRectangleLinesEx(btn, 1, fg);

        // Tower color preview
        DrawRectangle(px + 15, by + 10, 30, 30, stats.color);

        DrawText(stats.name.c_str(), px + 52, by + 5, 16, fg);
        DrawText(std::format("{}g  Dmg:{}", stats.cost, stats.damage).c_str(), px + 52, by + 22, 12, fg);
        float dps = (tower_types[i] == TowerType::Laser)
            ? stats.damage / stats.fire_rate
            : stats.damage * stats.fire_rate;
        DrawText(std::format("Rng:{:.0f} DPS:{:.0f}", stats.range, dps).c_str(), px + 52, by + 35, 10, GRAY);

        // Hover tooltip
        bool hovered = CheckCollisionPointRec(GetMousePosition(), btn);
        if (hovered && !IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            // Draw tooltip to the left of the panel
            int tx = px - 210;
            int ty = by;
            DrawRectangle(tx, ty, 200, 90, {20, 20, 30, 240});
            DrawRectangleLinesEx({static_cast<float>(tx), static_cast<float>(ty), 200, 90}, 1, GOLD);
            DrawText(stats.name.c_str(), tx + 8, ty + 5, 16, GOLD);
            DrawText(tower_descs[i], tx + 8, ty + 24, 10, LIGHTGRAY);
            DrawText(std::format("Damage: {}  Range: {:.0f}", stats.damage, stats.range).c_str(), tx + 8, ty + 40, 11, WHITE);
            DrawText(std::format("DPS: {:.1f}  Rate: {:.2f}/s", dps, stats.fire_rate).c_str(), tx + 8, ty + 54, 11, WHITE);
            if (i > 0) DrawText(effect_descs[i], tx + 8, ty + 70, 11, {200, 200, 100, 255});
        }

        // Click handler
        if (affordable && hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            ps.placing_tower = tower_types[i];
            ps.selected_tower = entt::null;
            game.sounds.play(game.sounds.ui_click);
        }
    }

    // Selected tower info panel
    if (ps.selected_tower != entt::null && game.registry.valid(ps.selected_tower)) {
        auto& tower = game.registry.get<Tower>(ps.selected_tower);
        int iy = HUD_HEIGHT + 380;
        DrawRectangle(px + 5, iy, PANEL_WIDTH - 10, 140, {40, 40, 50, 230});
        DrawText(game.tower_registry.get(tower.type, tower.level).name.c_str(), px + 15, iy + 5, 16, WHITE);
        DrawText(std::format("Level: {}", tower.level).c_str(), px + 15, iy + 25, 14, WHITE);
        DrawText(std::format("Damage: {}", tower.damage).c_str(), px + 15, iy + 42, 14, WHITE);
        DrawText(std::format("Range: {:.0f}", tower.range).c_str(), px + 15, iy + 58, 14, WHITE);

        if (tower.level < TowerRegistry::MAX_LEVEL) {
            int ucost = game.tower_registry.upgrade_cost(tower.type, tower.level);
            Rectangle ubtn = {static_cast<float>(px + 15), static_cast<float>(iy + 78), 85, 25};
            bool can_upgrade = ps.gold >= ucost;
            DrawRectangleRec(ubtn, can_upgrade ? Color{50, 100, 50, 255} : DARKGRAY);
            DrawText(std::format("Upgrade {}g", ucost).c_str(), px + 20, iy + 82, 12, can_upgrade ? WHITE : GRAY);

            if (can_upgrade && CheckCollisionPointRec(GetMousePosition(), ubtn) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                ps.gold -= ucost;
                ps.stats.gold_spent += ucost;
                tower.level++;
                auto& stats = game.tower_registry.get(tower.type, tower.level);
                tower.damage = stats.damage;
                tower.range = stats.range;
                tower.fire_rate = stats.fire_rate;
                tower.aoe_radius = stats.aoe_radius;
                tower.chain_count = stats.chain_count;
                tower.effect = stats.effect;
                tower.effect_duration = stats.effect_duration;
                auto& spr = game.registry.get<Sprite>(ps.selected_tower);
                spr.color = stats.color;
                game.sounds.play(game.sounds.ui_click);
            }
        }

        // Sell button
        int sell_val = tower.cost / 2;
        Rectangle sbtn = {static_cast<float>(px + 110), static_cast<float>(iy + 78), 85, 25};
        DrawRectangleRec(sbtn, {100, 50, 50, 255});
        DrawText(std::format("Sell +{}g", sell_val).c_str(), px + 115, iy + 82, 12, WHITE);

        if (CheckCollisionPointRec(GetMousePosition(), sbtn) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            ps.gold += sell_val;
            ps.stats.towers_sold++;
            auto& gc = game.registry.get<GridCell>(ps.selected_tower);
            ps.tower_positions.erase(gc.pos);
            game.registry.destroy(ps.selected_tower);
            ps.selected_tower = entt::null;
            game.recalculate_path();
            game.sounds.play(game.sounds.ui_click);
        }
    }

    // Wave countdown + preview
    if (!ps.wave_active && ps.current_wave < MAX_WAVES) {
        WaveNum next = ps.current_wave + 1;
        DrawText(std::format("Next wave in {:.1f}s", std::max(0.0f, ps.wave_timer)).c_str(),
                SCREEN_WIDTH / 2 - 80, SCREEN_HEIGHT - 30, 18, YELLOW);
        DrawText("Press SPACE to start early", SCREEN_WIDTH / 2 - 100, SCREEN_HEIGHT - 50, 14, GRAY);

        // Wave preview
        if (next <= game.wave_manager.total_waves()) {
            auto& wave = game.wave_manager.get_wave(next);
            std::string preview = "Wave " + std::to_string(next) + ": ";
            const char* names[] = {"Grunt", "Runner", "Tank", "Healer", "Flying", "Boss"};
            for (size_t s = 0; s < wave.spawns.size(); ++s) {
                if (s > 0) preview += ", ";
                preview += std::to_string(wave.spawns[s].count) + " " + names[static_cast<int>(wave.spawns[s].type)];
            }
            if (wave.is_boss_wave) {
                DrawText(preview.c_str(), SCREEN_WIDTH / 2 - 150, SCREEN_HEIGHT - 70, 14, {255, 100, 100, 255});
                DrawText("!! BOSS WAVE !!", SCREEN_WIDTH / 2 - 55, SCREEN_HEIGHT - 88, 18, RED);
            } else {
                DrawText(preview.c_str(), SCREEN_WIDTH / 2 - 150, SCREEN_HEIGHT - 70, 14, LIGHTGRAY);
            }
        }
    } else if (ps.wave_active) {
        DrawText(std::format("Enemies remaining: {}", ps.enemies_alive).c_str(),
                SCREEN_WIDTH / 2 - 80, SCREEN_HEIGHT - 30, 14, {200, 200, 200, 200});
    }

    // Wave announcement banner
    if (ps.banner.active) {
        float alpha = std::clamp(ps.banner.timer / 0.5f, 0.0f, 1.0f); // fade out in last 0.5s
        if (ps.banner.timer > 2.5f) alpha = std::clamp((3.0f - ps.banner.timer) / 0.5f, 0.0f, 1.0f); // fade in
        int font_size = 48;
        int tw = MeasureText(ps.banner.text.c_str(), font_size);
        Color c = ps.banner.color;
        c.a = static_cast<unsigned char>(255 * alpha);
        DrawText(ps.banner.text.c_str(), SCREEN_WIDTH / 2 - tw / 2, SCREEN_HEIGHT / 2 - 80, font_size, c);
    }

    // Tutorial overlay
    if (ps.tutorial.active && !ps.tutorial.completed) {
        const char* hints[] = {
            "Click a tower from the panel, then click a green tile to place it",
            "Towers auto-attack enemies in range. The circle shows range.",
            "WASD to move hero. Q/E/R for abilities.",
            "Click a tower to upgrade it. Sell for 50% refund.",
            "P to pause, F for fast-forward. Good luck!"
        };
        int step = std::clamp(ps.tutorial.step, 0, 4);
        int tw = MeasureText(hints[step], 16);
        int tx = SCREEN_WIDTH / 2 - tw / 2 - 10;
        int ty = SCREEN_HEIGHT / 2 + 40;
        DrawRectangle(tx - 5, ty - 5, tw + 20, 30, {0, 0, 0, 180});
        DrawRectangleLinesEx({static_cast<float>(tx - 5), static_cast<float>(ty - 5),
                             static_cast<float>(tw + 20), 30.0f}, 1, GOLD);
        DrawText(hints[step], tx + 5, ty + 2, 16, GOLD);
        DrawText("[TAB to dismiss]", SCREEN_WIDTH / 2 - 50, ty + 28, 10, GRAY);
    }

    // Controls help
    DrawText("WASD:Move  Q:Fire  E:Heal  R:Lightning  P:Pause  F:Speed  ESC:Menu",
            10, SCREEN_HEIGHT - 18, 12, {150, 150, 150, 180});
}

} // namespace ls::systems
