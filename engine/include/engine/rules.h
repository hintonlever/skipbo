#pragma once

#include "engine/game_state.h"
#include "engine/move.h"
#include <vector>

namespace skipbo {

// Generate all legal moves for the current player.
// Moves are appended to `moves` (caller should clear if needed).
void get_legal_moves(const GameState& state, std::vector<Move>& moves);

// Convenience wrapper that returns a new vector
std::vector<Move> get_legal_moves(const GameState& state);

// Fast zero-allocation overload using MoveList
void get_legal_moves(const GameState& state, MoveList& moves);

// Validate that a specific move is legal (O(1), no allocation)
bool is_legal_move(const GameState& state, const Move& move);

} // namespace skipbo
