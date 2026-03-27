#pragma once

#include "engine/game.h"
#include "engine/rules.h"
#include "ai/random_player.h"
#include "ai/heuristic_player.h"
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
        uint8_t cnt = game_.state().building_pile_count[pile];
        return cnt == 0 ? -1 : static_cast<int>(cnt);
    }

    int getBuildingPileSize(int pile) const {
        return static_cast<int>(game_.state().building_pile_count[pile]);
    }

    // Discard pile top card (-1 if empty)
    int getDiscardTop(int player, int pile) const {
        const auto& dp = game_.state().players[player].discard_piles[pile];
        return dp.empty() ? -1 : static_cast<int>(dp.back());
    }

    int getDiscardSize(int player, int pile) const {
        return static_cast<int>(game_.state().players[player].discard_piles[pile].size());
    }

    // All cards in a discard pile (bottom to top)
    std::vector<int> getDiscardPile(int player, int pile) const {
        const auto& dp = game_.state().players[player].discard_piles[pile];
        std::vector<int> cards;
        cards.reserve(dp.size());
        for (Card c : dp) cards.push_back(static_cast<int>(c));
        return cards;
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
        // Track Skip-Bo card usage before applying (card may be removed)
        if (!m.is_discard()) {
            Card card = CARD_NONE;
            const auto& player = game_.state().players[game_.current_player()];
            if (m.is_from_hand()) {
                int hi = m.hand_index();
                if (hi < player.hand_count) card = player.hand[hi];
            } else if (m.is_from_stock()) {
                card = player.stock_top();
            } else if (m.is_from_discard()) {
                card = player.discard_top(m.source_discard_index());
            }
            if (is_skipbo(card)) skipbo_played_[game_.current_player()]++;
        }
        return game_.apply_move(m);
    }

    // Let the MCTS AI play its turn (player 1). Returns when AI's turn is done.
    // Returns a flat array of moves the AI made: [source0, target0, source1, target1, ...]
    std::vector<int> playAITurn(int iterations, int determinizations, int heuristicPct, int rolloutDepth, int treeDepth) {
        MCTSConfig config;
        config.iterations_per_det = iterations;
        config.num_determinizations = determinizations;
        config.rollout_heuristic_rate = heuristicPct / 100.0;
        config.rollout_depth = rolloutDepth;
        config.max_tree_depth = treeDepth;
        ai_.set_config(config);

        return playAITurnWith(ai_);
    }

    // Let the heuristic AI play its turn (player 1).
    std::vector<int> playHeuristicAITurn() {
        return playAITurnWith(heuristic_ai_);
    }

    // Pass the current player's turn (used when no legal moves)
    void passTurn() {
        game_.pass_turn();
    }

    // --- Stats ---

    int getSkipBoPlayed(int player) const { return skipbo_played_[player]; }

    // --- Analysis ---

    // Analyze moves for the current player. Returns flat array:
    // [source0, target0, winProb0*1000, source1, target1, winProb1*1000, ...]
    std::vector<int> analyzeMoves(int iterations, int determinizations, int heuristicPct, int rolloutDepth, int treeDepth) {
        MCTSConfig config;
        config.iterations_per_det = iterations;
        config.num_determinizations = determinizations;
        config.rollout_heuristic_rate = heuristicPct / 100.0;
        config.rollout_depth = rolloutDepth;
        config.max_tree_depth = treeDepth;
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

    // Analyze MCTS tree for the current player (single determinization).
    // Returns flat array: [parentIdx, source, target, visits, avgReward*1000, ...]
    // Each node is 5 consecutive ints. Root has parentIdx=-1, source=-1, target=-1.
    std::vector<int> analyzeTree(int iterations, int heuristicPct, int rolloutDepth,
                                  int treeDepth, int vizMaxDepth, int vizTopN) {
        MCTSConfig config;
        config.iterations_per_det = iterations;
        config.num_determinizations = 1;
        config.rollout_heuristic_rate = heuristicPct / 100.0;
        config.rollout_depth = rolloutDepth;
        config.max_tree_depth = treeDepth;
        ai_.set_config(config);

        auto legal = get_legal_moves(game_.state());
        return ai_.analyze_tree(game_.state(), legal, vizMaxDepth, vizTopN);
    }

private:
    Game game_;
    MCTSPlayer ai_;
    HeuristicPlayer heuristic_ai_;
    std::mt19937 rng_;
    std::array<int, NUM_PLAYERS> skipbo_played_ = {0, 0};

    // Shared logic: play a full AI turn with any Player implementation
    std::vector<int> playAITurnWith(Player& player) {
        std::vector<int> moves_made;

        while (!game_.is_game_over() && game_.current_player() == 1) {
            auto legal = get_legal_moves(game_.state());
            if (legal.empty()) {
                game_.pass_turn();
                break;
            }

            Move chosen = player.choose_move(game_.state(), legal);
            moves_made.push_back(static_cast<int>(chosen.source));
            moves_made.push_back(static_cast<int>(chosen.target));
            // Track Skip-Bo usage for AI
            if (!chosen.is_discard()) {
                Card card = CARD_NONE;
                const auto& p = game_.state().players[1];
                if (chosen.is_from_hand()) {
                    int hi = chosen.hand_index();
                    if (hi < p.hand_count) card = p.hand[hi];
                } else if (chosen.is_from_stock()) {
                    card = p.stock_top();
                } else if (chosen.is_from_discard()) {
                    card = p.discard_top(chosen.source_discard_index());
                }
                if (is_skipbo(card)) skipbo_played_[1]++;
            }
            game_.apply_move(chosen);
        }

        return moves_made;
    }
};

