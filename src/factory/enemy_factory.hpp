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
};

inline EnemyStats get_enemy_stats(EnemyType type, float scaling) {
    auto s = [&](int v) { return static_cast<int>(v * scaling); };
    auto sf = [&](float v) { return v; }; // speed doesn't scale

    switch (type) {
        case EnemyType::Grunt:
            return {s(60), sf(55.0f), 0, s(10), {200, 50, 50, 255}, 20.0f};
        case EnemyType::Runner:
            return {s(35), sf(110.0f), 0, s(12), {255, 150, 50, 255}, 16.0f};
        case EnemyType::Tank:
            return {s(200), sf(30.0f), static_cast<int>(5 * scaling), s(25), {100, 100, 180, 255}, 28.0f};
        case EnemyType::Healer:
            return {s(80), sf(45.0f), 0, s(20), {50, 200, 50, 255}, 22.0f};
        case EnemyType::Flying:
            return {s(45), sf(75.0f), 0, s(15), {180, 50, 255, 255}, 18.0f};
        case EnemyType::Boss:
            return {s(1000), sf(25.0f), static_cast<int>(8 * scaling), s(100), {255, 50, 50, 255}, 36.0f};
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
    reg.emplace<Enemy>(e, type, stats.reward);
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
