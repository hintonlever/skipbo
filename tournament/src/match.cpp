#include "tournament/match.h"
#include "engine/rules.h"
#include <chrono>

namespace skipbo {

MatchResult run_match(Player& p0, Player& p1, uint64_t seed) {
    auto start = std::chrono::high_resolution_clock::now();

    Game game(seed);
    game.setup();

    Player* players[2] = {&p0, &p1};
    int turns = 0;
    int max_turns = 5000; // safety limit
    int consecutive_passes = 0;

    while (!game.is_game_over() && turns < max_turns && consecutive_passes < 4) {
        auto moves = get_legal_moves(game.state());
        if (moves.empty()) {
            game.pass_turn();
            consecutive_passes++;
            continue;
        }
        consecutive_passes = 0;

        int cp = game.current_player();
        Move chosen = players[cp]->choose_move(game.state(), moves);
        game.apply_move(chosen);

        if (chosen.is_discard()) {
            ++turns;
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    double ms = std::chrono::duration<double, std::milli>(end - start).count();

    int winner = game.winner();
    if (!game.is_game_over()) {
        // Stalemate — winner is whoever has fewer stock pile cards
        int s0 = game.state().players[0].stock_size();
        int s1 = game.state().players[1].stock_size();
        if (s0 < s1) winner = 0;
        else if (s1 < s0) winner = 1;
        else winner = static_cast<int>(seed % 2);  // true tie: coin flip
    }

    return {winner, turns, ms, seed};
}

} // namespace skipbo
