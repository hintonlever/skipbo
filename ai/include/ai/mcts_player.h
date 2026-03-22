#pragma once

#include "ai/player.h"
#include <random>

namespace skipbo {

struct MoveAnalysis {
    Move move;
    double win_probability;
    int total_visits;
};

struct MCTSConfig {
    int num_determinizations = 20;
    int iterations_per_det = 500;
    double exploration = 1.414;
    double rollout_heuristic_rate = 0.5; // 0.0 = pure random, 1.0 = pure heuristic
    int rollout_depth = 5; // moves per player (total moves = 2 * rollout_depth)
};

class MCTSPlayer : public Player {
public:
    explicit MCTSPlayer(uint64_t seed = 42, MCTSConfig config = {});

    Move choose_move(const GameState& observable_state,
                     const std::vector<Move>& legal_moves) override;
    std::string name() const override {
        int pct = static_cast<int>(config_.rollout_heuristic_rate * 100);
        return std::string("MCTS-") + std::to_string(pct) + "-d" + std::to_string(config_.rollout_depth);
    }

    std::vector<MoveAnalysis> analyze_moves(const GameState& observable_state,
                                            const std::vector<Move>& legal_moves);

    void set_config(MCTSConfig config) { config_ = config; }

private:
    std::mt19937 rng_;
    MCTSConfig config_;
};

} // namespace skipbo
