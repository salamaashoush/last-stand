#pragma once
#include <entt/entt.hpp>
#include "components/components.hpp"
#include "managers/tower_registry.hpp"
#include "managers/map_manager.hpp"

namespace ls {

inline entt::entity create_tower(entt::registry& reg, const TowerStats& stats, GridPos pos, const MapData& map) {
    auto e = reg.create();
    auto world = map.grid_to_world(pos);

    reg.emplace<Transform>(e, world);
    reg.emplace<GridCell>(e, pos);
    reg.emplace<Sprite>(e, stats.color, 5, 44.0f, 44.0f, true);
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
