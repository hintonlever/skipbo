#pragma once

#include "engine/game.h"
#include "engine/rules.h"
#include "ai/random_player.h"
#include "ai/mcts_player.h"
#include <vector>
#include <random>

namespace skipbo {

// Simplified API for JavaScript consumption via Embind.
// Manages a single game: human (player 0) vs AI (player 1).
class WasmGameController {
public:
    explicit WasmGameController(int seed)
        : game_(static_cast<uint64_t>(seed)),
          ai_(static_cast<uint64_t>(seed + 1000)),
          rng_(static_cast<uint64_t>(seed)) {
        game_.setup();
    }

    // --- State queries ---

    bool isGameOver() const { return game_.is_game_over(); }
    int getWinner() const { return game_.winner(); }
    int getCurrentPlayer() const { return game_.current_player(); }

    // Human player's hand (player 0)
    std::vector<int> getHand() const {
        const auto& p = game_.state().players[0];
        std::vector<int> hand;
        for (int i = 0; i < p.hand_count; ++i) {
            hand.push_back(static_cast<int>(p.hand[i]));
        }
        return hand;
    }

    // Stock pile top card for a player (-1 if empty)
    int getStockTop(int player) const {
        const auto& p = game_.state().players[player];
        return p.stock_empty() ? -1 : static_cast<int>(p.stock_top());
    }

    int getStockSize(int player) const {
        return game_.state().players[player].stock_size();
    }

    // Building pile top card (-1 if empty)
    int getBuildingPileTop(int pile) const {
        const auto& bp = game_.state().building_piles[pile];
        return bp.empty() ? -1 : static_cast<int>(bp.back());
    }

    int getBuildingPileSize(int pile) const {
        return static_cast<int>(game_.state().building_piles[pile].size());
    }

    // Discard pile top card (-1 if empty)
    int getDiscardTop(int player, int pile) const {
        const auto& dp = game_.state().players[player].discard_piles[pile];
        return dp.empty() ? -1 : static_cast<int>(dp.back());
    }

    int getDiscardSize(int player, int pile) const {
        return static_cast<int>(game_.state().players[player].discard_piles[pile].size());
    }

    // --- Legal moves ---

    // Returns legal moves as flat array: [source0, target0, source1, target1, ...]
    std::vector<int> getLegalMoves() const {
        auto moves = get_legal_moves(game_.state());
        std::vector<int> result;
        result.reserve(moves.size() * 2);
        for (const auto& m : moves) {
            result.push_back(static_cast<int>(m.source));
            result.push_back(static_cast<int>(m.target));
        }
        return result;
    }

    // --- Actions ---

    // Apply a move (source, target as ints). Returns true if successful.
    bool applyMove(int source, int target) {
        Move m{static_cast<MoveSource>(source), static_cast<MoveTarget>(target)};
        return game_.apply_move(m);
    }

    // Let the AI play its turn (player 1). Returns when AI's turn is done.
    // Returns a flat array of moves the AI made: [source0, target0, source1, target1, ...]
    std::vector<int> playAITurn(int iterations, int determinizations) {
        MCTSConfig config;
        config.iterations_per_det = iterations;
        config.num_determinizations = determinizations;
        ai_.set_config(config);

        std::vector<int> moves_made;

        while (!game_.is_game_over() && game_.current_player() == 1) {
            auto legal = get_legal_moves(game_.state());
            if (legal.empty()) {
                game_.pass_turn();
                break;
            }

            Move chosen = ai_.choose_move(game_.state(), legal);
            moves_made.push_back(static_cast<int>(chosen.source));
            moves_made.push_back(static_cast<int>(chosen.target));
            game_.apply_move(chosen);
        }

        return moves_made;
    }

    // Pass the current player's turn (used when no legal moves)
    void passTurn() {
        game_.pass_turn();
    }

    // --- Analysis ---

    // Analyze moves for the current player. Returns flat array:
    // [source0, target0, winProb0*1000, source1, target1, winProb1*1000, ...]
    std::vector<int> analyzeMoves(int iterations, int determinizations) {
        MCTSConfig config;
        config.iterations_per_det = iterations;
        config.num_determinizations = determinizations;
        ai_.set_config(config);

        auto legal = get_legal_moves(game_.state());
        auto analysis = ai_.analyze_moves(game_.state(), legal);

        std::vector<int> result;
        result.reserve(analysis.size() * 3);
        for (const auto& a : analysis) {
            result.push_back(static_cast<int>(a.move.source));
            result.push_back(static_cast<int>(a.move.target));
            result.push_back(static_cast<int>(a.win_probability * 1000));
        }
        return result;
    }

private:
    Game game_;
    MCTSPlayer ai_;
    std::mt19937 rng_;
};

} // namespace skipbo
