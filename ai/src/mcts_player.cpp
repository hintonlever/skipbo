#include "ai/mcts_player.h"
#include "ai/determinizer.h"
#include "engine/rules.h"
#include "engine/game.h"
#include <cassert>
#include <map>
#include <algorithm>
#include <limits>

namespace skipbo {

MCTSPlayer::MCTSPlayer(uint64_t seed, MCTSConfig config)
    : rng_(seed), config_(config) {}

// ---- Turn-level MCTS tree node ----

struct TurnNode {
    int action_index = -1; // index into parent's action list (-1 for root)
    TurnNode* parent = nullptr;
    std::vector<std::unique_ptr<TurnNode>> children;
    int visits = 0;
    double total_reward = 0.0;
    uint8_t acting_player = 0;

    // Actions available from this node's state
    std::vector<TurnAction> actions;
    int next_untried = 0;
    bool actions_generated = false;

    bool fully_expanded() const {
        // Actions not yet generated — must expand to generate them first
        if (!actions_generated) return false;
        if (actions.empty()) return true; // no legal actions (game over or dead end)
        if (next_untried >= static_cast<int>(actions.size())) return true;
        // Progressive widening: only expand sqrt(visits) children.
        int max_children = std::max(1, static_cast<int>(std::sqrt(visits + 1)));
        return static_cast<int>(children.size()) >= max_children;
    }
    bool is_leaf() const { return children.empty() && actions_generated; }

    double ucb1(double exploration) const {
        if (visits == 0) return std::numeric_limits<double>::infinity();
        double exploit = total_reward / visits;
        double explore = exploration * std::sqrt(std::log(parent->visits) / visits);
        return exploit + explore;
    }

    TurnNode* select_child() {
        TurnNode* best = nullptr;
        double best_score = -std::numeric_limits<double>::infinity();
        for (auto& child : children) {
            double score = child->ucb1(1.414);
            if (score > best_score) {
                best_score = score;
                best = child.get();
            }
        }
        return best;
    }