// Run a single AI vs AI match. aiType: 0=random, 1=heuristic, 2=mcts
// Returns [winner, turns, p0StockRemaining, p1StockRemaining]
inline std::vector<int> wasm_run_match(
    int p0Type, int p0Iters, int p0Dets, int p0Heuristic, int p0Rollout, int p0Tree,
    int p1Type, int p1Iters, int p1Dets, int p1Heuristic, int p1Rollout, int p1Tree,
    int seed)
{
    auto make_player = [](int type, int iters, int dets, int heuristic, int rollout, int tree,
                          uint64_t s) -> std::unique_ptr<Player> {
        switch (type) {
            case 1: return std::make_unique<HeuristicPlayer>();
            case 2: {
                MCTSConfig cfg;
                cfg.iterations_per_det = iters;
                cfg.num_determinizations = dets;
                cfg.rollout_heuristic_rate = heuristic / 100.0;
                cfg.rollout_depth = rollout;
                cfg.max_tree_depth = tree;
                return std::make_unique<MCTSPlayer>(s, cfg);
            }
            default: return std::make_unique<RandomPlayer>(s);
        }
    };

    // Use distinct seed domains to avoid correlation between game RNG and AI RNG.
    // Game seed and AI seeds must not overlap.
    auto p0 = make_player(p0Type, p0Iters, p0Dets, p0Heuristic, p0Rollout, p0Tree,
                           static_cast<uint64_t>(seed + 1000));
    auto p1 = make_player(p1Type, p1Iters, p1Dets, p1Heuristic, p1Rollout, p1Tree,
                           static_cast<uint64_t>(seed + 2000));

    Game game(static_cast<uint64_t>(seed));
    game.setup();

    Player* players[2] = {p0.get(), p1.get()};
    int turns = 0;
    int consecutive_passes = 0;

    while (!game.is_game_over() && turns < 5000 && consecutive_passes < 4) {
        auto moves = get_legal_moves(game.state());
        if (moves.empty()) {
            game.pass_turn();
            consecutive_passes++;
            continue;
        }
        consecutive_passes = 0;

        int cp = game.current_player();
        Move chosen = players[cp]->choose_move(game.state(), moves);
        game.apply_move(chosen);

        if (chosen.is_discard()) ++turns;
    }

    int winner = game.winner();
    if (!game.is_game_over()) {
        int s0 = game.state().players[0].stock_size();
        int s1 = game.state().players[1].stock_size();
        if (s0 < s1) winner = 0;
        else if (s1 < s0) winner = 1;
        else winner = seed % 2;  // true tie: coin flip based on seed
    }

    int s0 = game.state().players[0].stock_size();
    int s1 = game.state().players[1].stock_size();
    return {winner, turns, s0, s1};
}

} // namespace skipbo
