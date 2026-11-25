#pragma once
#include <cstdint>

namespace ls {

// Deterministic so wave generation and card rolls are reproducible in tests.
struct Rng {
    uint64_t state{0x9E3779B97F4A7C15ull};

    explicit Rng(uint64_t seed = 0x9E3779B97F4A7C15ull) : state(seed ? seed : 0x9E3779B97F4A7C15ull) {}

    uint32_t next_u32() {
        state ^= state >> 12;
        state ^= state << 25;
        state ^= state >> 27;
        return static_cast<uint32_t>((state * 0x2545F4914F6CDD1Dull) >> 32);
    }

    float unit() { return static_cast<float>(next_u32() >> 8) * (1.0f / 16777216.0f); }
    float uniform(float lo, float hi) { return lo + (hi - lo) * unit(); }
    int range(int lo, int hi_inclusive) {
        if (hi_inclusive <= lo) return lo;
        auto span = static_cast<uint32_t>(hi_inclusive - lo + 1);
        return lo + static_cast<int>(next_u32() % span);
    }
    bool chance(float p) { return unit() < p; }
};

} // namespace ls
