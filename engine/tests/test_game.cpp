#include <catch2/catch_test_macros.hpp>
#include "engine/game.h"
#include "engine/rules.h"
#include "ai/random_player.h"

using namespace skipbo;

TEST_CASE("Game setup creates valid initial state", "[game]") {
    Game game(42);
    game.setup();
    const auto& state = game.state();

    // Each player should have STOCK_PILE_SIZE stock cards
    REQUIRE(state.players[0].stock_size() == STOCK_PILE_SIZE);
    REQUIRE(state.players[1].stock_size() == STOCK_PILE_SIZE);

    // Current player should have 5 hand cards
    REQUIRE(state.players[0].hand_count == 5);

    // Player 1 hasn't drawn yet (their turn hasn't started)
    // Actually, only current player draws on setup
    REQUIRE(state.current_player == 0);

    // Building piles empty
    for (int i = 0; i < NUM_BUILDING_PILES; ++i) {
        REQUIRE(state.building_pile_count[i] == 0);
    }

    // Total cards should be 162
    int total = state.draw_pile.size();
    for (int p = 0; p < NUM_PLAYERS; ++p) {
        total += state.players[p].stock_size();
        total += state.players[p].hand_count;
        for (int d = 0; d < NUM_DISCARD_PILES; ++d) {
            total += state.players[p].discard_piles[d].size();
        }
    }
    for (int b = 0; b < NUM_BUILDING_PILES; ++b) {
        total += state.building_pile_count[b];
    }
    REQUIRE(total == TOTAL_CARDS);
}

TEST_CASE("Two random players can play a game to completion or stalemate", "[game]") {
    RandomPlayer p0(100);
    RandomPlayer p1(200);

    Game game(42);
    game.setup();

    Player* players[2] = {&p0, &p1};
    int moves_made = 0;
    int max_moves = 50000;
    int consecutive_passes = 0;

    while (!game.is_game_over() && moves_made < max_moves && consecutive_passes < 4) {
        auto legal = get_legal_moves(game.state());
        if (legal.empty()) {
            game.pass_turn();
            consecutive_passes++;
            continue;
        }
        consecutive_passes = 0;

        int cp = game.current_player();
        Move chosen = players[cp]->choose_move(game.state(), legal);
        bool ok = game.apply_move(chosen);
        REQUIRE(ok);
        moves_made++;
    }

    // Game terminates (either win or stalemate)
    REQUIRE((game.is_game_over() || consecutive_passes >= 4 || moves_made >= max_moves));
    // At least some moves were made
    REQUIRE(moves_made > 10);
}

TEST_CASE("Multiple games terminate with different seeds", "[game]") {
    for (uint64_t seed = 0; seed < 10; ++seed) {
        RandomPlayer p0(seed * 2);
        RandomPlayer p1(seed * 2 + 1);

        Game game(seed);
        game.setup();

        Player* players[2] = {&p0, &p1};
        int moves_made = 0;
        int consecutive_passes = 0;

        while (!game.is_game_over() && moves_made < 50000 && consecutive_passes < 4) {
            auto legal = get_legal_moves(game.state());
            if (legal.empty()) {
                game.pass_turn();
                consecutive_passes++;
                continue;
            }
            consecutive_passes = 0;
            int cp = game.current_player();
            Move chosen = players[cp]->choose_move(game.state(), legal);
            game.apply_move(chosen);
            moves_made++;
        }

        // Every game should terminate (either win or stalemate)
        REQUIRE((game.is_game_over() || consecutive_passes >= 4));
        REQUIRE(moves_made > 10);
    }
}
