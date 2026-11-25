#pragma once

#include "game/cards.hpp"
#include "game/theme.hpp"
#include <raylib.h>

namespace ls {

struct Game;

void draw_card_face(Game& game, CardId id, Rectangle rect, int level);

void build_ground(Game& game, const Theme& theme);
bool ground_ready(int key);
void flush_decals(Game& game);
void unload_ground();
void render_world(Game& game);
void render_hud(Game& game);
void draw_enemy_preview(Game& game, int type, float x, float y, float scale, float anim);
void draw_soldier_preview(Game& game, float x, float y, float scale, int weapon);
void draw_backdrop(Game& game, const Theme& theme, float time);
void draw_turret_preview(Game& game, int type, int level, float x, float y, float aim);
void gallery_prepare(Game& game, int page);
void render_gallery(Game& game, int page);
int gallery_page_count();

} // namespace ls
