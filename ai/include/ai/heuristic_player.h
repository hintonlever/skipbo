#pragma once

#include "ai/player.h"

namespace skipbo {

class HeuristicPlayer : public Player {
public:
    Move choose_move(const GameState& observable_state,
                     const std::vector<Move>& legal_moves) override;
    std::string name() const override { return "Heuristic"; }

private:
    // Get the card value a move would play (resolving skipbo to the needed value)
    static Card get_card(const GameState& state, const Move& move);

    // Score a discard move (higher = better discard choice)
    int score_discard(const GameState& state, const Move& move) const;
};

} // namespace skipbo
