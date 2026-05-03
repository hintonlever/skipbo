#pragma once

#include "ai/player.h"
#include <random>
#include <string>

namespace skipbo {

// Heuristic player driven by an explicit, ablation-friendly set of rules.
// Each rule contributes a weighted score per legal move. The chosen move is
// the highest-scoring one (ties broken by seeded RNG). Toggle a rule by
// setting `enable_*` to false; tune intensity via `w_*`.
//
// Rules (start of project, will grow as ablation surfaces more):
//   1. play_stock_when_possible — strongly prefer moves that source from the stock pile.
//   2. avoid_burying_low_cards  — penalise discarding a high card onto a pile topped by a lower card (which would bury that lower card).
//   3. prefer_short_discard_piles — when discarding, prefer shorter / empty target discard piles.
//   4. block_opponent_stock     — penalise plays that leave a building pile at (opponent_stock_top - 1), priming the opponent.
//   5. save_wildcards           — penalise spending a Skip-Bo wild from hand on a building pile (stock-sourced wild plays are not penalised).
struct RulesConfig {
    bool enable_play_stock         = true;
    bool enable_avoid_burying      = true;
    bool enable_short_discard      = true;
    bool enable_block_opponent     = true;
    bool enable_save_wildcards     = true;

    double w_play_stock        = 1000.0;  // dominant; effectively forces stock plays when legal
    double w_avoid_burying     = 5.0;
    double w_short_discard     = 3.0;
    double w_block_opponent    = 50.0;
    double w_save_wildcards    = 20.0;
};

class RulesPlayer : public Player {
public:
    explicit RulesPlayer(uint64_t seed = 42, RulesConfig config = {}, std::string name = "rules");

    Move choose_move(const GameState& observable_state,
                     const std::vector<Move>& legal_moves) override;
    std::string name() const override { return name_; }

    const RulesConfig& config() const { return config_; }

private:
    RulesConfig config_;
    std::mt19937 rng_;
    std::string name_;

    double score_move(const GameState& state, const Move& move) const;
};

} // namespace skipbo
