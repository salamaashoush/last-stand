#pragma once
#include "core/constants.hpp"
#include "core/rng.hpp"
#include "core/types.hpp"
#include <algorithm>
#include <cmath>
#include <vector>

namespace ls {

inline constexpr int WAVES_PER_STAGE = 10;

enum class WaveMod : uint8_t { None, Fast, Armored, Swarm, Elite, Giants, Regenerating, Count };

struct WaveModDef {
    const char* name;
    const char* desc;
};

inline constexpr WaveModDef WAVE_MOD_DEFS[] = {
    {"", ""},
    {"FAST HORDE", "ENEMIES MOVE 35% FASTER"},
    {"IRONCLAD", "ENEMIES BLOCK 35% DAMAGE"},
    {"SWARM", "TWICE AS MANY, HALF AS TOUGH"},
    {"ELITE GUARD", "ELITES EVERYWHERE"},
    {"GIANTS", "FEWER, BIGGER, MEANER"},
    {"REGENERATION", "FOES MEND WOUNDS OVER TIME"},
};

inline const WaveModDef& wave_mod_def(WaveMod m) {
    return WAVE_MOD_DEFS[static_cast<size_t>(m)];
}
inline constexpr int MINI_BOSS_WAVE = 5;

struct EnemyStats {
    float hp;
    float speed;
    float radius;
    float wall_dps;
    float knock_resist;
    int xp;
    int coins;
};

inline EnemyStats base_stats(EnemyType t) {
    switch (t) {
    case EnemyType::Walker:
        return {10, 48, 20, 4, 0.0f, 1, 1};
    case EnemyType::Runner:
        return {7, 120, 16, 3, 0.0f, 1, 1};
    case EnemyType::Brute:
        return {130, 30, 36, 14, 0.8f, 6, 5};
    case EnemyType::Exploder:
        return {30, 56, 25, 0, 0.3f, 3, 2};
    case EnemyType::Spitter:
        return {42, 40, 23, 0, 0.3f, 4, 3};
    case EnemyType::Shielder:
        return {34, 38, 22, 5, 0.6f, 3, 2};
    case EnemyType::Splitter:
        return {44, 42, 25, 5, 0.2f, 3, 2};
    case EnemyType::Flyer:
        return {6, 95, 16, 3, 0.0f, 1, 1};
    case EnemyType::Barrel:
        return {18, 34, 18, 0, 1.0f, 0, 2};
    case EnemyType::Boss:
        return {1900, 17, 84, 45, 1.0f, 60, 60};
    case EnemyType::Healer:
        return {46, 36, 23, 3, 0.35f, 5, 4};
    case EnemyType::Burrower:
        return {38, 62, 21, 7, 0.4f, 4, 3};
    case EnemyType::Berserker:
        return {74, 54, 27, 11, 0.55f, 5, 4};
    case EnemyType::Warlord:
        return {110, 34, 30, 10, 0.7f, 7, 6};
    case EnemyType::Count:
        break;
    }
    return {1, 1, 1, 0, 0, 0, 0};
}

// Stage 1 is a gentle on-ramp and stage 2 eases into the 1.5x-per-stage curve.
inline float stage_hp_mult(int stage) {
    if (stage <= 1) return 1.6f;
    if (stage == 2) return 2.6f;
    return 3.4f * std::pow(1.5f, static_cast<float>(stage - 3));
}

inline float wave_hp_mult(int wave) {
    return std::pow(1.25f, static_cast<float>(std::max(wave, 1) - 1));
}

inline float enemy_hp(EnemyType t, int stage, int wave, bool mini_boss = false) {
    float hp = base_stats(t).hp * stage_hp_mult(stage) * wave_hp_mult(wave);
    return mini_boss ? hp * 0.4f : hp;
}

inline float xp_mult(int wave) {
    return 1.0f + 0.03f * static_cast<float>(std::max(wave, 1) - 1);
}

inline int xp_for_level(int level) {
    return 25 + 15 * level + 3 * level * level;
}

inline int gems_for_run(int stage, int waves_cleared, int bosses_killed, bool won) {
    int gems = waves_cleared * (3 + 2 * stage) + bosses_killed * 15 * stage;
    if (won) gems += 40 * stage;
    return gems;
}

struct SpawnEvent {
    float time;
    EnemyType type;
    float x;
    bool mini_boss{false};
};

struct WaveCounts {
    int walkers{};
    int runners{};
    int brutes{};
    int exploders{};
    int spitters{};
    int shielders{};
    int splitters{};
    int flyers{};
    int barrels{};
    int healers{};
    int burrowers{};
    int berserkers{};
    int warlords{};
    int bosses{};
    bool mini_boss{false};

