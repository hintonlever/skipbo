#pragma once

#include "ai/player.h"
#include <vector>
#include <string>

namespace skipbo {

// Dense layer for PPO network inference.
struct PPODenseLayer {
    std::vector<float> weights; // row-major [out_size x in_size]
    std::vector<float> bias;    // [out_size]
    int in_size = 0;
    int out_size = 0;
};

// Lightweight PPO network: state (158) -> action logits (80).
// Architecture: 158 -> 256 -> 128 -> 80
class PPONet {
public:
    PPONet() = default;

    // Load from flat weight vector: [w1, b1, w2, b2, w3, b3]
    void load(const std::vector<float>& flat_weights);

    // Forward pass: state -> logits (80)
    void forward(const float* state, float* logits) const;

    bool loaded() const { return !layers_.empty(); }

private:
    std::vector<PPODenseLayer> layers_;
};

// Player that uses a PPO-trained policy network.
// At each move, encodes the state, runs the actor network,
// masks illegal moves, and picks the highest-scoring legal move.
class PPOPlayer : public Player {
public:
    explicit PPOPlayer(const PPONet& net);

    Move choose_move(const GameState& observable_state,
                     const std::vector<Move>& legal_moves) override;
    std::string name() const override { return "PPO"; }

private:
    const PPONet& net_;

    // Map between flat action index and Move
    static int move_to_action_index(const Move& m);
    static Move action_index_to_move(int idx);
};

} // namespace skipbo
