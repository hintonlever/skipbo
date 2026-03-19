#include "ai/random_player.h"
#include <cassert>

namespace skipbo {

RandomPlayer::RandomPlayer(uint64_t seed) : rng_(seed) {}

Move RandomPlayer::choose_move(const GameState& /*observable_state*/,
                               const std::vector<Move>& legal_moves) {
    assert(!legal_moves.empty());
    std::uniform_int_distribution<size_t> dist(0, legal_moves.size() - 1);
    return legal_moves[dist(rng_)];
}

} // namespace skipbo
