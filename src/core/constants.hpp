#pragma once

namespace ls {

// Everything renders into a fixed portrait canvas that is letterboxed into the window,
// so phones, desktop windows and the browser all see the same playfield.
inline constexpr int VIRTUAL_W = 720;
inline constexpr int VIRTUAL_H = 1280;
inline constexpr int TARGET_FPS = 60;

inline constexpr float HUD_TOP = 96.0f;
inline constexpr float SPAWN_Y = 40.0f;
inline constexpr float WALL_TOP = 990.0f;
inline constexpr float WALL_BOTTOM = 1066.0f;
inline constexpr float SQUAD_Y = 1104.0f;
inline constexpr float PANEL_TOP = 1180.0f;

inline constexpr float GATE_W = 172.0f;
inline constexpr float GATE_H = 64.0f;
inline constexpr float GATE_SPEED = 72.0f;

inline constexpr float BULLET_SPEED = 1150.0f;
inline constexpr float BULLET_RADIUS = 5.0f;

inline constexpr int TURRET_SLOTS = 4;
inline constexpr float TURRET_Y = 1018.0f;
inline constexpr float TURRET_SLOT_X[TURRET_SLOTS] = {78.0f, 248.0f, 472.0f, 642.0f};

} // namespace ls
