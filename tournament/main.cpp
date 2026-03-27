#include "ai/random_player.h"
#include "ai/heuristic_player.h"
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
    int turn_depth = 4;
    uint64_t seed = 12345;
    bool round_robin = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--matches" && i + 1 < argc) num_matches = std::atoi(argv[++i]);
        else if (arg == "--mcts-iters" && i + 1 < argc) mcts_iters = std::atoi(argv[++i]);
        else if (arg == "--mcts-dets" && i + 1 < argc) mcts_dets = std::atoi(argv[++i]);
        else if (arg == "--turn-depth" && i + 1 < argc) turn_depth = std::atoi(argv[++i]);
        else if (arg == "--seed" && i + 1 < argc) seed = std::stoull(argv[++i]);
        else if (arg == "--round-robin") round_robin = true;
        else if (arg == "--help") {
            std::cout << "Usage: skipbo_tournament [options]\n"
                      << "  --matches N         Matches per pairing (default: 100)\n"
                      << "  --mcts-iters N      MCTS iterations per det (default: 500)\n"
                      << "  --mcts-dets N       Number of determinizations (default: 20)\n"
                      << "  --turn-depth N      Max turns per player in tree (default: 4)\n"
                      << "  --seed N            Random seed (default: 12345)\n"
                      << "  --round-robin       Compare: Random, Heuristic, MCTS variants\n"
                      << "  --help              Show this help\n";
            return 0;
        }
    }

    TournamentConfig config;
    config.num_matches = num_matches;

    auto make_mcts = [&](uint64_t s, int depth) {
        MCTSConfig cfg;
        cfg.iterations_per_det = mcts_iters;
        cfg.num_determinizations = mcts_dets;
        cfg.max_turn_depth = depth;
        return MCTSPlayer(s, cfg);
    };

    if (round_robin) {
        RandomPlayer random_player(seed);
        HeuristicPlayer heuristic_player;
        auto mcts_d2 = make_mcts(seed + 1, 2);
        auto mcts_d4 = make_mcts(seed + 2, 4);

        struct { Player& a; Player& b; } pairings[] = {
            {heuristic_player, random_player},
            {mcts_d2, heuristic_player},
            {mcts_d2, random_player},
            {mcts_d4, mcts_d2},
            {mcts_d4, heuristic_player},
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
        auto mcts_player = make_mcts(seed + 1, turn_depth);
        HeuristicPlayer heuristic_player;

        std::cout << "Running " << num_matches << " games: " << mcts_player.name() << " vs Heuristic\n"
                  << "MCTS config: " << mcts_dets << " dets x " << mcts_iters << " iters, turn-depth " << turn_depth << "\n" << std::endl;

        auto result = run_tournament(mcts_player, heuristic_player, config, seed);
        print_results(result, mcts_player.name(), heuristic_player.name());
    }

    return 0;
}
