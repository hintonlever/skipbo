#include <catch2/catch_test_macros.hpp>
#include "engine/game_state.h"

using namespace skipbo;

TEST_CASE("PlayerState stock pile operations", "[game_state]") {
    PlayerState ps;
    REQUIRE(ps.stock_empty());
    REQUIRE(ps.stock_top() == CARD_NONE);

    ps.stock_pile = {3, 7, 5};
    REQUIRE(ps.stock_top() == 5); // back() is top
    REQUIRE(ps.stock_size() == 3);
}

TEST_CASE("PlayerState discard pile operations", "[game_state]") {
    PlayerState ps;
    REQUIRE(ps.discard_empty(0));
    REQUIRE(ps.discard_top(0) == CARD_NONE);

    ps.discard_piles[0] = {1, 4};
    REQUIRE(ps.discard_top(0) == 4);
    REQUIRE_FALSE(ps.discard_empty(0));
}

TEST_CASE("PlayerState remove hand card", "[game_state]") {
    PlayerState ps;
    ps.hand = {3, 7, 5, 1, 9};
    ps.hand_count = 5;

    ps.remove_hand_card(2); // remove the 5
    REQUIRE(ps.hand_count == 4);
    REQUIRE(ps.hand[0] == 3);
    REQUIRE(ps.hand[1] == 7);
    REQUIRE(ps.hand[2] == 1);
    REQUIRE(ps.hand[3] == 9);
}

TEST_CASE("GameState building pile needs", "[game_state]") {
    GameState gs;
    // Empty pile needs 1
    REQUIRE(gs.building_pile_needs(0) == CARD_MIN);

    gs.building_pile_count[0] = 3; // contains cards 1,2,3
    REQUIRE(gs.building_pile_needs(0) == 4);
}

TEST_CASE("GameState can play on building", "[game_state]") {
    GameState gs;
    // Empty pile: only 1 or SkipBo
    REQUIRE(gs.can_play_on_building(1, 0));
    REQUIRE(gs.can_play_on_building(CARD_SKIPBO, 0));
    REQUIRE_FALSE(gs.can_play_on_building(2, 0));

    gs.building_pile_count[0] = 3; // contains cards 1,2,3
    REQUIRE(gs.can_play_on_building(4, 0));
    REQUIRE(gs.can_play_on_building(CARD_SKIPBO, 0));
    REQUIRE_FALSE(gs.can_play_on_building(3, 0));
}
