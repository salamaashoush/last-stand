#pragma once
#include <entt/entt.hpp>
#include "components/components.hpp"

namespace ls {

inline entt::entity create_projectile(
    entt::registry& reg,
    Vec2 origin,
    entt::entity target,
    Vec2 target_pos,
    int damage,
    DamageType dtype,
    float speed,
    float aoe,
    EffectType effect,
    float effect_dur,
    int chain,
    Color color
) {
    auto e = reg.create();
    reg.emplace<Transform>(e, origin);
    reg.emplace<Velocity>(e);
    reg.emplace<Sprite>(e, color, 4, 8.0f, 8.0f, true);
    reg.emplace<Projectile>(e, Projectile{
        .source = entt::null,
        .target = target,
        .target_pos = target_pos,
        .speed = speed,
        .damage = damage,
        .damage_type = dtype,
        .aoe_radius = aoe,
        .effect = effect,
        .effect_duration = effect_dur,
        .chain_count = chain,
        .trail_color = color
    });
    reg.emplace<Lifetime>(e, 5.0f);
    return e;
}

inline entt::entity create_floating_text(entt::registry& reg, Vec2 pos, const std::string& text, Color color) {
    auto e = reg.create();
    reg.emplace<Transform>(e, pos);
    reg.emplace<FloatingText>(e, text, color, 0.0f, 1.0f, 40.0f);
    reg.emplace<Lifetime>(e, 1.0f);
    return e;
}

inline entt::entity create_particle(entt::registry& reg, Vec2 pos, Vec2 vel, Color color, float size, float lifetime, const std::string& texture = "") {
    auto e = reg.create();
    reg.emplace<Transform>(e, pos);
    reg.emplace<Velocity>(e, vel);
    reg.emplace<Particle>(e, color, size, 1.0f, texture);
    reg.emplace<Lifetime>(e, lifetime);
    return e;
}

} // namespace ls
