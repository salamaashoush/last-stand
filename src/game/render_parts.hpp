#pragma once
#include "core/gfx.hpp"
#include "game/world.hpp"

// Building blocks shared by the in-game renderer and the asset gallery.
namespace ls {

struct Game;

void draw_opaque(const Texture2D& t, Vec2 at);
const Texture2D* ptex(Game& g, int id);
void sprite(const Texture2D& t, Vec2 p, float w, float h, float rot_deg, Color c);
void draw_monster(const Gfx& g, EnemyType type, Vec2 p, float r, float anim, float flash, bool slowed, bool elite,
                  float time, float shield_ratio = 0.0f, float shield_flash = 0.0f, bool armored = false,
                  Color tint = WHITE, float tint_amount = 0.0f, int biome = -1, float hurt = 0.0f,
                  bool frozen = false, bool burrowed = false, bool corroded = false, bool buffed = false,
                  float rage_k = 0.0f);
void draw_dress(const Gfx& g, int biome, Vec2 c, float r, float ry, float time, bool big);
void draw_boss(const Gfx& g, const BossGenome& gen, int biome, Vec2 p, float r, float anim, float flash, bool enraged,
               float time, float shield_ratio, bool phase2 = false);
void draw_soldier(const Gfx& g, Vec2 p, float recoil, UnitType u, float scale = 1.0f, float time = 0.0f,
                  int biome = -1, int level = 1);
void draw_turret(const Gfx& g, TurretType type, int level, Vec2 p, float aim, float recoil, float time, float charge);
void draw_gate(Game& game, const Gate& gate);
void draw_loot(Game& game, const Loot& f);
void draw_op(Vec2 c, char op, float size, Color col);
void draw_wall(const Run& run);
void draw_night(const Theme& theme);
void draw_night_lights(Game& game, const Run& run);
void draw_corpses(Game& game, const Run& run);
void draw_boss_death(Game& game, const Run& run, const BossDeath& b);
void draw_wall_collapse(const Run& run, const Gfx& g);
void draw_particles(Game& game, const Run& run, bool additive);
void draw_shells(Game& game, const Run& run);
void draw_projectiles(Game& game, const Run& run);
void draw_stage_zones(Game& game, const Run& run);

} // namespace ls