    int total() const {
        return walkers + runners + brutes + exploders + spitters + shielders + splitters + flyers + barrels + healers +
               burrowers + berserkers + warlords + bosses;
    }
};

// Handcrafted 10-stage enemy composition: paced enemy introductions across Stages 1-6,
// then distinct tactical battle archetypes across Stages 7-10 (plus full Endless scaling!).
inline WaveCounts wave_counts(int stage, int wave) {
    float scale = 1.0f + 0.22f * static_cast<float>(std::max(stage, 1) - 1);
    auto sc = [&](float n) { return std::max(0, static_cast<int>(std::round(n * scale))); };
    WaveCounts c;
    c.walkers = sc(22.0f + 10.0f * static_cast<float>(wave));
    c.runners = wave >= 2 ? sc(6.0f + 3.5f * static_cast<float>(wave)) : 0;
    c.brutes = wave >= 3 ? sc(static_cast<float>((wave - 1) / 2)) : 0;
    c.barrels = wave >= 2 ? 2 + wave / 3 : 0;

    // Paced roster introductions: every stage 1..6 unveils brand-new enemy threats!
    if (stage == 1) {
        // Stage 1 (Sunlit Outpost): Classic frontier — Walkers, Runners, Brutes + Barrels (plus Splitters late)
        c.splitters = wave >= 7 ? sc(static_cast<float>(wave / 3)) : 0;
    } else if (stage == 2) {
        // Stage 2 (Golden Dunes): Sky & Sand Ambush — introduces Flyers & Burrowers!
        c.flyers = wave >= 2 ? sc(8.0f + 2.5f * static_cast<float>(wave)) : 0;
        c.burrowers = wave >= 3 ? sc(2.0f + static_cast<float>(wave) * 0.45f) : 0;
    } else if (stage == 3) {
        // Stage 3 (Crystal Glacier): Frost Phalanx — introduces Shielders & Splitters!
        c.shielders = wave >= 2 ? sc(2.0f + static_cast<float>(wave) * 0.65f) : 0;
        c.splitters = wave >= 3 ? sc(2.0f + static_cast<float>(wave) * 0.55f) : 0;
        c.flyers = wave >= 5 ? sc(4.0f + static_cast<float>(wave)) : 0;
    } else if (stage == 4) {
        // Stage 4 (Obsidian Caldera): Volcanic Stampede — introduces Exploders & Berserkers!
        c.exploders = wave >= 2 ? sc(3.0f + static_cast<float>(wave) * 0.6f) : 0;
        c.berserkers = wave >= 3 ? sc(2.0f + static_cast<float>(wave) * 0.45f) : 0;
        c.shielders = wave >= 4 ? sc(1.0f + static_cast<float>(wave) * 0.35f) : 0;
        c.burrowers = wave >= 6 ? sc(1.0f + static_cast<float>(wave) * 0.3f) : 0;
    } else if (stage == 5) {
        // Stage 5 (Venom Hollow): Plague Coven — introduces Spitters & Healers!
        c.spitters = wave >= 2 ? sc(1.5f + static_cast<float>(wave) * 0.45f) : 0;
        c.healers = wave >= 3 ? sc(1.5f + static_cast<float>(wave) * 0.35f) : 0;
        c.splitters = wave >= 3 ? sc(1.5f + static_cast<float>(wave) * 0.4f) : 0;
        c.exploders = wave >= 5 ? sc(2.0f + static_cast<float>(wave) * 0.35f) : 0;
        c.flyers = wave >= 4 ? sc(4.0f + static_cast<float>(wave)) : 0;
    } else if (stage == 6) {
        // Stage 6 (Twilight Citadel): Arcane Legion — introduces Warlords!
        c.warlords = wave >= 3 ? sc(1.0f + static_cast<float>(wave - 2) * 0.38f) : 0;
        c.shielders = wave >= 2 ? sc(2.0f + static_cast<float>(wave) * 0.45f) : 0;
        c.healers = wave >= 4 ? sc(1.0f + static_cast<float>(wave) * 0.3f) : 0;
        c.spitters = wave >= 4 ? sc(1.0f + static_cast<float>(wave) * 0.35f) : 0;
        c.flyers = wave >= 3 ? sc(5.0f + static_cast<float>(wave) * 1.4f) : 0;
        c.berserkers = wave >= 5 ? sc(1.5f + static_cast<float>(wave) * 0.3f) : 0;
    } else if (stage == 7) {
        // Stage 7 (Blood-Moon Woods): Night Blitz — hyper-fast Runners, Flyers, Burrowers & Berserkers!
        c.walkers = sc(14.0f + 6.0f * static_cast<float>(wave));
        c.runners = sc(12.0f + 5.5f * static_cast<float>(wave));
        c.flyers = sc(9.0f + 2.8f * static_cast<float>(wave));
        c.burrowers = wave >= 2 ? sc(2.5f + static_cast<float>(wave) * 0.5f) : 0;
        c.berserkers = wave >= 3 ? sc(2.5f + static_cast<float>(wave) * 0.45f) : 0;
        c.exploders = wave >= 4 ? sc(2.0f + static_cast<float>(wave) * 0.35f) : 0;
        c.warlords = wave >= 6 ? sc(1.0f + static_cast<float>(wave - 4) * 0.25f) : 0;
    } else if (stage == 8) {
        // Stage 8 (Pharaoh's Crypt): Cursed Phalanx — heavy Brutes, Shielders, Warlords, Healers & Spitters!
        c.walkers = sc(16.0f + 7.0f * static_cast<float>(wave));
        c.brutes = wave >= 2 ? sc(1.5f + static_cast<float>(wave) * 0.65f) : 0;
        c.shielders = sc(3.0f + static_cast<float>(wave) * 0.7f);
        c.warlords = wave >= 2 ? sc(1.5f + static_cast<float>(wave) * 0.38f) : 0;
        c.healers = wave >= 3 ? sc(1.5f + static_cast<float>(wave) * 0.35f) : 0;
        c.spitters = wave >= 3 ? sc(1.5f + static_cast<float>(wave) * 0.4f) : 0;
        c.burrowers = wave >= 5 ? sc(1.5f + static_cast<float>(wave) * 0.3f) : 0;
    } else if (stage == 9) {
        // Stage 9 (Inferno Abyss): Hellfire Cataclysm — Exploders, Berserkers, Splitters, Spitters & Healers!
        c.exploders = sc(3.5f + static_cast<float>(wave) * 0.65f);
        c.berserkers = wave >= 2 ? sc(2.5f + static_cast<float>(wave) * 0.5f) : 0;
        c.splitters = wave >= 2 ? sc(2.0f + static_cast<float>(wave) * 0.45f) : 0;
        c.spitters = wave >= 3 ? sc(1.5f + static_cast<float>(wave) * 0.4f) : 0;
        c.healers = wave >= 4 ? sc(1.5f + static_cast<float>(wave) * 0.35f) : 0;
        c.warlords = wave >= 4 ? sc(1.0f + static_cast<float>(wave - 2) * 0.35f) : 0;
        c.barrels = 4 + wave / 2;
    } else {
        // Stage 10 (Eclipse Throne) & Endless: Full combined-arms Sovereign's Legion!
        c.exploders = wave >= 2 ? sc(2.5f + static_cast<float>(wave) * 0.5f) : 0;
        c.spitters = wave >= 3 ? sc(1.5f + static_cast<float>(wave) * 0.4f) : 0;
        c.splitters = wave >= 2 ? sc(1.5f + static_cast<float>(wave) * 0.45f) : 0;
        c.shielders = wave >= 2 ? sc(2.0f + static_cast<float>(wave) * 0.55f) : 0;
        c.flyers = wave >= 2 ? sc(6.0f + static_cast<float>(wave) * 2.0f) : 0;
        c.healers = wave >= 3 ? sc(1.5f + static_cast<float>(wave) * 0.35f) : 0;
        c.burrowers = wave >= 3 ? sc(1.5f + static_cast<float>(wave) * 0.35f) : 0;
        c.berserkers = wave >= 3 ? sc(1.5f + static_cast<float>(wave) * 0.4f) : 0;
        c.warlords = wave >= 3 ? sc(1.5f + static_cast<float>(wave - 1) * 0.38f) : 0;
    }

    if (wave == MINI_BOSS_WAVE) {
        c.bosses = 1;
        c.mini_boss = true;
    } else if (wave == WAVES_PER_STAGE) {
        c.bosses = 1;
    }
    return c;
}

inline float wave_duration(int wave) {
    return 18.0f + 1.6f * static_cast<float>(wave);
}

inline std::vector<SpawnEvent> build_wave(int stage, int wave, Rng& rng) {
    constexpr float margin = 40.0f;
    constexpr float lo_x = margin;
    constexpr float hi_x = VIRTUAL_W - margin;
    auto counts = wave_counts(stage, wave);
    float duration = wave_duration(wave);
    std::vector<SpawnEvent> out;
    out.reserve(static_cast<size_t>(counts.total()));

    // Most of a wave arrives as surges: tight clumps that read as a horde on screen.
    // The rest trickles in so the field is never empty between surges.
    auto emit = [&](EnemyType type, int count) {
        if (count <= 0) return;
        int surged = count * 2 / 3;
        int trickle = count - surged;
        int surges = std::max(1, std::min(4, surged / 8));
        for (int s = 0; s < surges && surged > 0; ++s) {
            int in_surge = (s == surges - 1) ? surged : surged / (surges - s);
            surged -= in_surge;
            float t0 = duration * (0.08f + 0.84f * (static_cast<float>(s) + rng.unit()) / static_cast<float>(surges));
            float cx = rng.uniform(lo_x + 80.0f, hi_x - 80.0f);
            bool line = rng.chance(0.3f);
            for (int i = 0; i < in_surge; ++i) {
                float x = line ? lo_x + (hi_x - lo_x) * (static_cast<float>(i) + 0.5f) / static_cast<float>(in_surge)
                               : cx + rng.uniform(-110.0f, 110.0f);
                float t = t0 + (line ? rng.uniform(0.0f, 0.4f) : rng.uniform(0.0f, 1.8f));
                out.push_back({std::min(t, duration), type, std::clamp(x, lo_x, hi_x)});
            }
        }
        for (int i = 0; i < trickle; ++i) {
            out.push_back({rng.uniform(0.0f, duration), type, rng.uniform(lo_x, hi_x)});
        }
    };

    emit(EnemyType::Walker, counts.walkers);
    emit(EnemyType::Runner, counts.runners);
    for (int i = 0; i < counts.brutes; ++i) {
        out.push_back({duration * (0.2f + 0.7f * rng.unit()), EnemyType::Brute, rng.uniform(lo_x + 40, hi_x - 40)});
    }
    for (int i = 0; i < counts.exploders; ++i) {
        out.push_back({duration * (0.15f + 0.8f * rng.unit()), EnemyType::Exploder, rng.uniform(lo_x, hi_x)});
    }
    emit(EnemyType::Splitter, counts.splitters);
    emit(EnemyType::Shielder, counts.shielders);
    emit(EnemyType::Flyer, counts.flyers);
    for (int i = 0; i < counts.healers; ++i) {
        out.push_back({duration * (0.18f + 0.65f * rng.unit()), EnemyType::Healer, rng.uniform(lo_x + 50, hi_x - 50)});
    }
    for (int i = 0; i < counts.burrowers; ++i) {
        out.push_back({duration * (0.12f + 0.75f * rng.unit()), EnemyType::Burrower, rng.uniform(lo_x + 30, hi_x - 30)});
    }
    for (int i = 0; i < counts.berserkers; ++i) {
        out.push_back(
            {duration * (0.16f + 0.72f * rng.unit()), EnemyType::Berserker, rng.uniform(lo_x + 30, hi_x - 30)});
    }
    for (int i = 0; i < counts.warlords; ++i) {
        out.push_back({duration * (0.22f + 0.6f * rng.unit()), EnemyType::Warlord, rng.uniform(lo_x + 60, hi_x - 60)});
    }
    // Barrels drift in the thick of a surge, where a well-timed shot clears the most.
    for (int i = 0; i < counts.barrels; ++i) {
        out.push_back({duration * (0.15f + 0.7f * rng.unit()), EnemyType::Barrel, rng.uniform(lo_x + 60, hi_x - 60)});
    }
    for (int i = 0; i < counts.spitters; ++i) {
        out.push_back({duration * (0.1f + 0.6f * rng.unit()), EnemyType::Spitter, rng.uniform(lo_x, hi_x)});
    }
    if (counts.bosses > 0) {
        out.push_back({duration * 0.35f, EnemyType::Boss, VIRTUAL_W * 0.5f, counts.mini_boss});
    }

    std::ranges::sort(out, {}, &SpawnEvent::time);
    return out;
}

inline WaveMod roll_wave_mod(int stage, int wave, Rng& rng) {
    if (wave < 3 || wave == MINI_BOSS_WAVE || wave == WAVES_PER_STAGE) return WaveMod::None;
    float chance = std::min(0.78f, 0.22f + 0.05f * static_cast<float>(wave) + 0.1f * static_cast<float>(stage - 1));
    if (!rng.chance(chance)) return WaveMod::None;
    return static_cast<WaveMod>(rng.range(1, static_cast<int>(WaveMod::Count) - 1));
}

// The director's threat level stretches a wave: harder enemies, and more of them.
inline float threat_hp_mult(float threat) {
    return threat;
}

// How much a run is expected to snowball from its opening firepower by a given wave.
inline float expected_growth(int wave) {
    return std::pow(1.45f, static_cast<float>(std::max(wave, 1) - 1));
}

// Enemies answer the in-run snowball only (gates, cards, levels), measured against the power
// the run opened with. Permanent upgrades raise that opening power, so they stay a real edge
// instead of being scaled away.
inline float power_hp_mult(float player_power, float start_power, int wave) {
    float ratio = player_power / (std::max(1.0f, start_power) * expected_growth(wave));
    return ratio > 1.0f ? std::pow(ratio, 0.75f) : 1.0f;
}
inline float threat_count_mult(float threat) {
    return std::sqrt(threat);
}

} // namespace ls
