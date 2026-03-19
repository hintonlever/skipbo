#pragma once

#include "engine/game_state.h"
#include "engine/move.h"
#include <vector>
#include <string>

namespace skipbo {

class Player {
public:
    virtual ~Player() = default;
    virtual Move choose_move(const GameState& observable_state,
                             const std::vector<Move>& legal_moves) = 0;
    virtual std::string name() const = 0;
};

} // namespace skipbo
