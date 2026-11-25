#include "game/balance.hpp"
#include "game/formation.hpp"
#include "game/turrets.hpp"
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <cmath>

using namespace ls;

TEST_CASE("waves are sorted, in bounds and match their counts", "[balance]") {
    Rng rng{3};
    for (int stage = 1; stage <= 5; ++stage) {
        for (int wave = 1; wave <= WAVES_PER_STAGE; ++wave) {
            auto spawns = build_wave(stage, wave, rng);
            CHECK(static_cast<int>(spawns.size()) == wave_counts(stage, wave).total());
            for (size_t i = 1; i < spawns.size(); ++i) CHECK(spawns[i - 1].time <= spawns[i].time);
            for (const auto& s : spawns) {
                CHECK(s.x >= 0.0f);
                CHECK(s.x <= static_cast<float>(VIRTUAL_W));
                CHECK(s.time <= wave_duration(wave));
            }
        }
    }
}

TEST_CASE("bosses arrive on wave five and ten only", "[balance]") {
    for (int wave = 1; wave <= WAVES_PER_STAGE; ++wave) {
        auto c = wave_counts(1, wave);
        CHECK(c.bosses == ((wave == MINI_BOSS_WAVE || wave == WAVES_PER_STAGE) ? 1 : 0));
        CHECK(c.mini_boss == (wave == MINI_BOSS_WAVE));
    }
}

TEST_CASE("difficulty only ever rises through stages and waves", "[balance]") {
    for (int s = 1; s < 15; ++s) CHECK(stage_hp_mult(s + 1) > stage_hp_mult(s));
    for (int w = 1; w < 10; ++w) CHECK(wave_hp_mult(w + 1) > wave_hp_mult(w));
}

TEST_CASE("enemies only answer in-run growth beyond the expected snowball", "[balance]") {
    float start = 250.0f;
    CHECK(power_hp_mult(start * expected_growth(4) * 0.5f, start, 4) == 1.0f);
    CHECK_THAT(power_hp_mult(start * expected_growth(4) * 4.0f, start, 4),
               Catch::Matchers::WithinRel(std::pow(4.0f, 0.65f), 1e-4f));
}

TEST_CASE("formation stays inside the screen at any size", "[formation]") {
    for (int n : {1, 5, 12, 40, 300}) {
        int visible = visible_soldiers(n);
        for (float x : {0.0f, 360.0f, 720.0f}) {
            float cx = clamp_squad_x(x, visible);
            for (int i = 0; i < visible; ++i) {
                float sx = cx + soldier_offset(i, visible).x;
                CHECK(sx > 0.0f);
                CHECK(sx < static_cast<float>(VIRTUAL_W));
            }
        }
    }
    CHECK(overflow_mult(20) == 1.0f);
    CHECK(overflow_mult(400) > 1.0f);
}

TEST_CASE("turret upgrades cost more and selling refunds half", "[turrets]") {
    for (int l = 0; l < TURRET_MAX_LEVEL - 1; ++l) {
        CHECK(turret_upgrade_cost(TurretType::Cannon, l + 1) > turret_upgrade_cost(TurretType::Cannon, l));
    }
    int spent = turret_upgrade_cost(TurretType::Tesla, 0) + turret_upgrade_cost(TurretType::Tesla, 1);
    CHECK(turret_refund(TurretType::Tesla, 2) == spent / 2);
}
