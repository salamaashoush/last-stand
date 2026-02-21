#pragma once
#include <entt/entt.hpp>
#include "components/components.hpp"
#include "core/constants.hpp"

namespace ls {

inline entt::entity create_hero(entt::registry& reg, Vec2 pos) {
    auto e = reg.create();
    reg.emplace<Transform>(e, pos);
    reg.emplace<Velocity>(e);
    reg.emplace<Sprite>(e, Color{50, 150, 255, 255}, 6, 28.0f, 28.0f, true);
    reg.emplace<Health>(e, HERO_BASE_HP, HERO_BASE_HP, 5);
    reg.emplace<HealthBarComp>(e);
    reg.emplace<Hero>(e, Hero{
        .level = 1,
        .xp = 0,
        .xp_to_next = HERO_XP_PER_LEVEL,
        .attack_cooldown = 0.0f,
        .abilities = {
            Ability{AbilityId::Fireball, 8.0f, 0.0f, 80, 64.0f, 0.0f},
            Ability{AbilityId::HealAura, 12.0f, 0.0f, 0, 100.0f, 3.0f},
            Ability{AbilityId::LightningStrike, 15.0f, 0.0f, 120, 48.0f, 0.0f}
        }
    });
    return e;
}

} // namespace ls
