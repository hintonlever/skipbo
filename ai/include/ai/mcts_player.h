#pragma once

#include "ai/player.h"
#include "ai/turn_action.h"
#include <random>
#include <vector>

namespace skipbo {

struct MoveAnalysis {
    Move move;
    double win_probability;
    int total_visits;
};

// Analysis result for a complete turn action (chain of moves)
struct ChainAnalysis {
    TurnAction action;
    double reward;       // average reward (from root player's perspective)
    int total_visits;
};

struct MCTSConfig {
    int num_determinizations = 20;
    int iterations_per_det = 500;
    double exploration = 1.414;
    int max_turn_depth = 4; // max turns per player in the tree
};

class MCTSPlayer : public Player {
public:
    explicit MCTSPlayer(uint64_t seed = 42, MCTSConfig config = {});

    Move choose_move(const GameState& observable_state,
                     const std::vector<Move>& legal_moves) override;
    std::string name() const override {
        return std::string("MCTS-t") + std::to_string(config_.max_turn_depth);
    }

    // Analyze turn actions for the current player. Returns per-chain analysis.
    std::vector<ChainAnalysis> analyze_chains(const GameState& observable_state);

    // Legacy: per-move analysis (extracts first move from best chains)
    std::vector<MoveAnalysis> analyze_moves(const GameState& observable_state,
                                            const std::vector<Move>& legal_moves);

    void set_config(MCTSConfig config) { config_ = config; }

private:
    std::mt19937 rng_;
    MCTSConfig config_;

    // Cached turn plan for choose_move
    std::vector<Move> planned_moves_;
    size_t plan_idx_ = 0;
};

} // namespace skipbo
