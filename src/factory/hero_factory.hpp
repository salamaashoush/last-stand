#pragma once
#include <entt/entt.hpp>
#include "components/components.hpp"
#include "core/constants.hpp"
#include "core/asset_paths.hpp"

namespace ls {

inline entt::entity create_hero(entt::registry& reg, Vec2 pos) {
    auto e = reg.create();
    reg.emplace<Transform>(e, pos);
    reg.emplace<Velocity>(e);
    reg.emplace<Sprite>(e, Color{50, 150, 255, 255}, 6, 20.0f, 20.0f, true);
    reg.emplace<Health>(e, HERO_BASE_HP, HERO_BASE_HP, 5);
    reg.emplace<HealthBarComp>(e);
    reg.emplace<AnimatedSprite>(e, AnimatedSprite{
        .texture_name = assets::HERO_SPRITE,
        .frame_width = 16,
        .frame_height = 16,
        .columns = 4,
        .rows = 7,
        .current_frame = 0,
        .direction = 0,  // facing down
        .frame_timer = 0.0f,
        .frame_speed = 8.0f,
        .anim_frames = {0, 1, 2, 3},  // walk cycle rows
        .display_size = 34.0f,
        .playing = true
    });
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
