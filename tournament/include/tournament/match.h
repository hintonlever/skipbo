#pragma once

#include "tournament/bot.h"
#include <cstdint>
#include <string>

namespace skipbo {

struct MatchResult {
    std::string bot_a_id;
    std::string bot_b_id;
    uint64_t seed;
    bool a_was_first;   // true: bot_a played seat 0
    int winner_seat;    // 0 or 1: the seat that won (decided by stalemate fallback if needed)
    int winner_a_or_b;  // 0 = bot_a won, 1 = bot_b won
    int n_turns;
    bool stalemate;     // true if the game ended via stalemate fallback
    double duration_ms;
};

// Run a single deterministic game. The same `seed` always produces the same deck
// shuffle and the same per-bot RNG seeds, so (a, b, seed, a_was_first) is reproducible.
MatchResult run_match(const Bot& bot_a, const Bot& bot_b,
                      uint64_t seed, bool a_was_first);

} // namespace skipbo
