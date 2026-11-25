#pragma once
#include "core/rng.hpp"
#include "core/types.hpp"
#include <algorithm>
#include <cmath>
#include <vector>

namespace ls {

inline constexpr int MAX_SOLDIERS = 999;
inline constexpr int MUL_STEP_POINTS = 12;
inline constexpr int STAT_STEP = 1;
inline constexpr int STAT_MAX = 15;
inline constexpr int STAT_MIN = -40;

// Gates follow the genre's two-colour rule: a blue gate climbs with every bullet that lands,
// a red gate gets worse with every bullet that lands. Add gates carry a signed count; mul
// gates carry a factor where negative values divide (value -2 is "÷2").
struct GateState {
    GateKind kind{GateKind::Add};
    int value{0};
    bool red{false};
    int hit_accum{0};
    int points{0};
};

// Gate growth is paced against the squad's raw fire output, so a 5-man squad and a
// 200-man squad both need several seconds of focused fire to move a gate meaningfully.
inline int hits_per_point(float shots_per_sec) {
    return std::clamp(static_cast<int>(std::lround(shots_per_sec / 1.2f)), 2, 400);
}

inline int add_step(int soldiers) {
    return std::max(1, soldiers / 30);
}

// Shooting can lift a blue add gate to at most +30% of the squad, so growth stays a reward
// rather than an exponential that outruns every enemy curve.
inline int add_cap(int soldiers) {
    return std::max(3, soldiers * 3 / 10);
}

// Returns true when the gate's displayed value changed.
inline bool gate_hit(GateState& g, int hits_needed, float growth, int soldiers) {
    g.hit_accum += 1;
    if (g.hit_accum < hits_needed) return false;
    g.hit_accum = 0;
    int pts = std::max(1, static_cast<int>(std::lround(growth)));
    g.points += pts;
    if (g.kind == GateKind::Add) {
        if (g.red) {
            g.value -= pts * add_step(soldiers);
            return true;
        }
        int cap = add_cap(soldiers);
        if (g.value >= cap) return false;
        g.value = std::min(cap, g.value + pts * add_step(soldiers));
        return true;
    }
    if (g.kind == GateKind::Rate || g.kind == GateKind::Power) {
        int before = g.value;
        g.value = g.red ? std::max(STAT_MIN, g.value - STAT_STEP * pts) : std::min(STAT_MAX, g.value + STAT_STEP * pts);
        return g.value != before;
    }
    if (g.points < MUL_STEP_POINTS) return false;
    g.points = 0;
    if (g.red) {
        if (g.value > -4) {
            g.value -= 1;
            return true;
        }
        return false;
    }
    if (g.value < 3) {
        g.value += 1;
        return true;
    }
    return false;
}

inline int apply_gate(int soldiers, const GateState& g) {
    long long out = soldiers;
    if (g.kind == GateKind::Rate || g.kind == GateKind::Power) return soldiers;
    if (g.kind == GateKind::Add) {
        out = soldiers + static_cast<long long>(g.value);
    } else if (g.value > 0) {
        out = static_cast<long long>(soldiers) * g.value;
    } else if (g.value < 0) {
        int d = -g.value;
        out = (soldiers + d - 1) / d;
    }
    return static_cast<int>(std::clamp(out, 1LL, static_cast<long long>(MAX_SOLDIERS)));
}

inline bool gate_is_good(const GateState& g) {
    return g.value > 0 && !(g.kind == GateKind::Mul && g.value == 1);
}

// Every row spans the field, so the squad passes exactly one gate. Each row has at least one
// blue gate that starts below zero and has to be shot positive in time, and at least one red
// trap that only gets worse if the squad fires into it.
inline std::vector<GateState> roll_gate_row(Rng& rng, int wave, int soldiers, int lanes) {
    std::vector<GateState> row;
    int scale = add_step(soldiers);
    int blue_lane = rng.range(0, lanes - 1);
    int red_lane = (blue_lane + rng.range(1, lanes - 1)) % lanes;
    for (int i = 0; i < lanes; ++i) {
        GateState g;
        g.red = i == red_lane || (i != blue_lane && rng.chance(0.5f));
        if (rng.chance(0.15f)) {
            // Stat gates change the whole squad's guns for the rest of the run, in percent.
            g.kind = rng.chance(0.5f) ? GateKind::Rate : GateKind::Power;
            g.value = g.red ? -rng.range(4, 10) : -rng.range(2, 8);
        } else if (rng.chance(0.14f)) {
            g.kind = GateKind::Mul;
            g.value = g.red ? -2 : (rng.chance(0.4f) ? 2 : 1);
        } else if (g.red) {
            g.value = -rng.range(2, 6 + wave) * scale;
        } else {
            g.value = -rng.range(2, 6 + wave / 2) * scale;
        }
        row.push_back(g);
    }
    return row;
}

} // namespace ls
