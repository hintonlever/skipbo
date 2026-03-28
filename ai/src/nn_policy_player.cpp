#include "ai/nn_policy_player.h"
#include "ai/nn_encoding.h"
#include "ai/turn_action.h"
#include <cassert>
#include <algorithm>
#include <limits>

namespace skipbo {

NNPolicyPlayer::NNPolicyPlayer(uint64_t seed, const NeuralNet& nn)
    : rng_(seed), nn_(nn) {}

Move NNPolicyPlayer::choose_move(const GameState& observable_state,
                                  const std::vector<Move>& legal_moves) {
    assert(!legal_moves.empty());

    // Continue cached plan if valid
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

    // Generate all turn actions
    auto actions = generate_turn_actions(observable_state, rng_);
    if (actions.empty()) {
        return legal_moves[0];
    }

    // Score each action with the policy network and pick the best
    int my_id = observable_state.current_player;
    float state_enc[STATE_ENCODING_SIZE];
    encode_state(observable_state, my_id, state_enc);

    int best_idx = 0;
    float best_score = -std::numeric_limits<float>::infinity();

    for (int i = 0; i < static_cast<int>(actions.size()); i++) {
        float chain_enc[CHAIN_ENCODING_SIZE];
        encode_chain(observable_state, actions[i], chain_enc);
        float score = nn_.evaluate_policy(state_enc, chain_enc);
        if (score > best_score) {
            best_score = score;
            best_idx = i;
        }
    }

    const TurnAction& best = actions[best_idx];
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
