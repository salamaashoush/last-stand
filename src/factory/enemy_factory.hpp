#pragma once
#include <entt/entt.hpp>
#include "components/components.hpp"
#include "managers/map_manager.hpp"

namespace ls {

struct EnemyStats {
    int hp;
    float speed;
    int armor;
    Gold reward;
    Color color;
    float size;
    int attack_damage;
    float attack_range;
    float attack_cooldown;
};

inline EnemyStats get_enemy_stats(EnemyType type, float scaling) {
    auto s = [&](int v) { return static_cast<int>(v * scaling); };
    auto sf = [&](float v) { return v; }; // speed doesn't scale

    //                 HP         speed   armor                       reward  color                         size    atk  range  cd
    switch (type) {
        case EnemyType::Grunt:
            return {s(80),  sf(60.0f),  0,                          s(10), {200, 50, 50, 255},   20.0f,  s(8),  30.0f, 1.0f};
        case EnemyType::Runner:
            return {s(45),  sf(120.0f), 0,                          s(12), {255, 150, 50, 255},  16.0f,  s(5),  25.0f, 0.6f};
        case EnemyType::Tank:
            return {s(300), sf(32.0f),  static_cast<int>(6*scaling), s(25), {100, 100, 180, 255}, 28.0f, s(20),  35.0f, 1.5f};
        case EnemyType::Healer:
            return {s(100), sf(48.0f),  0,                          s(20), {50, 200, 50, 255},   22.0f,  s(4),  25.0f, 1.2f};
        case EnemyType::Flying:
            return {s(55),  sf(85.0f),  0,                          s(15), {180, 50, 255, 255},  18.0f,  s(6),  20.0f, 0.8f};
        case EnemyType::Boss:
            return {s(1500),sf(28.0f),  static_cast<int>(10*scaling),s(100),{255, 50, 50, 255},  36.0f, s(30),  50.0f, 1.0f};
    }
    return {};
}

inline AbilityType get_boss_ability(WaveNum wave) {
    if (wave <= 5) return AbilityType::SpeedBurst;
    if (wave <= 10) return AbilityType::SpawnMinions;
    return AbilityType::DamageAura;
}

inline entt::entity create_enemy(entt::registry& reg, EnemyType type, const std::vector<Vec2>& path, float scaling, WaveNum wave = 0) {
    if (path.empty()) return entt::null;

    auto stats = get_enemy_stats(type, scaling);
    auto e = reg.create();

    reg.emplace<Transform>(e, path[0]);
    reg.emplace<Velocity>(e);
    reg.emplace<Sprite>(e, stats.color, 3, stats.size, stats.size, true);
    reg.emplace<Health>(e, stats.hp, stats.hp, stats.armor);
    reg.emplace<HealthBarComp>(e);
    reg.emplace<Enemy>(e, type, stats.reward, stats.attack_damage, stats.attack_range, stats.attack_cooldown, 0.0f, stats.size * 0.5f);
    reg.emplace<PathFollower>(e, path, size_t{0}, stats.speed, stats.speed);

    if (type == EnemyType::Flying) {
        reg.emplace<Flying>(e);
    }

    if (type == EnemyType::Healer) {
        reg.emplace<Aura>(e, 80.0f, static_cast<int>(5 * scaling), EffectType::None, 0.0f);
    }

    if (type == EnemyType::Boss) {
        auto ability = get_boss_ability(wave);
        reg.emplace<Boss>(e, 5.0f, 0.0f, "Wave Boss", ability, false, 0.0f);
    }

    return e;
}

} // namespace ls
