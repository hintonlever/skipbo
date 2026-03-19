#include <catch2/catch_test_macros.hpp>
#include "engine/card.h"

using namespace skipbo;

TEST_CASE("Card constants", "[card]") {
    REQUIRE(TOTAL_CARDS == 162);
    REQUIRE(CARDS_PER_VALUE * CARD_MAX + NUM_SKIPBO == 162);
}

TEST_CASE("Card type checks", "[card]") {
    REQUIRE(is_skipbo(CARD_SKIPBO));
    REQUIRE_FALSE(is_skipbo(1));
    REQUIRE(is_numbered(1));
    REQUIRE(is_numbered(12));
    REQUIRE_FALSE(is_numbered(0));
    REQUIRE_FALSE(is_numbered(13));
    REQUIRE(is_valid_card(CARD_SKIPBO));
    REQUIRE(is_valid_card(6));
    REQUIRE_FALSE(is_valid_card(CARD_NONE));
}

TEST_CASE("Card to string", "[card]") {
    REQUIRE(card_to_string(CARD_SKIPBO) == "SB");
    REQUIRE(card_to_string(1) == "1");
    REQUIRE(card_to_string(12) == "12");
    REQUIRE(card_to_string(CARD_NONE) == "[]");
}
