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
    int max_tree_depth = 5; // max depth of MCTS tree (select/expand phase)
    int rollout_depth = 10; // moves per player in rollout (total moves = 2 * rollout_depth)
};

class MCTSPlayer : public Player {
public:
    explicit MCTSPlayer(uint64_t seed = 42, MCTSConfig config = {});

    Move choose_move(const GameState& observable_state,
                     const std::vector<Move>& legal_moves) override;
    std::string name() const override {
        int pct = static_cast<int>(config_.rollout_heuristic_rate * 100);
        return std::string("MCTS-") + std::to_string(pct) +
               "-t" + std::to_string(config_.max_tree_depth) +
               "-r" + std::to_string(config_.rollout_depth);
    }

    std::vector<MoveAnalysis> analyze_moves(const GameState& observable_state,
                                            const std::vector<Move>& legal_moves);

    // Run a single determinization and serialize the MCTS tree.
    // Returns flat array: [parentIdx, source, target, visits, avgReward*1000, ...]
    // Each node is 5 consecutive ints. Root has parentIdx=-1, source=-1, target=-1.
    std::vector<int> analyze_tree(const GameState& observable_state,
                                  const std::vector<Move>& legal_moves,
                                  int viz_max_depth = 3, int viz_top_n = 10);

    void set_config(MCTSConfig config) { config_ = config; }

private:
    std::mt19937 rng_;
    MCTSConfig config_;
};

} // namespace skipbo
