#pragma once

#include "engine/game_state.h"
#include <random>

namespace skipbo {

class Determinizer {
public:
    // Given the observable state from `perspective` player's viewpoint,
    // sample a complete consistent game state by randomly assigning hidden cards.
    static GameState sample(const GameState& observable, int perspective,
                            std::mt19937& rng);
};

} // namespace skipbo
