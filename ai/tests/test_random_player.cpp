#include <catch2/catch_test_macros.hpp>
#include "ai/random_player.h"

using namespace skipbo;

TEST_CASE("RandomPlayer always returns a legal move", "[random_player]") {
    RandomPlayer rp(42);

    GameState gs;
    gs.players[0].hand = {1, 5, CARD_SKIPBO, CARD_NONE, CARD_NONE};
    gs.players[0].hand_count = 3;
    gs.players[0].stock_pile = {3};
    gs.current_player = 0;

    std::vector<Move> moves = {{MoveSource::Hand0, MoveTarget::BuildingPile0},
                                {MoveSource::Hand1, MoveTarget::DiscardPile0}};

    for (int i = 0; i < 100; ++i) {
        Move chosen = rp.choose_move(gs, moves);
        bool found = false;
        for (const auto& m : moves) {
            if (m == chosen) { found = true; break; }
        }
        REQUIRE(found);
    }
}

TEST_CASE("RandomPlayer is reproducible with same seed", "[random_player]") {
    RandomPlayer rp1(42);
    RandomPlayer rp2(42);

    GameState gs;
    gs.players[0].hand = {1, 5, 3, 7, CARD_SKIPBO};
    gs.players[0].hand_count = 5;
    gs.players[0].stock_pile = {3};
    gs.current_player = 0;

    std::vector<Move> moves = {{MoveSource::Hand0, MoveTarget::BuildingPile0},
                                {MoveSource::Hand1, MoveTarget::DiscardPile0},
                                {MoveSource::Hand2, MoveTarget::BuildingPile1}};

    for (int i = 0; i < 50; ++i) {
        Move m1 = rp1.choose_move(gs, moves);
        Move m2 = rp2.choose_move(gs, moves);
        REQUIRE(m1 == m2);
    }
}