    TurnNode* expand(std::mt19937& rng) {
        assert(next_untried < static_cast<int>(actions.size()));
        int idx = next_untried++;
        auto child = std::make_unique<TurnNode>();
        child->action_index = idx;
        child->parent = this;
        TurnNode* ptr = child.get();
        children.push_back(std::move(child));
        return ptr;
    }
};

// ---- Core turn-level MCTS ----

std::vector<ChainAnalysis> MCTSPlayer::analyze_chains(
        const GameState& observable_state) {

    if (observable_state.game_over) return {};

    int my_id = observable_state.current_player;
    int root_my_stock = observable_state.players[my_id].stock_size();
    int root_opp_stock = observable_state.players[1 - my_id].stock_size();

    // Generate root actions from the observable state
    auto root_actions = generate_turn_actions(observable_state, rng_);
    if (root_actions.empty()) return {};

    // Accumulate rewards across determinizations
    // Key: action index in root_actions, Value: (total_reward, total_visits)
    std::vector<double> agg_reward(root_actions.size(), 0.0);
    std::vector<int> agg_visits(root_actions.size(), 0);

    for (int d = 0; d < config_.num_determinizations; ++d) {
        GameState det_state = Determinizer::sample(
            observable_state, my_id, rng_);

        // Build tree root
        TurnNode root;
        root.acting_player = static_cast<uint8_t>(my_id);
        root.actions = root_actions;
        root.actions_generated = true;

        int max_turns = config_.max_turn_depth * 2; // per player * 2

        for (int iter = 0; iter < config_.iterations_per_det; ++iter) {
            GameState sim = det_state;
            TurnNode* node = &root;
            int turn_depth = 0;

            // SELECT
            while (node->fully_expanded() && !node->is_leaf() &&
                   turn_depth < max_turns) {
                node = node->select_child();
                // Apply this node's action to the sim state
                const TurnAction& action = node->parent->actions[node->action_index];
                apply_turn_action(sim, action, &rng_);
                if (action.has_discard()) turn_depth++;
            }

            // EXPAND
            if (!node->fully_expanded() && !sim.game_over &&
                turn_depth < max_turns) {
                // Generate actions for this node if not yet done.
                // Non-root nodes get a small budget (few actions) to keep
                // deeper search fast and focused.
                if (!node->actions_generated && node != &root) {
                    node->actions = generate_turn_actions(sim, rng_, 8);
                    node->actions_generated = true;
                }
                if (!node->actions.empty() && !node->fully_expanded()) {
                    TurnNode* child = node->expand(rng_);
                    child->acting_player = sim.current_player;
                    // Apply the expanded action
                    const TurnAction& action = node->actions[child->action_index];
                    apply_turn_action(sim, action, &rng_);
                    node = child;
                }
            }

            // EVALUATE (static eval, no rollout)
            double eval = static_eval(sim, my_id, root_my_stock, root_opp_stock);

            // BACKPROPAGATE with negation for opponent nodes
            while (node != nullptr) {
                node->visits++;
                node->total_reward += (node->acting_player == my_id) ? eval : -eval;
                node = node->parent;
            }
        }

        // Aggregate root children's results
        for (const auto& child : root.children) {
            int idx = child->action_index;
            if (idx >= 0 && idx < static_cast<int>(root_actions.size())) {
                if (child->visits > 0) {
                    agg_reward[idx] += child->total_reward;
                    agg_visits[idx] += child->visits;
                }
            }
        }
    }

    // Build results
    std::vector<ChainAnalysis> results;
    results.reserve(root_actions.size());
    for (size_t i = 0; i < root_actions.size(); i++) {
        double reward = agg_visits[i] > 0 ? agg_reward[i] / agg_visits[i] : 0.0;
        results.push_back({root_actions[i], reward, agg_visits[i]});
    }

    // Sort by reward descending
    std::sort(results.begin(), results.end(),
              [](const ChainAnalysis& a, const ChainAnalysis& b) {
                  return a.reward > b.reward;
              });

    return results;
}

// ---- choose_move: plan full turn, return moves one at a time ----

Move MCTSPlayer::choose_move(const GameState& observable_state,
                              const std::vector<Move>& legal_moves) {
    assert(!legal_moves.empty());

    // If we have a cached plan, try to continue it
    if (plan_idx_ < planned_moves_.size()) {
        Move m = planned_moves_[plan_idx_];
        for (const auto& lm : legal_moves) {
            if (lm == m) {
                plan_idx_++;
                return m;
            }
        }
        // Cached move not legal, replan
        planned_moves_.clear();
        plan_idx_ = 0;
    }

    // If only one legal move, just play it
    if (legal_moves.size() == 1) {
        planned_moves_.clear();
        plan_idx_ = 0;
        return legal_moves[0];
    }

    // Run turn-level MCTS
    auto chains = analyze_chains(observable_state);
    if (chains.empty()) {
        return legal_moves[0]; // fallback
    }

    // Pick the best chain and cache all its moves
    const TurnAction& best = chains[0].action;
    planned_moves_.clear();
    planned_moves_.reserve(best.num_moves);
    for (int i = 0; i < best.num_moves; i++) {
        planned_moves_.push_back(best.moves[i]);
    }

    // Verify the first move is legal
    if (!planned_moves_.empty()) {
        bool found = false;
        for (const auto& lm : legal_moves) {
            if (lm == planned_moves_[0]) { found = true; break; }
        }
        if (!found) {
            // First move not legal (shouldn't happen, but fallback)
            planned_moves_.clear();
            plan_idx_ = 0;
            return legal_moves[0];
        }
    }

    plan_idx_ = 1;
    return planned_moves_[0];
}

// ---- Legacy: per-move analysis ----

std::vector<MoveAnalysis> MCTSPlayer::analyze_moves(
        const GameState& observable_state,
        const std::vector<Move>& legal_moves) {

    auto chains = analyze_chains(observable_state);

    // Group by first move
    std::map<int, std::pair<double, int>> first_move_scores; // move_key -> (reward_sum, visits)
    for (const auto& ca : chains) {
        if (ca.action.num_moves == 0) continue;
        const Move& m = ca.action.moves[0];
        int key = static_cast<int>(m.source) * 100 + static_cast<int>(m.target);
        auto& entry = first_move_scores[key];
        entry.first += ca.reward * ca.total_visits;
        entry.second += ca.total_visits;
    }

    std::vector<MoveAnalysis> results;
    for (const auto& [key, val] : first_move_scores) {
        Move m;
        m.source = static_cast<MoveSource>(key / 100);
        m.target = static_cast<MoveTarget>(key % 100);
        double score = val.second > 0 ? val.first / val.second : 0.0;
        results.push_back({m, score, val.second});
    }
    return results;
}

} // namespace skipbo
