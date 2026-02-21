#include "managers/wave_manager.hpp"
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

using namespace ls;

TEST_CASE("WaveManager has 30 waves", "[wave]") {
    WaveManager wm;
    CHECK(wm.total_waves() == 30);
}

TEST_CASE("Boss waves at multiples of 5", "[wave]") {
    WaveManager wm;
    // get_wave is 0-indexed: get_wave(0) => wave 1, get_wave(4) => wave 5, etc.
    for (WaveNum i = 0; i < 30; ++i) {
        auto& wave = wm.get_wave(i);
        WaveNum wave_num = i + 1;
        if (wave_num % 5 == 0) {
            CHECK(wave.is_boss_wave);
        } else {
            CHECK_FALSE(wave.is_boss_wave);
        }
    }
}

TEST_CASE("Scaling is monotonically increasing", "[wave]") {
    WaveManager wm;
    float prev = 0.0f;
    for (WaveNum w = 1; w <= 30; ++w) {
        float s = wm.scaling(w);
        CHECK(s > prev);
        prev = s;
    }
}

TEST_CASE("Scaling at wave 1 is 1.0", "[wave]") {
    WaveManager wm;
    CHECK_THAT(wm.scaling(1), Catch::Matchers::WithinAbs(1.0, 0.001));
}

TEST_CASE("Wave numbers are sequential", "[wave]") {
    WaveManager wm;
    for (WaveNum i = 0; i < 30; ++i) {
        CHECK(wm.get_wave(i).number == i + 1);
    }
}
