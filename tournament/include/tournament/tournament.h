#pragma once

#include "ai/player.h"
#include "tournament/elo.h"
#include <vector>
#include <memory>
#include <string>
#include <array>

namespace skipbo {

struct TournamentConfig {
    int num_matches = 1000;
    bool alternate_first_player = true;
};

struct TournamentResult {
    std::array<EloRating, 2> ratings;
    std::array<int, 2> wins = {0, 0};
    int total_games = 0;
    double total_duration_ms = 0.0;
};

TournamentResult run_tournament(Player& p0, Player& p1,
                                 TournamentConfig config, uint64_t seed);

void print_results(const TournamentResult& result,
                   const std::string& name0, const std::string& name1);

} // namespace skipbo
