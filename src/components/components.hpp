#pragma once
#include <vector>
#include <string>
#include <optional>
#include <entt/entt.hpp>
#include "core/types.hpp"

namespace ls {

// === Spatial ===

struct Transform {
    Vec2 position{};
    float rotation{};
    float scale{1.0f};
};

struct Velocity {
    Vec2 vel{};
};

struct GridCell {
    GridPos pos{};
};

// === Visual ===

struct Sprite {
    Color color{WHITE};
    int layer{0};
    float width{TILE_SIZE};
    float height{TILE_SIZE};
    bool visible{true};
    std::string texture_name{};

    static constexpr int TILE_SIZE = 48;
};

struct HealthBarComp {
    float offset_y{-8.0f};
    float width{40.0f};
    float height{4.0f};
};

struct FloatingText {
    std::string text;
    Color color{WHITE};
    float timer{};
    float max_time{1.0f};
    float speed{40.0f};
};

struct Particle {
    Color color{WHITE};
    float size{4.0f};
    float decay{1.0f};
    std::string particle_texture{};
};

struct AnimatedSprite {
    std::string texture_name{};
    int frame_width{16};      // width of one frame in pixels
    int frame_height{16};     // height of one frame in pixels
    int columns{4};           // columns in spritesheet
    int rows{7};              // rows in spritesheet
    int current_frame{0};     // current animation frame index (into anim_frames)
    int direction{0};         // column index: 0=down, 1=up, 2=left, 3=right
    float frame_timer{0.0f};
    float frame_speed{6.0f};  // frames per second
    std::vector<int> anim_frames{0, 1, 2, 3}; // row indices for current animation
    float display_size{40.0f}; // rendered size on screen
    bool playing{true};
};

// === Combat ===

struct Health {
    int current{};
    int max{};
    int armor{};

    float ratio() const { return max > 0 ? static_cast<float>(current) / max : 0.0f; }
};

struct Damage {
    int amount{};
    DamageType type{DamageType::Physical};
};

struct Effect {
    EffectType type{EffectType::None};
    float duration{};
    float tick_timer{};
    float tick_interval{0.5f};
    int tick_damage{};
    float slow_factor{1.0f};
};

struct Aura {
    float radius{80.0f};
    int heal_per_sec{};
    EffectType effect{EffectType::None};
    float effect_duration{};
};

// === Tower ===

struct Tower {
    TowerType type{};
    int level{1};
    float range{};
    float fire_rate{};
    float cooldown{};
    int damage{};
    int cost{};
    entt::entity target{entt::null};
    EffectType effect{EffectType::None};
    float effect_duration{};
    float aoe_radius{};
    int chain_count{};
};

// === Projectile ===

struct Projectile {
    entt::entity source{entt::null};
    entt::entity target{entt::null};
    Vec2 target_pos{};
    float speed{300.0f};
    int damage{};
    DamageType damage_type{DamageType::Physical};
    float aoe_radius{};
    EffectType effect{EffectType::None};
    float effect_duration{};
    int chain_count{};
    Color trail_color{WHITE};
};

// === Enemy ===

struct Enemy {
    EnemyType type{};
    Gold reward{};
    int attack_damage{5};
    float attack_range{30.0f};
    float attack_cooldown{1.0f};
    float attack_timer{0.0f};
    float collision_radius{10.0f};
};

struct PathFollower {
    std::vector<Vec2> path;
    size_t current_index{};
    float speed{};
    float base_speed{};
};

struct Boss {
    float ability_cooldown{5.0f};
    float ability_timer{};
    std::string name;
    AbilityType boss_ability{AbilityType::SpeedBurst};
    bool ability_active{false};
    float ability_duration{0.0f};
};

struct AttackFlash {
    float timer{0.0f};
};

struct Flying {};

// === Hero ===

struct Ability {
    AbilityId id{};
    float cooldown{};
    float timer{};
    int damage{};
    float radius{};
    float duration{};
    bool ready() const { return timer <= 0.0f; }
};

struct Hero {
    int level{1};
    int xp{};
    int xp_to_next{100};
    float attack_cooldown{};
    Ability abilities[3]{};
};

// === Utility ===

struct Lifetime {
    float remaining{};
};

struct Selected {};
struct Dead {};
struct Hovered {};

struct Coin {
    Gold value{};
    float bob_timer{0.0f};
    float pickup_radius{20.0f};
};

} // namespace ls
