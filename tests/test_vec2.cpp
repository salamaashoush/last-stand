#include "core/types.hpp"
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

using namespace ls;
using Catch::Matchers::WithinAbs;

TEST_CASE("Vec2 addition", "[vec2]") {
    Vec2 a{1, 2};
    Vec2 b{3, 4};
    auto c = a + b;
    CHECK(c.x == 4.0f);
    CHECK(c.y == 6.0f);
}

TEST_CASE("Vec2 subtraction", "[vec2]") {
    Vec2 a{5, 7};
    Vec2 b{2, 3};
    auto c = a - b;
    CHECK(c.x == 3.0f);
    CHECK(c.y == 4.0f);
}

TEST_CASE("Vec2 scalar multiply", "[vec2]") {
    Vec2 a{3, 4};
    auto c = a * 2.0f;
    CHECK(c.x == 6.0f);
    CHECK(c.y == 8.0f);
}

TEST_CASE("Vec2 length", "[vec2]") {
    Vec2 a{3, 4};
    CHECK_THAT(a.length(), WithinAbs(5.0, 0.0001));
}

TEST_CASE("Vec2 normalized", "[vec2]") {
    Vec2 a{3, 4};
    auto n = a.normalized();
    CHECK_THAT(n.length(), WithinAbs(1.0, 0.0001));
    CHECK_THAT(n.x, WithinAbs(0.6, 0.0001));
    CHECK_THAT(n.y, WithinAbs(0.8, 0.0001));
}

TEST_CASE("Vec2 zero normalized", "[vec2]") {
    Vec2 z{0, 0};
    auto n = z.normalized();
    CHECK(n.x == 0.0f);
    CHECK(n.y == 0.0f);
}

TEST_CASE("Vec2 distance_to", "[vec2]") {
    Vec2 a{0, 0};
    Vec2 b{3, 4};
    CHECK_THAT(a.distance_to(b), WithinAbs(5.0, 0.0001));
}

TEST_CASE("Vec2 to_raylib/from_raylib round-trip", "[vec2]") {
    Vec2 a{42.5f, -13.7f};
    auto rl = a.to_raylib();
    auto back = Vec2::from_raylib(rl);
    CHECK(back.x == a.x);
    CHECK(back.y == a.y);
}
