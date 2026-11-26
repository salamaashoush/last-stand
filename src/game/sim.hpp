#pragma once
#include "game/world.hpp"
#include <string>

namespace ls {

struct Game;

void start_run(Game& game);
void update_run(Game& game, float dt);
void pick_card(Game& game, int index);
void reroll_cards(Game& game);
void build_or_upgrade(Game& game, int slot, TurretType type);
void sell_turret(Game& game, int slot);
void trigger_airstrike(Game& game);
void trigger_frenzy(Game& game);
void trigger_ali(Game& game);
void trigger_salama(Game& game);
void cycle_formation(Game& game);
void repair_wall(Game& game);
int repair_cost(const Run& run);
int wall_tier(const Run& run);
float squad_power_estimate(const Run& run);

// Endless mode runs past wave 10 by rolling stage difficulty forward every ten waves.
struct Difficulty {
    int stage;
    int wave;
};
Difficulty difficulty_for(const Run& run);

// Hooks the asset gallery uses to stage effects in isolation.
void gallery_explosion(Run& run, Vec2 p, float radius);
void gallery_kill(Game& game, int enemy_index);
void gallery_chain(Game& game, int first, int count, Vec2 origin);
void gallery_gate_pass(Game& game, Gate& gate);
void gallery_step(Run& run, float dt);
void gallery_step_full(Game& game, float dt);
void gallery_boss_death(Game& game, Vec2 p, float r);
void gallery_corpses(Game& game, int n);
int theme_key_for(const Run& run);
void apply_theme(Run& run);
std::string loot_label(const Loot& f);

} // namespace ls
