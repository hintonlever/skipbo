#include "ai/rules_player.h"
#include <cassert>
#include <utility>

namespace skipbo {

namespace {

// Resolve the value a move would actually place on a building pile.
// For Skip-Bo wilds played to building, the resolved value is the pile-needs value.
// For discards, returns the raw card (Skip-Bo stays as wild).
Card resolved_card(const GameState& state, const Move& move) {
    const auto& player = state.players[state.current_player];
    Card card = CARD_NONE;
    if (move.is_from_hand()) {
        int hi = move.hand_index();
        if (hi < player.hand_count) card = player.hand[hi];
    } else if (move.is_from_stock()) {
        card = player.stock_top();
    } else if (move.is_from_discard()) {
        int di = move.source_discard_index();
        if (!player.discard_empty(di)) card = player.discard_top(di);
    }

    if (move.is_discard()) return card;
    if (is_skipbo(card)) {
        return state.building_pile_needs(move.target_building_index());
    }
    return card;
}

}  // namespace

RulesPlayer::RulesPlayer(uint64_t seed, RulesConfig config, std::string name)
    : config_(config), rng_(seed), name_(std::move(name)) {}

double RulesPlayer::score_move(const GameState& state, const Move& move) const {
    double score = 0.0;
    const auto& me = state.players[state.current_player];
    const auto& opp = state.players[1 - state.current_player];

    // Rule 1: prefer moves that source from the stock pile.
    if (config_.enable_play_stock && move.is_from_stock()) {
        score += config_.w_play_stock;
    }

    if (move.is_discard()) {
        int di = move.target_discard_index();
        const auto& target_pile = me.discard_piles[di];

        // Card we are discarding (raw — wild stays wild).
        Card card = CARD_NONE;
        if (move.is_from_hand()) {
            int hi = move.hand_index();
            if (hi < me.hand_count) card = me.hand[hi];
        }

        // Rule 2: avoid burying a lower-value card under a higher-value discard.
        // Skip-Bo wilds at top would not be "buried" in a meaningful sense; leave neutral.
        if (config_.enable_avoid_burying && card != CARD_NONE && !target_pile.empty()) {
            Card top = target_pile.back();
            if (is_numbered(top) && is_numbered(card) && card > top) {
                int gap = static_cast<int>(card) - static_cast<int>(top);
                score -= config_.w_avoid_burying * gap;
            }
        }

        // Rule 3: prefer shorter discard piles.
        if (config_.enable_short_discard) {
            score -= config_.w_short_discard * target_pile.size();
        }

        // Rule 5: penalise discarding a Skip-Bo wild (we want to keep wilds for builds).
        if (config_.enable_save_wildcards && is_skipbo(card)) {
            score -= config_.w_save_wildcards;
        }

        return score;
    }

    // Building-pile play.
    int bi = move.target_building_index();
    int new_count = state.building_pile_count[bi] + 1;

    // Rule 4: avoid leaving a building pile at (opponent_stock_top - 1), which primes the opponent.
    if (config_.enable_block_opponent && !opp.stock_empty()) {
        Card opp_stock = opp.stock_top();
        if (is_numbered(opp_stock) && new_count == static_cast<int>(opp_stock) - 1) {
            score -= config_.w_block_opponent;
        }
        // Mild reward for plays that push past the opponent's stock value (already non-helpful to them).
    }

    // Rule 5: penalise spending a Skip-Bo wild from hand on a building pile.
    // Wilds played from stock are forced (we keep Rule 1 dominant) so don't penalise those.
    if (config_.enable_save_wildcards && move.is_from_hand()) {
        int hi = move.hand_index();
        if (hi < me.hand_count && is_skipbo(me.hand[hi])) {
            score -= config_.w_save_wildcards;
        }
    }

    // (Resolved card not used yet — kept available for future rules.)
    (void)resolved_card;

    return score;
}

Move RulesPlayer::choose_move(const GameState& state,
                              const std::vector<Move>& legal_moves) {
    assert(!legal_moves.empty());

    double best_score = -1e18;
    int best_count = 0;
    int best_idx = 0;

    // Single-pass argmax with reservoir-style tie-break (deterministic given rng).
    for (size_t i = 0; i < legal_moves.size(); ++i) {
        double s = score_move(state, legal_moves[i]);
        if (s > best_score) {
            best_score = s;
            best_count = 1;
            best_idx = static_cast<int>(i);
        } else if (s == best_score) {
            ++best_count;
            std::uniform_int_distribution<int> d(0, best_count - 1);
            if (d(rng_) == 0) best_idx = static_cast<int>(i);
        }
    }

    return legal_moves[best_idx];
}

} // namespace skipbo
