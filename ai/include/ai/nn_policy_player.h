#pragma once

#include "ai/player.h"
#include "ai/neural_net.h"
#include <random>
#include <vector>
#include <string>

namespace skipbo {

// Player that uses the policy network to pick the best turn action directly,
// without MCTS search. Generates all turn actions, scores them with the policy
// network, and picks the highest-scored one.
class NNPolicyPlayer : public Player {
public:
    NNPolicyPlayer(uint64_t seed, const NeuralNet& nn);

    Move choose_move(const GameState& observable_state,
                     const std::vector<Move>& legal_moves) override;
    std::string name() const override { return "NN-Policy"; }

private:
    std::mt19937 rng_;
    const NeuralNet& nn_;

    // Cached turn plan
    std::vector<Move> planned_moves_;
    size_t plan_idx_ = 0;
};

} // namespace skipbo
