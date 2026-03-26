#include "ai/heuristic_player.h"
#include <cassert>
#include <cstdlib>

namespace skipbo {

// Helper: get the card value that a move would play
Card HeuristicPlayer::get_card(const GameState& state, const Move& move) {
    int cp = state.current_player;
    const auto& me = state.players[cp];

    if (move.is_from_hand()) {
        return me.hand[move.hand_index()];
    } else if (move.is_from_stock()) {
        return me.stock_top();
    } else if (move.is_from_discard()) {
        return me.discard_top(move.source_discard_index());
    }
    return CARD_NONE;
}

// Try to find a chain of moves from hand/discard that builds a pile up to
// (and including) a target value. Returns the first move to play, or nullopt.
// skip_bos_used is filled with the count of skipbos the chain would consume.
struct ChainResult {
    Move first_move;
    int skipbos_used;
    bool found;
};

static ChainResult find_chain_to_stock(const GameState& state, int pile_idx) {
    int cp = state.current_player;
    const auto& me = state.players[cp];
    Card stock_top = me.stock_top();
    if (stock_top == CARD_NONE) return {{}, 0, false};

    Card pile_needs = state.building_pile_needs(pile_idx);
    Card effective_stock = is_skipbo(stock_top) ? pile_needs : stock_top;

    // Stock can only play on this pile if it's the right value (or skipbo)
    if (!is_skipbo(stock_top) && stock_top != pile_needs) {
        // Stock doesn't directly go here. Check if we can chain up to it.
        if (stock_top < pile_needs) return {{}, 0, false}; // stock value already passed
        // We need to play cards pile_needs .. stock_top-1 from hand/discard,
        // then stock_top goes on the pile.
        effective_stock = stock_top;
    } else if (is_skipbo(stock_top)) {
        // Skipbo stock can go on any pile directly - no chain needed
        // Return this as a zero-cost chain (the stock->build move itself)
        // But the caller handles stock->build moves separately, so skip
        return {{}, 0, false};
    } else {
        // stock_top == pile_needs: stock goes directly on pile, no chain needed
        return {{}, 0, false};
    }

    // We need cards: pile_needs, pile_needs+1, ..., effective_stock-1 from hand/discard
    // Then we play stock_top (== effective_stock) on top.
    int chain_len = effective_stock - pile_needs;
    if (chain_len <= 0 || chain_len > 12) return {{}, 0, false};

    // Collect available cards from hand and discard tops
    // For each needed value, find a source (prefer non-skipbo)
    struct CardSource {
        MoveSource source;
        bool is_skipbo;
    };

    // Track which hand indices and discard piles are "used" in the chain
    bool hand_used[HAND_SIZE] = {};
    bool discard_used[NUM_DISCARD_PILES] = {};
    int skipbos_used = 0;

    // For each value in the chain, find a source
    struct ChainStep {
        MoveSource source;
    };
    ChainStep steps[12]; // max chain length

    for (int i = 0; i < chain_len; i++) {
        Card needed = pile_needs + i;
        bool found = false;

        // First pass: look for exact numbered card in hand
        for (int h = 0; h < me.hand_count; h++) {
            if (!hand_used[h] && me.hand[h] == needed) {
                steps[i].source = static_cast<MoveSource>(static_cast<int>(MoveSource::Hand0) + h);
                hand_used[h] = true;
                found = true;
                break;
            }
        }
        if (found) continue;

        // Second pass: look for exact numbered card in discard tops
        for (int d = 0; d < NUM_DISCARD_PILES; d++) {
            if (!discard_used[d] && !me.discard_piles[d].empty() &&
                me.discard_top(d) == needed) {
                steps[i].source = static_cast<MoveSource>(
                    static_cast<int>(MoveSource::DiscardPile0) + d);
                discard_used[d] = true;
                found = true;
                break;
            }
        }
        if (found) continue;

        // Third pass: use skipbo from hand (reluctantly)
        for (int h = 0; h < me.hand_count; h++) {
            if (!hand_used[h] && is_skipbo(me.hand[h])) {
                steps[i].source = static_cast<MoveSource>(static_cast<int>(MoveSource::Hand0) + h);
                hand_used[h] = true;
                skipbos_used++;
                found = true;
                break;
            }
        }
        if (found) continue;

        // Can't complete the chain
        return {{}, 0, false};
    }

    // Chain is possible! Return the first move.
    auto target = static_cast<MoveTarget>(
        static_cast<int>(MoveTarget::BuildingPile0) + pile_idx);
    return {Move{steps[0].source, target}, skipbos_used, true};
}

int HeuristicPlayer::score_discard(const GameState& state, const Move& move) const {
    int cp = state.current_player;
    const auto& me = state.players[cp];
    int pile_idx = move.target_discard_index();
    Card card = get_card(state, move);
    if (card == CARD_NONE || is_skipbo(card)) return -10000; // never discard skipbo

    int score = 0;

    // Prefer discarding higher cards first (higher = better to get rid of)
    score += card * 10;

    const auto& pile = me.discard_piles[pile_idx];

    if (pile.empty()) {
        // Penalize opening a new pile (prefer grouping on existing piles)
        score -= 50;
    } else {
        Card top = pile.back();
        if (top == card) {
            // Same card grouping - very good
            score += 200;
        } else if (top != CARD_NONE && is_numbered(top)) {
            // Sequential - card is one less than top (building down for later play)
            // We want ascending order on discard so we can play them off in sequence
            if (card == top - 1) {
                // Card is one below top - good sequential discard
                score += 150;
            } else if (card == top + 1) {
                // Card is one above top - also useful sequential
                score += 100;
            } else if (card > top) {
                // Higher card on lower - not ideal but keeps higher cards discarded
                score += 20;
            }
        }
    }

    // Count how many piles are non-empty
    int non_empty = 0;
    for (int d = 0; d < NUM_DISCARD_PILES; d++) {
        if (!me.discard_piles[d].empty()) non_empty++;
    }

    // If we'd be filling the last empty pile, penalize unless it's a match
    if (pile.empty() && non_empty >= 3) {
        // This would fill all 4 piles. Allow as sacrificial pile but penalize.
        score -= 100;
    }

    return score;
}

Move HeuristicPlayer::choose_move(const GameState& state,
                                   const std::vector<Move>& legal_moves) {
    assert(!legal_moves.empty());

    int cp = state.current_player;
    const auto& me = state.players[cp];
    int opp = 1 - cp;
    Card opp_stock_top = state.players[opp].stock_top();

    // Categorize moves
    std::vector<Move> stock_to_build;
    std::vector<Move> hand_to_build;
    std::vector<Move> discard_to_build;
    std::vector<Move> discard_moves;

    for (const auto& m : legal_moves) {
        if (m.is_discard()) {
            discard_moves.push_back(m);
        } else if (m.is_from_stock()) {
            stock_to_build.push_back(m);
        } else if (m.is_from_hand()) {
            hand_to_build.push_back(m);
        } else if (m.is_from_discard()) {
            discard_to_build.push_back(m);
        }
    }

    // --- PRIORITY 1: Play stock to build pile ---
    if (!stock_to_build.empty()) {
        // If multiple options, prefer the pile furthest from opponent's stock top
        Move best = stock_to_build[0];
        int best_score = -1000;
        for (const auto& m : stock_to_build) {
            int pile = m.target_building_index();
            Card pile_val = state.building_pile_count[pile] + 1; // what it becomes
            int score = 0;
            // Slightly prefer piles that don't help opponent
            if (opp_stock_top != CARD_NONE && is_numbered(opp_stock_top)) {
                int dist = std::abs(static_cast<int>(opp_stock_top) - static_cast<int>(pile_val));
                score += dist;
            }
            if (score > best_score) {
                best_score = score;
                best = m;
            }
        }
        return best;
    }

    // --- PRIORITY 2: Chain from hand/discard to enable stock play ---
    // For each building pile, check if a chain of hand/discard cards can
    // build up to the stock top value, then stock plays on top.
    {
        ChainResult best_chain = {{}, 100, false};
        for (int p = 0; p < NUM_BUILDING_PILES; p++) {
            auto chain = find_chain_to_stock(state, p);
            if (chain.found && chain.skipbos_used < best_chain.skipbos_used) {
                best_chain = chain;
            }
        }
        if (best_chain.found) {
            // Verify this move is actually in legal_moves
            for (const auto& m : legal_moves) {
                if (m == best_chain.first_move) return m;
            }
        }
    }

    // --- PRIORITY 3: Play non-skipbo hand cards to build piles ---
    // Goal: empty as many hand cards as possible (draw more next turn)
    // Constraint: don't build a pile to within 3 of opponent's stock top
    {
        Move best = {MoveSource::Hand0, MoveTarget::BuildingPile0};
        int best_score = -10000;
        bool found = false;

        for (const auto& m : hand_to_build) {
            Card card = get_card(state, m);
            if (is_skipbo(card)) continue; // keep skipbos sacred

            int pile = m.target_building_index();
            Card new_pile_val = state.building_pile_count[pile] + 1;

            int score = 100; // base score for playing a hand card

            // Check opponent stock proximity
            if (opp_stock_top != CARD_NONE && is_numbered(opp_stock_top)) {
                int dist = static_cast<int>(opp_stock_top) - static_cast<int>(new_pile_val);
                if (dist >= 0 && dist < 3) {
                    // This would bring the pile within 3 of opponent's stock
                    score -= 500;
                }
            }

            // Prefer playing cards that complete a pile (reach 12 = recycled)
            if (new_pile_val == CARD_MAX) {
                score += 300;
            }

            // Slight preference for lower-valued piles (more room to grow)
            score -= new_pile_val;

            if (score > best_score) {
                best_score = score;
                best = m;
                found = true;
            }
        }

        if (found && best_score > 0) return best;
    }

    // --- PRIORITY 4: Play discard tops to build piles ---
    // Same logic but from discard piles
    {
        Move best = {MoveSource::DiscardPile0, MoveTarget::BuildingPile0};
        int best_score = -10000;
        bool found = false;

        for (const auto& m : discard_to_build) {
            Card card = get_card(state, m);

            int pile = m.target_building_index();
            Card new_pile_val = state.building_pile_count[pile] + 1;

            int score = 80; // slightly lower priority than hand cards

            if (opp_stock_top != CARD_NONE && is_numbered(opp_stock_top)) {
                int dist = static_cast<int>(opp_stock_top) - static_cast<int>(new_pile_val);
                if (dist >= 0 && dist < 3) {
                    score -= 500;
                }
            }

            if (new_pile_val == CARD_MAX) {
                score += 300;
            }

            // Bonus: uncovering a useful card underneath
            int src_pile = m.source_discard_index();
            if (me.discard_piles[src_pile].size() > 1) {
                // There's a card underneath - good to uncover
                score += 20;
            }

            score -= new_pile_val;

            if (score > best_score) {
                best_score = score;
                best = m;
                found = true;
            }
        }

        if (found && best_score > 0) return best;
    }

    // --- PRIORITY 5: Play skipbo hand cards to build if they complete a pile ---
    // Only use skipbo to finish a pile (building_pile at 11, needs 12)
    for (const auto& m : hand_to_build) {
        Card card = get_card(state, m);
        if (!is_skipbo(card)) continue;
        int pile = m.target_building_index();
        if (state.building_pile_count[pile] == CARD_MAX - 1) {
            return m; // use skipbo to complete pile 11->12 (recycles the pile)
        }
    }

    // --- PRIORITY 6: Discard ---
    if (!discard_moves.empty()) {
        // Never discard a skipbo
        Move best = discard_moves[0];
        int best_score = -100000;

        for (const auto& m : discard_moves) {
            Card card = get_card(state, m);
            if (is_skipbo(card)) continue; // skip skipbo discards

            int score = score_discard(state, m);
            if (score > best_score) {
                best_score = score;
                best = m;
            }
        }

        // If best is still a skipbo (all hand cards are skipbo), pick any discard
        if (is_skipbo(get_card(state, best))) {
            // Forced to discard a skipbo. Pick the pile that wastes least.
            for (const auto& m : discard_moves) {
                return m; // just pick the first one
            }
        }

        return best;
    }

    // Fallback: return first legal move
    return legal_moves[0];
}

} // namespace skipbo
