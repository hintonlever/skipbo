#pragma once

#include "engine/game_state.h"
#include "engine/move.h"
#include <random>
#include <functional>

namespace skipbo {

class Game {
public:
    explicit Game(uint64_t seed);

    // Set up the game: deal stock piles and initial hands
    void setup();

    // Start a new turn for the current player (draw up to 5 cards)
    void start_turn();

    // Apply a move. Returns true if the move was applied successfully.
    // If the move is a discard, also ends the turn and switches players.
    // If the move empties the stock pile, sets game_over.
    // If the move plays all hand cards, draws 5 new cards.
    bool apply_move(const Move& move);

    const GameState& state() const { return state_; }
    bool is_game_over() const { return state_.game_over; }
    int winner() const { return state_.winner; }
    uint8_t current_player() const { return state_.current_player; }

    // Pass the current player's turn (used when no legal moves available)
    void pass_turn();

    // For MCTS: apply a move to an arbitrary state (no drawing, no RNG needed
    // except for building pile completion recycling which we skip in simulation)
    static bool apply_move_to_state(GameState& state, const Move& move);

private:
    GameState state_;
    std::mt19937 rng_;

    void draw_cards_for_player(int player_id, int count);
    void check_building_pile_completion(int pile);
};

} // namespace skipbo
