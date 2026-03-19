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
};

class MCTSPlayer : public Player {
public:
    explicit MCTSPlayer(uint64_t seed = 42, MCTSConfig config = {});

    Move choose_move(const GameState& observable_state,
                     const std::vector<Move>& legal_moves) override;
    std::string name() const override { return "MCTS"; }

    std::vector<MoveAnalysis> analyze_moves(const GameState& observable_state,
                                            const std::vector<Move>& legal_moves);

    void set_config(MCTSConfig config) { config_ = config; }

private:
    std::mt19937 rng_;
    MCTSConfig config_;
};

} // namespace skipbo
