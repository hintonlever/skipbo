#include <catch2/catch_test_macros.hpp>
#include "ai/determinizer.h"
#include "engine/game.h"
#include <map>

using namespace skipbo;

TEST_CASE("Determinizer preserves visible information", "[determinizer]") {
    Game game(42);
    game.setup();
    const auto& state = game.state();

    std::mt19937 rng(123);
    auto sampled = Determinizer::sample(state, 0, rng);

    // Player 0's hand should be preserved
    REQUIRE(sampled.players[0].hand_count == state.players[0].hand_count);
    for (int i = 0; i < state.players[0].hand_count; ++i) {
        REQUIRE(sampled.players[0].hand[i] == state.players[0].hand[i]);
    }

    // Player 0's stock pile should be preserved
    REQUIRE(sampled.players[0].stock_pile == state.players[0].stock_pile);

    // Opponent's stock pile top should be preserved
    REQUIRE(sampled.players[1].stock_top() == state.players[1].stock_top());
    REQUIRE(sampled.players[1].stock_size() == state.players[1].stock_size());
}

TEST_CASE("Determinizer preserves total card count", "[determinizer]") {
    Game game(42);
    game.setup();
    const auto& state = game.state();

    std::mt19937 rng(456);
    auto sampled = Determinizer::sample(state, 0, rng);

    auto count_cards = [](const GameState& gs) {
        int total = 0;
        for (int p = 0; p < NUM_PLAYERS; ++p) {
            total += gs.players[p].stock_size();
            total += gs.players[p].hand_count;
            for (int d = 0; d < NUM_DISCARD_PILES; ++d) {
                total += gs.players[p].discard_piles[d].size();
            }
        }
        for (int b = 0; b < NUM_BUILDING_PILES; ++b) {
            total += gs.building_piles[b].size();
        }
        total += gs.draw_pile.size();
        return total;
    };

    REQUIRE(count_cards(sampled) == count_cards(state));
    REQUIRE(count_cards(sampled) == TOTAL_CARDS);
}

TEST_CASE("Different seeds produce different determinizations", "[determinizer]") {
    Game game(42);
    game.setup();
    const auto& state = game.state();

    std::mt19937 rng1(100);
    std::mt19937 rng2(200);
    auto s1 = Determinizer::sample(state, 0, rng1);
    auto s2 = Determinizer::sample(state, 0, rng2);

    // Draw piles should differ (very high probability with 72+ hidden cards)
    bool differ = false;
    for (size_t i = 0; i < s1.draw_pile.size() && i < s2.draw_pile.size(); ++i) {
        if (s1.draw_pile[i] != s2.draw_pile[i]) {
            differ = true;
            break;
        }
    }
    REQUIRE(differ);
}
