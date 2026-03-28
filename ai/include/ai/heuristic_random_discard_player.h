#pragma once

#include "ai/player.h"
#include <random>

namespace skipbo {

class HeuristicRandomDiscardPlayer : public Player {
public:
    Move choose_move(const GameState& observable_state,
                     const std::vector<Move>& legal_moves) override;
    std::string name() const override { return "HeuristicRandomDiscard"; }

private:
    // Get the card value a move would play (resolving skipbo to the needed value)
    static Card get_card(const GameState& state, const Move& move);

    std::mt19937 rng_{std::random_device{}()};
};

} // namespace skipbo
