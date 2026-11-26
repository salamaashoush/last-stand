#include "game/cards.hpp"
#include <catch2/catch_test_macros.hpp>
#include <set>

using namespace ls;

TEST_CASE("card definitions are indexed by their id", "[cards]") {
    for (int i = 0; i < CARD_COUNT; ++i) CHECK(static_cast<int>(CARD_DEFS[static_cast<size_t>(i)].id) == i);
}

TEST_CASE("a roll offers three different available cards", "[cards]") {
    Rng rng{7};
    RunStats s;
    for (int i = 0; i < 300; ++i) {
        auto offer = roll_cards(s, rng);
        std::set<CardId> unique(offer.begin(), offer.end());
        CHECK(unique.size() == 3);
        for (auto id : offer) CHECK(card_available(s, id));
    }
}

TEST_CASE("evolutions unlock only when both requirements are met", "[cards]") {
    RunStats s;
    CHECK_FALSE(card_available(s, CardId::BulletStorm));
    for (int i = 0; i < 3; ++i) apply_card(s, CardId::Multishot);
    CHECK_FALSE(card_available(s, CardId::BulletStorm));
    for (int i = 0; i < 3; ++i) apply_card(s, CardId::RapidFire);
    CHECK(card_available(s, CardId::BulletStorm));
    apply_card(s, CardId::BulletStorm);
    CHECK_FALSE(card_available(s, CardId::BulletStorm));
    CHECK(s.multishot == 6);
}

TEST_CASE("maxed cards drop out of the pool", "[cards]") {
    RunStats s;
    for (int i = 0; i < card_def(CardId::Piercing).max_level; ++i) apply_card(s, CardId::Piercing);
    CHECK_FALSE(card_available(s, CardId::Piercing));
    CHECK(s.pierce == card_def(CardId::Piercing).max_level);
}

TEST_CASE("burn scales with damage taken after the card", "[cards]") {
    RunStats s;
    apply_card(s, CardId::Incendiary);
    float before = s.burn_dps();
    apply_card(s, CardId::HeavyRounds);
    CHECK(s.burn_dps() > before);
}

TEST_CASE("fortify raises max wall and asks for a full repair", "[cards]") {
    RunStats s;
    auto fx = apply_card(s, CardId::Fortify);
    CHECK(s.wall_max > 1000.0f);
    CHECK(fx.repair_fraction == 1.0f);
}

TEST_CASE("new evolutions unlock from their pairs and grant their effects", "[cards]") {
    RunStats s;
    CHECK(evolution_partner(CardId::Fortify) == CardId::Overclock);
    CHECK_FALSE(card_available(s, CardId::FortressOverdrive));
    apply_card(s, CardId::Fortify);
    apply_card(s, CardId::Fortify);
    apply_card(s, CardId::Overclock);
    apply_card(s, CardId::Overclock);
    CHECK(card_available(s, CardId::FortressOverdrive));
    auto fx = apply_card(s, CardId::FortressOverdrive);
    CHECK(fx.grant_shield);
    CHECK(s.aegis_wall);
}
