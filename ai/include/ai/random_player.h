#pragma once

#include "ai/player.h"
#include <random>

namespace skipbo {

class RandomPlayer : public Player {
public:
    explicit RandomPlayer(uint64_t seed = 42);
    Move choose_move(const GameState& observable_state,
                     const std::vector<Move>& legal_moves) override;
    std::string name() const override { return "Random"; }

private:
    std::mt19937 rng_;
};

} // namespace skipbo
