#include "game/gates.hpp"
#include <catch2/catch_test_macros.hpp>

using namespace ls;

TEST_CASE("add gates add their value and never drop the squad below one", "[gates]") {
    CHECK(apply_gate(10, {GateKind::Add, 5}) == 15);
    CHECK(apply_gate(10, {GateKind::Add, -4}) == 6);
    CHECK(apply_gate(3, {GateKind::Add, -40}) == 1);
}

TEST_CASE("mul gates multiply, negative factors divide rounding up", "[gates]") {
    CHECK(apply_gate(7, {GateKind::Mul, 2}) == 14);
    CHECK(apply_gate(7, {GateKind::Mul, -2}) == 4);
    CHECK(apply_gate(7, {GateKind::Mul, 1}) == 7);
    CHECK(apply_gate(800, {GateKind::Mul, 3}) == MAX_SOLDIERS);
}

TEST_CASE("blue gates climb when shot but never past the cap", "[gates]") {
    GateState g{GateKind::Add, -6, false};
    int soldiers = 60;
    gate_hit(g, 1, 1.0f, soldiers);
    CHECK(g.value == -6 + add_step(soldiers));
    for (int i = 0; i < 1000; ++i) gate_hit(g, 1, 1.0f, soldiers);
    CHECK(g.value == add_cap(soldiers));
}

TEST_CASE("red gates only get worse when shot", "[gates]") {
    GateState g{GateKind::Add, -4, true};
    for (int i = 0; i < 10; ++i) gate_hit(g, 1, 1.0f, 30);
    CHECK(g.value == -14);
    GateState div{GateKind::Mul, -2, true};
    for (int i = 0; i < MUL_STEP_POINTS * 10; ++i) gate_hit(div, 1, 1.0f, 30);
    CHECK(div.value == -4);
}

TEST_CASE("blue multipliers step up to x3 with sustained fire", "[gates]") {
    GateState g{GateKind::Mul, 1, false};
    for (int i = 0; i < MUL_STEP_POINTS; ++i) gate_hit(g, 1, 1.0f, 10);
    CHECK(g.value == 2);
    for (int i = 0; i < MUL_STEP_POINTS * 5; ++i) gate_hit(g, 1, 1.0f, 10);
    CHECK(g.value == 3);
}

TEST_CASE("every row has a blue gate that starts negative and a red trap", "[gates]") {
    Rng rng{42};
    for (int i = 0; i < 500; ++i) {
        int soldiers = rng.range(1, 400);
        int lanes = rng.range(2, 3);
        auto row = roll_gate_row(rng, rng.range(1, 10), soldiers, lanes);
        REQUIRE(static_cast<int>(row.size()) == lanes);
        bool any_blue = false;
        bool any_red = false;
        for (const auto& g : row) {
            any_blue |= !g.red;
            any_red |= g.red;
            if (g.kind == GateKind::Add) CHECK(g.value < 0);
            if (g.red) CHECK_FALSE(gate_is_good(g));
        }
        CHECK(any_blue);
        CHECK(any_red);
    }
}

TEST_CASE("hits needed per point scale with fire output", "[gates]") {
    CHECK(hits_per_point(1.0f) == 2);
    CHECK(hits_per_point(24.0f) == 20);
    CHECK(hits_per_point(1e6f) == 400);
}

TEST_CASE("stat gates leave the squad size alone and move in percent", "[gates]") {
    for (GateKind k : {GateKind::Rate, GateKind::Power, GateKind::Crit, GateKind::Turret}) {
        GateState g{k, -6, false};
        CHECK(apply_gate(40, g) == 40);
        for (int i = 0; i < 100; ++i) gate_hit(g, 1, 1.0f, 40);
        CHECK(g.value == STAT_MAX);
        GateState red{k, -6, true};
        for (int i = 0; i < 100; ++i) gate_hit(red, 1, 1.0f, 40);
        CHECK(red.value == STAT_MIN);
    }
}

TEST_CASE("gate_after_hits matches firing the bullets one at a time", "[gates]") {
    Rng rng{11};
    for (int i = 0; i < 2000; ++i) {
        GateState g;
        g.kind = static_cast<GateKind>(rng.range(0, 5));
        g.red = rng.chance(0.5f);
        g.value = g.kind == GateKind::Mul ? (g.red ? -2 : rng.range(1, 2)) : -rng.range(0, 20);
        g.hit_accum = rng.range(0, 3);
        int hpp = rng.range(4, 30);
        int soldiers = rng.range(1, 300);
        float growth = rng.chance(0.5f) ? 1.0f : 2.0f;
        int hits = rng.range(0, 500);
        GateState slow = g;
        for (int k = 0; k < hits; ++k) gate_hit(slow, hpp, growth, soldiers);
        GateState fast = gate_after_hits(g, hits, hpp, growth, soldiers);
        REQUIRE(fast.value == slow.value);
        REQUIRE(fast.hit_accum == slow.hit_accum);
        REQUIRE(fast.points == slow.points);
    }
}
