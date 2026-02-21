#include "core/types.hpp"
#include <catch2/catch_test_macros.hpp>

using namespace ls;

TEST_CASE("GridPos equality", "[gridpos]") {
    GridPos a{3, 5};
    GridPos b{3, 5};
    GridPos c{4, 5};
    CHECK(a == b);
    CHECK_FALSE(a == c);
}

TEST_CASE("GridPos ordering", "[gridpos]") {
    GridPos a{1, 2};
    GridPos b{1, 3};
    GridPos c{2, 0};
    CHECK(a < b);
    CHECK(a < c);
    CHECK(b < c);
}

TEST_CASE("GridPos default construction", "[gridpos]") {
    GridPos p{};
    CHECK(p.x == 0);
    CHECK(p.y == 0);
}
