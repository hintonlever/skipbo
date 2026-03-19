#pragma once

#include "ai/player.h"
#include "engine/game.h"
#include <cstdint>

namespace skipbo {

struct MatchResult {
    int winner;        // 0 or 1
    int total_turns;
    double duration_ms;
    uint64_t seed;
};

MatchResult run_match(Player& p0, Player& p1, uint64_t seed);

} // namespace skipbo
