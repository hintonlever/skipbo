#include "ai/random_player.h"
#include "ai/mcts_player.h"
#include "tournament/tournament.h"
#include <iostream>
#include <cstdlib>
#include <string>

using namespace skipbo;

int main(int argc, char** argv) {
    int num_matches = 100;
    int mcts_iters = 500;
    int mcts_dets = 20;
    uint64_t seed = 12345;
    bool round_robin = false;
    double heuristic_rate = 0.5;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--matches" && i + 1 < argc) num_matches = std::atoi(argv[++i]);
        else if (arg == "--mcts-iters" && i + 1 < argc) mcts_iters = std::atoi(argv[++i]);
        else if (arg == "--mcts-dets" && i + 1 < argc) mcts_dets = std::atoi(argv[++i]);
        else if (arg == "--seed" && i + 1 < argc) seed = std::stoull(argv[++i]);
        else if (arg == "--heuristic" && i + 1 < argc) heuristic_rate = std::atof(argv[++i]);
        else if (arg == "--round-robin") round_robin = true;
        else if (arg == "--help") {
            std::cout << "Usage: skipbo_tournament [options]\n"
                      << "  --matches N       Matches per pairing (default: 100)\n"
                      << "  --mcts-iters N    MCTS iterations per det (default: 500)\n"
                      << "  --mcts-dets N     Number of determinizations (default: 20)\n"
                      << "  --heuristic F     Rollout heuristic rate 0.0-1.0 (default: 0.5)\n"
                      << "  --seed N          Random seed (default: 12345)\n"
                      << "  --round-robin     Compare rates: <rate> vs 0.0 vs Random\n"
                      << "  --help            Show this help\n";
            return 0;
        }
    }

    TournamentConfig config;
    config.num_matches = num_matches;

    auto make_mcts = [&](uint64_t s, double rate) {
        MCTSConfig cfg;
        cfg.iterations_per_det = mcts_iters;
        cfg.num_determinizations = mcts_dets;
        cfg.rollout_heuristic_rate = rate;
        return MCTSPlayer(s, cfg);
    };

    if (round_robin) {
        auto mcts_h = make_mcts(seed + 1, heuristic_rate);
        auto mcts_r = make_mcts(seed + 2, 0.0);
        RandomPlayer random_player(seed + 3);

        struct { Player& a; Player& b; } pairings[] = {
            {mcts_h, mcts_r},
            {mcts_h, random_player},
            {mcts_r, random_player},
        };

        std::cout << "Round-robin: " << num_matches << " matches per pairing\n"
                  << "MCTS config: " << mcts_dets << " dets x " << mcts_iters << " iters\n"
                  << std::endl;

        for (auto& [a, b] : pairings) {
            std::cout << "--- " << a.name() << " vs " << b.name() << " ---" << std::endl;
            auto result = run_tournament(a, b, config, seed);
            print_results(result, a.name(), b.name());
        }
    } else {
        auto mcts_player = make_mcts(seed + 1, heuristic_rate);
        RandomPlayer random_player(seed);

        std::cout << "Running " << num_matches << " games: " << mcts_player.name() << " vs Random\n"
                  << "MCTS config: " << mcts_dets << " dets x " << mcts_iters << " iters\n" << std::endl;

        auto result = run_tournament(mcts_player, random_player, config, seed);
        print_results(result, mcts_player.name(), random_player.name());
    }

    return 0;
}
