#pragma once

// Forward declare Game
namespace ls { struct Game; }

namespace ls::systems {

void hero_system(Game& game, float dt);
void enemy_spawn_system(Game& game, float dt);
void path_follow_system(Game& game, float dt);
void movement_system(Game& game, float dt);
void tower_targeting_system(Game& game, float dt);
void tower_attack_system(Game& game, float dt);
void projectile_system(Game& game, float dt);
void aura_system(Game& game, float dt);
void effect_system(Game& game, float dt);
void damage_system(Game& game, float dt);
void health_system(Game& game, float dt);
void economy_system(Game& game, float dt);
void collision_system(Game& game, float dt);
void lifetime_system(Game& game, float dt);
void particle_system(Game& game, float dt);
void boss_system(Game& game, float dt);
void enemy_combat_system(Game& game, float dt);
void body_collision_system(Game& game, float dt);
void tower_health_system(Game& game, float dt);
void animated_sprite_system(Game& game, float dt);
void coin_system(Game& game, float dt);
void render_system(Game& game);
void ui_system(Game& game);

} // namespace ls::systems
