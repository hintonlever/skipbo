#pragma once

#include "ai/player.h"
#include "ai/turn_action.h"
#include "ai/neural_net.h"
#include "ai/mcts_player.h" // for ChainAnalysis
#include <random>
#include <vector>
#include <string>

namespace skipbo {

struct NNMCTSConfig {
    int num_determinizations = 20;
    int iterations_per_det = 500;
    double c_puct = 2.5;       // PUCT exploration constant
    int max_turn_depth = 4;    // max turns per player in tree
};

class NNMCTSPlayer : public Player {
public:
    NNMCTSPlayer(uint64_t seed, NNMCTSConfig config, const NeuralNet& nn);

    Move choose_move(const GameState& observable_state,
                     const std::vector<Move>& legal_moves) override;
    std::string name() const override { return name_; }
    void set_name(const std::string& n) { name_ = n; }

    std::vector<ChainAnalysis> analyze_chains(const GameState& observable_state);

    void set_config(NNMCTSConfig config) { config_ = config; }

private:
    std::mt19937 rng_;
    NNMCTSConfig config_;
    const NeuralNet& nn_;
    std::string name_ = "NN-MCTS";

    // Cached turn plan for choose_move
    std::vector<Move> planned_moves_;
    size_t plan_idx_ = 0;
};

} // namespace skipbo
