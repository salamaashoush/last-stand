#pragma once

namespace ls {

inline constexpr int SCREEN_WIDTH = 1280;
inline constexpr int SCREEN_HEIGHT = 720;
inline constexpr int TARGET_FPS = 60;

inline constexpr int TILE_SIZE = 48;
inline constexpr int GRID_OFFSET_X = 0;
inline constexpr int GRID_OFFSET_Y = 0;
inline constexpr int GRID_COLS = 48;
inline constexpr int GRID_ROWS = 24;

inline constexpr int STARTING_GOLD = 0;
inline constexpr int STARTING_LIVES = 20;
inline constexpr int MAX_WAVES = 30;
inline constexpr int BOSS_WAVE_INTERVAL = 5;

inline constexpr float HERO_SPEED = 280.0f;
inline constexpr float HERO_ATTACK_RANGE = 140.0f;
inline constexpr float HERO_ATTACK_COOLDOWN = 0.3f;
inline constexpr int HERO_BASE_DAMAGE = 15;
inline constexpr int HERO_BASE_HP = 200;
inline constexpr int HERO_XP_PER_LEVEL = 100;

inline constexpr float PROJECTILE_SPEED = 500.0f;
inline constexpr float FLOATING_TEXT_DURATION = 1.0f;
inline constexpr float FLOATING_TEXT_SPEED = 40.0f;

inline constexpr int HUD_HEIGHT = 48;
inline constexpr int PANEL_WIDTH = 220;

inline constexpr float WAVE_DELAY = 2.0f;
inline constexpr float SPAWN_INTERVAL = 0.35f;

} // namespace ls
