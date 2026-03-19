#include "tournament/tournament.h"
#include "tournament/match.h"
#include <iostream>
#include <iomanip>

namespace skipbo {

TournamentResult run_tournament(Player& p0, Player& p1,
                                 TournamentConfig config, uint64_t seed) {
    TournamentResult result;

    for (int i = 0; i < config.num_matches; ++i) {
        uint64_t match_seed = seed + i;
        MatchResult mr;

        if (config.alternate_first_player && (i % 2 == 1)) {
            mr = run_match(p1, p0, match_seed);
            if (mr.winner == 0) mr.winner = 1;
            else if (mr.winner == 1) mr.winner = 0;
        } else {
            mr = run_match(p0, p1, match_seed);
        }

        result.total_duration_ms += mr.duration_ms;
        result.total_games++;

        if (mr.winner == 0) result.wins[0]++;
        else if (mr.winner == 1) result.wins[1]++;

        elo_update(result.ratings[0], result.ratings[1], mr.winner);

        if ((i + 1) % 100 == 0) {
            std::cerr << "\rProgress: " << (i + 1) << "/" << config.num_matches
                      << " games" << std::flush;
        }
    }
    std::cerr << std::endl;

    return result;
}

void print_results(const TournamentResult& result,
                   const std::string& name0, const std::string& name1) {
    std::cout << "\n=== Tournament Results ===" << std::endl;
    std::cout << "Games played: " << result.total_games << std::endl;
    std::cout << std::fixed << std::setprecision(1);
    std::cout << "Total time: " << result.total_duration_ms << " ms" << std::endl;
    std::cout << "Avg time/game: " << result.total_duration_ms / result.total_games << " ms\n" << std::endl;

    auto print_player = [&](const std::string& name, int idx) {
        int w = result.wins[idx];
        double pct = 100.0 * w / result.total_games;
        double elo = result.ratings[idx].rating;
        std::cout << std::setw(12) << name
                  << "  Wins: " << std::setw(5) << w
                  << " (" << std::setw(5) << std::setprecision(1) << pct << "%)"
                  << "  ELO: " << std::setw(7) << std::setprecision(1) << elo
                  << std::endl;
    };

    print_player(name0, 0);
    print_player(name1, 1);
    std::cout << std::endl;
}

} // namespace skipbo
