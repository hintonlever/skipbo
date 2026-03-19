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
    bool use_mcts = true;

    // Simple arg parsing
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--matches" && i + 1 < argc) num_matches = std::atoi(argv[++i]);
        else if (arg == "--mcts-iters" && i + 1 < argc) mcts_iters = std::atoi(argv[++i]);
        else if (arg == "--mcts-dets" && i + 1 < argc) mcts_dets = std::atoi(argv[++i]);
        else if (arg == "--seed" && i + 1 < argc) seed = std::stoull(argv[++i]);
        else if (arg == "--random-only") use_mcts = false;
        else if (arg == "--help") {
            std::cout << "Usage: skipbo_tournament [options]\n"
                      << "  --matches N       Number of matches (default: 100)\n"
                      << "  --mcts-iters N    MCTS iterations per determinization (default: 500)\n"
                      << "  --mcts-dets N     Number of determinizations (default: 20)\n"
                      << "  --seed N          Random seed (default: 12345)\n"
                      << "  --random-only     Run Random vs Random\n"
                      << "  --help            Show this help\n";
            return 0;
        }
    }

    RandomPlayer random_player(seed);

    TournamentConfig config;
    config.num_matches = num_matches;

    if (use_mcts) {
        MCTSConfig mcts_config;
        mcts_config.iterations_per_det = mcts_iters;
        mcts_config.num_determinizations = mcts_dets;
        MCTSPlayer mcts_player(seed + 1, mcts_config);

        std::cout << "Running " << num_matches << " games: MCTS vs Random" << std::endl;
        std::cout << "MCTS config: " << mcts_dets << " determinizations x "
                  << mcts_iters << " iterations" << std::endl;

        auto result = run_tournament(mcts_player, random_player, config, seed);
        print_results(result, mcts_player.name(), random_player.name());
    } else {
        RandomPlayer random_player2(seed + 1);
        std::cout << "Running " << num_matches << " games: Random vs Random" << std::endl;

        auto result = run_tournament(random_player, random_player2, config, seed);
        print_results(result, "Random1", "Random2");
    }

    return 0;
}
