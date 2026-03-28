#include "ai/nn_mcts_player.h"
#include "ai/nn_encoding.h"
#include "ai/determinizer.h"
#include "ai/turn_action.h"
#include <cassert>
#include <algorithm>
#include <limits>
#include <cmath>
#include <memory>

namespace skipbo {

NNMCTSPlayer::NNMCTSPlayer(uint64_t seed, NNMCTSConfig config, const NeuralNet& nn)
    : rng_(seed), config_(config), nn_(nn) {}

// ---- NN-guided turn-level MCTS tree node ----

struct NNTurnNode {
    int action_index = -1;
    NNTurnNode* parent = nullptr;
    std::vector<std::unique_ptr<NNTurnNode>> children;
    int visits = 0;
    double total_reward = 0.0;
    double prior = 0.0;          // P(s,a) from policy network
    uint8_t acting_player = 0;

    std::vector<TurnAction> actions;
    std::vector<float> action_priors; // policy network priors for all actions
    bool actions_generated = false;
    int next_untried = 0;

    bool fully_expanded() const {
        if (!actions_generated) return false;
        if (actions.empty()) return true;
        return next_untried >= static_cast<int>(actions.size());
    }

    bool is_leaf() const {
        return children.empty() && actions_generated;
    }

    double puct_score(double c_puct) const {
        if (visits == 0) return std::numeric_limits<double>::infinity();
        double Q = total_reward / visits;
        double U = c_puct * prior * std::sqrt(static_cast<double>(parent->visits))
                   / (1.0 + visits);
        return Q + U;
    }

    NNTurnNode* select_child(double c_puct) {
        NNTurnNode* best = nullptr;
        double best_score = -std::numeric_limits<double>::infinity();
        for (auto& child : children) {
            double score = child->puct_score(c_puct);
            if (score > best_score) {
                best_score = score;
                best = child.get();
            }
        }
        return best;
    }

