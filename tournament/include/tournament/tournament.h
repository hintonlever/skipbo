#pragma once

#include "tournament/bot.h"
#include "tournament/elo.h"
#include <array>
#include <cstdint>
#include <string>

namespace skipbo {

struct TournamentConfig {
    int n_pairings = 100;            // each pairing runs 2 games (a-first + b-first)
    int n_threads = 0;               // 0 = std::thread::hardware_concurrency()
    uint64_t base_seed = 12345;
    std::string results_csv = "results.csv";
    bool append_csv = false;         // false: overwrite + write header; true: append (no header if file exists)
    int progress_every = 50;         // games between progress prints; 0 = silent
};

struct TournamentSummary {
    std::string bot_a_id;
    std::string bot_b_id;
    int total_games = 0;
    std::array<int, 2> wins = {0, 0};   // [0]=a wins, [1]=b wins
    int stalemates = 0;
    double total_duration_ms = 0.0;
    double wall_seconds = 0.0;
    EloRating elo_a;
    EloRating elo_b;
};

TournamentSummary run_tournament(const Bot& bot_a, const Bot& bot_b,
                                 const TournamentConfig& cfg);

void print_summary(const TournamentSummary& s);

} // namespace skipbo
