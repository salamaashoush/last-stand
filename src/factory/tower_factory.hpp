#pragma once
#include <entt/entt.hpp>
#include "components/components.hpp"
#include "managers/tower_registry.hpp"
#include "managers/map_manager.hpp"

namespace ls {

inline int tower_max_hp(TowerType type, int level) {
    int base = 100;
    switch (type) {
        case TowerType::Arrow:     base = 80;  break;
        case TowerType::Cannon:    base = 150; break;
        case TowerType::Ice:       base = 90;  break;
        case TowerType::Lightning: base = 100; break;
        case TowerType::Poison:    base = 90;  break;
        case TowerType::Laser:     base = 120; break;
    }
    return base + (level - 1) * 30;
}

inline entt::entity create_tower(entt::registry& reg, const TowerStats& stats, GridPos pos, const MapData& map) {
    auto e = reg.create();
    auto world = map.grid_to_world(pos);

    reg.emplace<Transform>(e, world);
    reg.emplace<GridCell>(e, pos);
    reg.emplace<Sprite>(e, stats.color, 5, 44.0f, 44.0f, true);
    int hp = tower_max_hp(stats.type, stats.level);
    reg.emplace<Health>(e, hp, hp, 0);
    reg.emplace<HealthBarComp>(e, -24.0f, 40.0f, 4.0f);
    reg.emplace<Tower>(e, Tower{
        .type = stats.type,
        .level = stats.level,
        .range = stats.range,
        .fire_rate = stats.fire_rate,
        .cooldown = 0.0f,
        .damage = stats.damage,
        .cost = stats.cost,
        .target = entt::null,
        .effect = stats.effect,
        .effect_duration = stats.effect_duration,
        .aoe_radius = stats.aoe_radius,
        .chain_count = stats.chain_count
    });

    return e;
}

} // namespace ls
