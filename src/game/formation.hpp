#pragma once
#include "core/constants.hpp"
#include "core/types.hpp"
#include <algorithm>
#include <cmath>

namespace ls {

// Past this many bodies the squad stops drawing new soldiers and instead fires harder,
// which keeps 900-soldier squads readable and the bullet count bounded.
inline constexpr int MAX_VISIBLE_SOLDIERS = 40;
inline constexpr int FORMATION_COLS = 10;
inline constexpr float SOLDIER_SPACING_X = 30.0f;
inline constexpr float SOLDIER_SPACING_Y = 22.0f;

inline constexpr float SOLDIER_SCALE = 1.3f;
inline constexpr float TURRET_BARREL = 40.0f;
inline constexpr Vec2 TESLA_ORB{0.0f, -18.0f};

// Gun tip relative to a soldier's centre at SOLDIER_SCALE; render and sim both use it so
// bullets leave the barrel that is drawn. Lengths match draw_soldier's per-unit guns.
inline float gun_length(int unit) {
    constexpr float lengths[] = {17.0f, 20.0f, 16.0f, 27.0f, 14.0f, 15.0f};
    return unit >= 0 && unit < 6 ? lengths[unit] : 17.0f;
}

inline Vec2 muzzle_offset(int unit, float scale = SOLDIER_SCALE) {
    return {6.5f * scale, (-2.0f - gun_length(unit)) * scale};
}

inline int visible_soldiers(int count) {
    return std::clamp(count, 0, MAX_VISIBLE_SOLDIERS);
}

// Sublinear so a 900-man squad is overwhelming without making gates the only thing that matters.
inline float overflow_mult(int count) {
    if (count <= MAX_VISIBLE_SOLDIERS) return 1.0f;
    return std::pow(static_cast<float>(count) / static_cast<float>(MAX_VISIBLE_SOLDIERS), 0.55f);
}

inline int formation_cols(int visible) {
    return std::clamp(visible, 1, FORMATION_COLS);
}

inline float formation_half_width(int visible) {
    return static_cast<float>(formation_cols(visible) - 1) * SOLDIER_SPACING_X * 0.5f + 16.0f;
}

inline Vec2 soldier_offset(int index, int visible) {
    int cols = formation_cols(visible);
    int row = index / cols;
    int col = index % cols;
    int in_row = std::min(cols, visible - row * cols);
    float row_w = static_cast<float>(in_row - 1) * SOLDIER_SPACING_X;
    float stagger = (row % 2) ? SOLDIER_SPACING_X * 0.25f : 0.0f;
    return {-row_w * 0.5f + static_cast<float>(col) * SOLDIER_SPACING_X + stagger,
            static_cast<float>(row) * SOLDIER_SPACING_Y};
}

inline float clamp_squad_x(float x, int visible) {
    float hw = formation_half_width(visible);
    return std::clamp(x, hw + 6.0f, static_cast<float>(VIRTUAL_W) - hw - 6.0f);
}

} // namespace ls
