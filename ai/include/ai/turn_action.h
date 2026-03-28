#pragma once

#include "engine/move.h"
#include "engine/game_state.h"
#include "engine/game.h"
#include <vector>
#include <random>

namespace skipbo {

// A complete turn: zero or more build moves followed by a discard.
struct TurnAction {
    static constexpr int MAX_MOVES = 32;
    Move moves[MAX_MOVES];
    Card cards[MAX_MOVES]; // actual card value for each move (resolved at generation time)
    int num_moves = 0;

    void add(const Move& m, Card card = CARD_NONE) {
        cards[num_moves] = card;
        moves[num_moves++] = m;
    }
    bool has_discard() const { return num_moves > 0 && moves[num_moves - 1].is_discard(); }
    int num_builds() const { return has_discard() ? num_moves - 1 : num_moves; }
    const Move& discard() const { return moves[num_moves - 1]; }
};

// Generate all distinct complete turn actions (builds + discard) from the
// given state for the current player. Equivalent moves are collapsed:
//  - Build piles with the same count are interchangeable
//  - Empty discard piles are interchangeable
//  - Discard piles with the same top card are interchangeable
// Returns at most max_actions actions, ordered by a quick heuristic score.
std::vector<TurnAction> generate_turn_actions(
    const GameState& state, std::mt19937& rng,
    int max_actions = 200);

// Apply all moves in a turn action to a game state.
// Returns false if any move fails.
bool apply_turn_action(GameState& state, const TurnAction& action,
                       std::mt19937* rng = nullptr);

// Static evaluation of a game state from a player's perspective.
// Returns a score where positive = good for 'perspective' player.
// root_my_stock / root_opp_stock are the stock sizes at the search root.
double static_eval(const GameState& state, int perspective,
                   int root_my_stock, int root_opp_stock);

} // namespace skipbo
