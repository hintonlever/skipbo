#include "tournament/match.h"
#include "engine/game.h"
#include "engine/rules.h"
#include <chrono>

namespace skipbo {

namespace {

constexpr int MAX_TURNS_SAFETY = 5000;
constexpr int MAX_CONSECUTIVE_PASSES = 4;

}  // namespace

MatchResult run_match(const Bot& bot_a, const Bot& bot_b,
                      uint64_t seed, bool a_was_first) {
    auto t0 = std::chrono::high_resolution_clock::now();

    // Seat 0 plays first. Map bot_a / bot_b to seats based on a_was_first.
    uint64_t seat0_seed = derive_player_seed(seed, 0);
    uint64_t seat1_seed = derive_player_seed(seed, 1);
    auto seat0_player = a_was_first ? bot_a.factory(seat0_seed) : bot_b.factory(seat0_seed);
    auto seat1_player = a_was_first ? bot_b.factory(seat1_seed) : bot_a.factory(seat1_seed);

    Player* seats[2] = {seat0_player.get(), seat1_player.get()};

    Game game(seed);
    game.setup();

    int turns = 0;
    int consecutive_passes = 0;

    while (!game.is_game_over()
           && turns < MAX_TURNS_SAFETY
           && consecutive_passes < MAX_CONSECUTIVE_PASSES) {
        auto moves = get_legal_moves(game.state());
        if (moves.empty()) {
            game.pass_turn();
            consecutive_passes++;
            continue;
        }
        consecutive_passes = 0;

        int cp = game.current_player();
        Move chosen = seats[cp]->choose_move(game.state(), moves);
        game.apply_move(chosen);

        if (chosen.is_discard()) {
            ++turns;
        }
    }

    bool stalemate = !game.is_game_over();
    int winner_seat = game.winner();
    if (stalemate) {
        int s0 = game.state().players[0].stock_size();
        int s1 = game.state().players[1].stock_size();
        if (s0 < s1) winner_seat = 0;
        else if (s1 < s0) winner_seat = 1;
        else winner_seat = static_cast<int>(seed & 1);  // tied stock: deterministic coin flip
    }

    int winner_a_or_b = (winner_seat == 0)
        ? (a_was_first ? 0 : 1)
        : (a_was_first ? 1 : 0);

    auto t1 = std::chrono::high_resolution_clock::now();
    double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

    return MatchResult{
        bot_a.id, bot_b.id, seed, a_was_first,
        winner_seat, winner_a_or_b, turns, stalemate, ms,
    };
}

} // namespace skipbo
