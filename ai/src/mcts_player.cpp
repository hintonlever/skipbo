#include "ai/mcts_player.h"
#include "ai/mcts_node.h"
#include "ai/determinizer.h"
#include "engine/rules.h"
#include "engine/game.h"
#include <cassert>
#include <map>

namespace skipbo {

MCTSPlayer::MCTSPlayer(uint64_t seed, MCTSConfig config)
    : rng_(seed), config_(config) {}

static double rollout(GameState state, int perspective, std::mt19937& rng) {
    std::vector<Move> moves;
    int max_moves = 500; // safety limit
    int consecutive_passes = 0;
    while (!state.game_over && max_moves-- > 0 && consecutive_passes < 4) {
        moves.clear();
        get_legal_moves(state, moves);
        if (moves.empty()) {
            // No legal moves — pass turn
            state.current_player = 1 - state.current_player;
            consecutive_passes++;
            continue;
        }
        consecutive_passes = 0;
        std::uniform_int_distribution<size_t> dist(0, moves.size() - 1);
        Game::apply_move_to_state(state, moves[dist(rng)]);
    }
    if (state.winner == perspective) return 1.0;
    if (state.winner == 1 - perspective) return 0.0;
    // Stalemate — evaluate by who has fewer stock pile cards
    int my_stock = state.players[perspective].stock_size();
    int opp_stock = state.players[1 - perspective].stock_size();
    if (my_stock < opp_stock) return 0.75;
    if (my_stock > opp_stock) return 0.25;
    return 0.5;
}

struct MoveHash {
    size_t operator()(const Move& m) const {
        return (static_cast<size_t>(m.source) << 8) | static_cast<size_t>(m.target);
    }
};

std::vector<MoveAnalysis> MCTSPlayer::analyze_moves(
        const GameState& observable_state,
        const std::vector<Move>& legal_moves) {

    if (legal_moves.empty()) return {};

    // Accumulate visit counts across determinizations
    std::map<size_t, double> move_scores;  // move index -> aggregated score
    std::map<size_t, int> move_total_visits;

    for (int d = 0; d < config_.num_determinizations; ++d) {
        GameState det_state = Determinizer::sample(
            observable_state, observable_state.current_player, rng_);

        MCTSNode root(legal_moves);
        int my_id = observable_state.current_player;

        for (int iter = 0; iter < config_.iterations_per_det; ++iter) {
            GameState sim_state = det_state;
            MCTSNode* node = &root;

            // SELECT
            while (node->fully_expanded() && !node->is_leaf()) {
                node = node->select_child();
                Game::apply_move_to_state(sim_state, node->move);
            }

            // EXPAND
            if (!node->fully_expanded() && !sim_state.game_over) {
                std::uniform_int_distribution<size_t> dist(
                    0, node->untried_moves.size() - 1);
                size_t idx = dist(rng_);
                Move expand_move = node->untried_moves[idx];
                Game::apply_move_to_state(sim_state, expand_move);
                auto next_moves = get_legal_moves(sim_state);
                node = node->add_child(expand_move, next_moves);
            }

            // ROLLOUT
            double reward = rollout(sim_state, my_id, rng_);

            // BACKPROPAGATE
            while (node != nullptr) {
                node->visits++;
                node->total_reward += reward;
                node = node->parent;
            }
        }

        // Aggregate scores from this determinization
        int total_root_visits = root.visits;
        for (const auto& child : root.children) {
            // Find which legal_move index this corresponds to
            for (size_t i = 0; i < legal_moves.size(); ++i) {
                if (legal_moves[i] == child->move) {
                    if (total_root_visits > 0) {
                        move_scores[i] += static_cast<double>(child->visits) / total_root_visits;
                    }
                    move_total_visits[i] += child->visits;
                    break;
                }
            }
        }
    }

    // Build results
    std::vector<MoveAnalysis> results;
    results.reserve(legal_moves.size());
    for (size_t i = 0; i < legal_moves.size(); ++i) {
        double score = move_scores.count(i) ? move_scores[i] / config_.num_determinizations : 0.0;
        int visits = move_total_visits.count(i) ? move_total_visits[i] : 0;
        results.push_back({legal_moves[i], score, visits});
    }
    return results;
}

Move MCTSPlayer::choose_move(const GameState& observable_state,
                              const std::vector<Move>& legal_moves) {
    assert(!legal_moves.empty());

    if (legal_moves.size() == 1) return legal_moves[0];

    auto analysis = analyze_moves(observable_state, legal_moves);

    // Pick move with highest win probability
    size_t best = 0;
    for (size_t i = 1; i < analysis.size(); ++i) {
        if (analysis[i].win_probability > analysis[best].win_probability) {
            best = i;
        }
    }
    return analysis[best].move;
}

} // namespace skipbo
