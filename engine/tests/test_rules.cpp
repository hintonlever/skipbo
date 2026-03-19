#include <catch2/catch_test_macros.hpp>
#include "engine/rules.h"

using namespace skipbo;

static GameState make_simple_state() {
    GameState gs;
    // Player 0 has hand [1, 5, SB], stock pile with top 3, empty discards
    gs.players[0].hand = {1, 5, CARD_SKIPBO, CARD_NONE, CARD_NONE};
    gs.players[0].hand_count = 3;
    gs.players[0].stock_pile = {7, 3}; // top is 3
    // Player 1 irrelevant for move generation
    gs.players[1].hand_count = 0;
    gs.players[1].stock_pile = {2, 8};
    gs.current_player = 0;
    return gs;
}

TEST_CASE("Hand cards can play on building piles", "[rules]") {
    auto gs = make_simple_state();
    // Building pile 0 empty -> needs 1
    auto moves = get_legal_moves(gs);

    // Hand card 1 should be playable on all 4 building piles (all empty, all need 1)
    int build_moves_for_card1 = 0;
    for (const auto& m : moves) {
        if (m.source == MoveSource::Hand0 && !m.is_discard()) {
            build_moves_for_card1++;
        }
    }
    REQUIRE(build_moves_for_card1 == 4); // card 1 can go on any empty building pile
}

TEST_CASE("SkipBo wild can play on any building pile", "[rules]") {
    auto gs = make_simple_state();
    auto moves = get_legal_moves(gs);

    int skipbo_build_moves = 0;
    for (const auto& m : moves) {
        if (m.source == MoveSource::Hand2 && !m.is_discard()) {
            skipbo_build_moves++;
        }
    }
    REQUIRE(skipbo_build_moves == 4); // SB can go on any building pile
}

TEST_CASE("Hand cards can discard to any discard pile", "[rules]") {
    auto gs = make_simple_state();
    auto moves = get_legal_moves(gs);

    int discard_moves = 0;
    for (const auto& m : moves) {
        if (m.is_discard()) discard_moves++;
    }
    // 3 hand cards x 4 discard piles = 12 discard moves
    REQUIRE(discard_moves == 12);
}

TEST_CASE("Stock pile top can play on building piles", "[rules]") {
    auto gs = make_simple_state();
    // Stock top is 3, but building piles are empty (need 1), so no stock moves
    auto moves = get_legal_moves(gs);
    int stock_moves = 0;
    for (const auto& m : moves) {
        if (m.is_from_stock()) stock_moves++;
    }
    REQUIRE(stock_moves == 0);

    // Put 1,2 on building pile 0 so it needs 3
    gs.building_piles[0] = {1, 2};
    moves = get_legal_moves(gs);
    stock_moves = 0;
    for (const auto& m : moves) {
        if (m.is_from_stock()) stock_moves++;
    }
    REQUIRE(stock_moves == 1); // stock top 3 -> building pile 0
}

TEST_CASE("Discard pile top can play on building piles", "[rules]") {
    auto gs = make_simple_state();
    gs.players[0].discard_piles[0] = {1};
    auto moves = get_legal_moves(gs);

    int discard_to_build = 0;
    for (const auto& m : moves) {
        if (m.is_from_discard() && !m.is_discard()) {
            discard_to_build++;
        }
    }
    // Discard pile 0 top is 1, all building piles empty (need 1) -> 4 moves
    REQUIRE(discard_to_build == 4);
}

TEST_CASE("No moves when game is over", "[rules]") {
    GameState gs;
    gs.game_over = true;
    auto moves = get_legal_moves(gs);
    REQUIRE(moves.empty());
}
