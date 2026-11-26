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

inline const char* formation_name(Formation f) {
    switch (f) {
    case Formation::Line:
        return "LINE";
    case Formation::Wedge:
        return "WEDGE";
    case Formation::Guard:
        return "GUARD";
    case Formation::Count:
        break;
    }
    return "LINE";
}

inline const char* formation_blurb(Formation f) {
    switch (f) {
    case Formation::Line:
        return "+10% FIRE RATE";
    case Formation::Wedge:
        return "+15% DMG  +15% CRIT";
    case Formation::Guard:
        return "WIDE AIM  -12% WALL DMG";
    case Formation::Count:
        break;
    }
    return "";
}

inline float formation_rate_mult(Formation f) {
    return f == Formation::Line ? 1.10f : 1.0f;
}
inline float formation_damage_mult(Formation f) {
    return f == Formation::Wedge ? 1.15f : 1.0f;
}
inline float formation_crit_bonus(Formation f) {
    return f == Formation::Wedge ? 0.15f : 0.0f;
}
inline float formation_knock_mult(Formation f) {
    return f == Formation::Guard ? 1.25f : 1.0f;
}
inline float formation_wall_resist(Formation f) {
    return f == Formation::Guard ? 0.12f : 0.0f;
}
inline float formation_aim_cone_deg(Formation f) {
    return f == Formation::Guard ? 26.0f : f == Formation::Wedge ? 11.0f : 16.0f;
}

// Gun tip relative to a soldier's centre at SOLDIER_SCALE; render and sim both use it so
// bullets leave the barrel that is drawn. Lengths match draw_soldier's per-unit guns.
inline float gun_length(int unit) {
    constexpr float lengths[] = {17.0f, 20.0f, 16.0f, 27.0f, 14.0f, 15.0f, 19.0f, 18.0f};
    return unit >= 0 && unit < 8 ? lengths[unit] : 17.0f;
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

inline int formation_cols(int visible, Formation f = Formation::Line) {
    int max_c = f == Formation::Line ? 12 : f == Formation::Wedge ? 7 : 8;
    return std::clamp(visible, 1, max_c);
}

inline float formation_spacing_x(Formation f) {
    return f == Formation::Line ? 31.0f : f == Formation::Wedge ? 23.0f : 26.0f;
}

inline float formation_half_width(int visible, Formation f = Formation::Line) {
    if (f == Formation::Wedge) {
        int cols = std::min(visible, 7);
        return static_cast<float>(std::max(0, cols - 1)) * 23.0f * 0.5f + 16.0f;
    }
    return static_cast<float>(formation_cols(visible, f) - 1) * formation_spacing_x(f) * 0.5f + 16.0f;
}

inline Vec2 soldier_offset(int index, int visible, Formation f = Formation::Line) {
    if (f == Formation::Wedge) {
        // Spearhead V-formation: tight ranks widening backward from the point soldier.
        int row = 0;
        int rem = index;
        while (rem >= std::min(7, 1 + row * 2)) {
            rem -= std::min(7, 1 + row * 2);
            row++;
        }
        int in_row = std::min(std::min(7, 1 + row * 2), visible - (index - rem));
        float row_w = static_cast<float>(std::max(0, in_row - 1)) * 23.0f;
        float x = -row_w * 0.5f + static_cast<float>(rem) * 23.0f;
        float y = static_cast<float>(row) * 16.5f - 8.0f + std::abs(x) * 0.08f;
        return {x, std::clamp(y, -10.0f, 58.0f)};
    }
    int cols = formation_cols(visible, f);
    float spx = formation_spacing_x(f);
    float spy = f == Formation::Guard ? 19.0f : SOLDIER_SPACING_Y;
    int row = index / cols;
    int col = index % cols;
    int in_row = std::min(cols, visible - row * cols);
    float row_w = static_cast<float>(in_row - 1) * spx;
    float stagger = (row % 2) ? spx * 0.25f : 0.0f;
    float y_off = f == Formation::Guard ? -6.0f : 0.0f;
    return {-row_w * 0.5f + static_cast<float>(col) * spx + stagger, static_cast<float>(row) * spy + y_off};
}

// Omar stands beside the formation on whichever side has room; the squad badge takes the other.
inline float omar_x(float squad_x, int visible, Formation f = Formation::Line) {
    float half = formation_half_width(visible, f);
    float left = squad_x - half - 24.0f;
    return left > 40.0f ? left : squad_x + half + 24.0f;
}

inline float clamp_squad_x(float x, int visible, Formation f = Formation::Line) {
    float hw = formation_half_width(visible, f);
    return std::clamp(x, hw + 6.0f, static_cast<float>(VIRTUAL_W) - hw - 6.0f);
}

} // namespace ls
