#pragma once

#include "ai/player.h"
#include "engine/move.h"
#include <functional>
#include <random>
#include <string>

namespace skipbo {

// Pluggable rollout policy: pick a move from `legal_moves` in `state` using `rng`.
// `legal_moves` is a MoveList for zero-allocation hot-path use.
using PIMCRolloutPolicy = std::function<Move(const GameState& state,
                                              const MoveList& legal_moves,
                                              std::mt19937& rng)>;

struct PIMCConfig {
    int n_worlds = 10;            // determinized worlds sampled per move
    int n_simulations = 500;      // MCTS simulations per world
    int rollout_cap_turns = 200;  // max discards in a rollout before terminal-by-stock-count
    double ucb_c = 1.41421356237; // sqrt(2)
    PIMCRolloutPolicy rollout_policy{};  // empty -> defaults to uniform random
};

// Perfect-Information Monte Carlo. Determinizes hidden information into N concrete
// worlds, runs standard move-level MCTS in each, and aggregates root-child visit
// counts across worlds. Plays the move with the most aggregated visits.
class PIMCPlayer : public Player {
public:
    explicit PIMCPlayer(uint64_t seed = 42, PIMCConfig config = {}, std::string name = "pimc");

    Move choose_move(const GameState& observable_state,
                     const std::vector<Move>& legal_moves) override;
    std::string name() const override { return name_; }

    const PIMCConfig& config() const { return config_; }

private:
    PIMCConfig config_;
    std::mt19937 rng_;
    std::string name_;
};

} // namespace skipbo