    NNTurnNode* expand() {
        assert(next_untried < static_cast<int>(actions.size()));
        int idx = next_untried++;
        auto child = std::make_unique<NNTurnNode>();
        child->action_index = idx;
        child->parent = this;
        // Set prior from precomputed policy scores
        if (idx < static_cast<int>(action_priors.size())) {
            child->prior = action_priors[idx];
        } else {
            child->prior = 1.0 / actions.size(); // uniform fallback
        }
        NNTurnNode* ptr = child.get();
        children.push_back(std::move(child));
        return ptr;
    }
};

// Compute policy priors for a node's actions using the neural network
static void compute_priors(const NeuralNet& nn, const GameState& state,
                           NNTurnNode* node) {
    int n = static_cast<int>(node->actions.size());
    if (n == 0) return;

    int perspective = node->acting_player;
    float state_enc[STATE_ENCODING_SIZE];
    encode_state(state, perspective, state_enc);

    std::vector<float> chain_encs(n * CHAIN_ENCODING_SIZE);
    for (int i = 0; i < n; i++) {
        encode_chain(state, node->actions[i],
                     chain_encs.data() + i * CHAIN_ENCODING_SIZE);
    }

    node->action_priors.resize(n);
    nn.evaluate_policy_batch(state_enc, chain_encs.data(), n,
                             node->action_priors.data());
}

// ---- Core NN-MCTS ----

std::vector<ChainAnalysis> NNMCTSPlayer::analyze_chains(
        const GameState& observable_state) {
    if (observable_state.game_over) return {};

    int my_id = observable_state.current_player;

    // Generate root actions from the observable state
    auto root_actions = generate_turn_actions(observable_state, rng_);
    if (root_actions.empty()) return {};

    // Precompute root policy priors
    float root_state_enc[STATE_ENCODING_SIZE];
    encode_state(observable_state, my_id, root_state_enc);

    int num_root = static_cast<int>(root_actions.size());
    std::vector<float> root_chain_encs(num_root * CHAIN_ENCODING_SIZE);
    for (int i = 0; i < num_root; i++) {
        encode_chain(observable_state, root_actions[i],
                     root_chain_encs.data() + i * CHAIN_ENCODING_SIZE);
    }
    std::vector<float> root_priors(num_root);
    nn_.evaluate_policy_batch(root_state_enc, root_chain_encs.data(),
                              num_root, root_priors.data());

    // Accumulate rewards across determinizations
    std::vector<double> agg_reward(root_actions.size(), 0.0);
    std::vector<int> agg_visits(root_actions.size(), 0);

    for (int d = 0; d < config_.num_determinizations; ++d) {
        GameState det_state = Determinizer::sample(
            observable_state, my_id, rng_);

        // Build tree root
        NNTurnNode root;
        root.acting_player = static_cast<uint8_t>(my_id);
        root.actions = root_actions;
        root.action_priors = root_priors;
        root.actions_generated = true;

        int max_turns = config_.max_turn_depth * 2;

        for (int iter = 0; iter < config_.iterations_per_det; ++iter) {
            GameState sim = det_state;
            NNTurnNode* node = &root;
            int turn_depth = 0;

            // SELECT
            while (node->fully_expanded() && !node->is_leaf() &&
                   turn_depth < max_turns) {
                node = node->select_child(config_.c_puct);
                const TurnAction& action = node->parent->actions[node->action_index];
                apply_turn_action(sim, action, &rng_);
                if (action.has_discard()) turn_depth++;
            }

            // EXPAND
            if (!node->fully_expanded() && !sim.game_over &&
                turn_depth < max_turns) {
                if (!node->actions_generated && node != &root) {
                    node->actions = generate_turn_actions(sim, rng_, 8);
                    node->actions_generated = true;
                    // Compute priors for non-root nodes
                    if (nn_.has_policy_network()) {
                        compute_priors(nn_, sim, node);
                    }
                }
                if (!node->actions.empty() && !node->fully_expanded()) {
                    NNTurnNode* child = node->expand();
                    child->acting_player = sim.current_player;
                    const TurnAction& action = node->actions[child->action_index];
                    apply_turn_action(sim, action, &rng_);
                    node = child;
                }
            }

            // EVALUATE with value network
            double eval;
            if (nn_.has_value_network() && !sim.game_over) {
                float state_enc[STATE_ENCODING_SIZE];
                encode_state(sim, my_id, state_enc);
                eval = nn_.evaluate_value(state_enc);
            } else if (sim.game_over) {
                eval = (sim.winner == my_id) ? 1.0 : -1.0;
            } else {
                eval = 0.0; // shouldn't happen if network is loaded
            }

            // BACKPROPAGATE
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

    std::sort(results.begin(), results.end(),
              [](const ChainAnalysis& a, const ChainAnalysis& b) {
                  return a.reward > b.reward;
              });

    return results;
}

Move NNMCTSPlayer::choose_move(const GameState& observable_state,
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
        planned_moves_.clear();
        plan_idx_ = 0;
    }

    if (legal_moves.size() == 1) {
        planned_moves_.clear();
        plan_idx_ = 0;
        return legal_moves[0];
    }

    auto chains = analyze_chains(observable_state);
    if (chains.empty()) {
        return legal_moves[0];
    }

    const TurnAction& best = chains[0].action;
    planned_moves_.clear();
    planned_moves_.reserve(best.num_moves);
    for (int i = 0; i < best.num_moves; i++) {
        planned_moves_.push_back(best.moves[i]);
    }

    if (!planned_moves_.empty()) {
        bool found = false;
        for (const auto& lm : legal_moves) {
            if (lm == planned_moves_[0]) { found = true; break; }
        }
        if (!found) {
            planned_moves_.clear();
            plan_idx_ = 0;
            return legal_moves[0];
        }
    }

    plan_idx_ = 1;
    return planned_moves_[0];
}

} // namespace skipbo
