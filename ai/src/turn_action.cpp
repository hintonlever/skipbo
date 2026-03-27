#include "ai/turn_action.h"
#include "engine/rules.h"
#include "engine/game.h"
#include <algorithm>
#include <unordered_set>
#include <queue>

namespace skipbo {

// ---- Hashing helpers ----

// Hash a single discard pile (full contents, order matters)
static uint64_t hash_discard_pile(const GameState& state, int player, int pile) {
    const auto& dp = state.players[player].discard_piles[pile];
    uint64_t h = dp.size();
    for (int i = 0; i < dp.size(); i++)
        h = h * 31 + dp[i];
    return h;
}

// Hash a player's state with pile-order independence:
// Building piles sorted by count, discard piles sorted by content hash.
// This means swapping two piles with identical state doesn't change the hash.
static uint64_t hash_player_state(const GameState& state, int player) {
    uint64_t h = 0;

    // Building piles — sorted by count for position independence
    uint8_t bp[NUM_BUILDING_PILES];
    for (int i = 0; i < NUM_BUILDING_PILES; i++) bp[i] = state.building_pile_count[i];
    std::sort(bp, bp + NUM_BUILDING_PILES);
    for (int i = 0; i < NUM_BUILDING_PILES; i++) h = h * 31 + bp[i];

    // Hand — sorted by value for order independence
    const auto& p = state.players[player];
    Card sorted_hand[HAND_SIZE];
    int n = p.hand_count;
    for (int i = 0; i < n; i++) sorted_hand[i] = p.hand[i];
    std::sort(sorted_hand, sorted_hand + n);
    for (int i = 0; i < n; i++) h = h * 31 + sorted_hand[i];
    h = h * 31 + n;

    // Stock
    h = h * 31 + p.stock_size();
    if (!p.stock_empty()) h = h * 31 + p.stock_top();

    // Discard piles — sorted by content hash for position independence
    uint64_t dp_hashes[NUM_DISCARD_PILES];
    for (int d = 0; d < NUM_DISCARD_PILES; d++)
        dp_hashes[d] = hash_discard_pile(state, player, d);
    std::sort(dp_hashes, dp_hashes + NUM_DISCARD_PILES);
    for (int d = 0; d < NUM_DISCARD_PILES; d++)
        h = h * 1000003 + dp_hashes[d];

    return h;
}

// ---- Equivalence collapsing helpers ----

// Get the card value a move would play
static Card get_move_card(const GameState& state, const Move& m) {
    const auto& p = state.players[state.current_player];
    if (m.is_from_hand()) return p.hand[m.hand_index()];
    if (m.is_from_stock()) return p.stock_top();
    if (m.is_from_discard()) return p.discard_top(m.source_discard_index());
    return CARD_NONE;
}

// Collapse build moves: piles with the same count are interchangeable.
// Key = (card_value, pile_count, source_kind).
// Hand cards with the same value are interchangeable.
static void collapse_build_moves(const GameState& state, MoveList& moves) {
    struct Key {
        Card card;
        uint8_t pile_count;
        uint8_t source_kind; // 0=hand, 1=stock, 2+=discard pile index+2
    };

    MoveList result;
    Key seen[MAX_LEGAL_MOVES];
    int seen_count = 0;

    for (int i = 0; i < moves.size(); i++) {
        const Move& m = moves[i];
        Card card = get_move_card(state, m);
        uint8_t pile_count = state.building_pile_count[m.target_building_index()];
        uint8_t source_kind;
        if (m.is_from_hand()) source_kind = 0;
        else if (m.is_from_stock()) source_kind = 1;
        else source_kind = 2 + m.source_discard_index();

        bool dup = false;
        for (int j = 0; j < seen_count; j++) {
            if (seen[j].card == card && seen[j].pile_count == pile_count &&
                seen[j].source_kind == source_kind) {
                dup = true;
                break;
            }
        }
        if (!dup) {
            seen[seen_count++] = {card, pile_count, source_kind};
            result.push_back(m);
        }
    }
    moves = result;
}

// Collapse discard moves: piles with identical full contents are interchangeable.
// Key = (card_value, pile_content_hash).
static void collapse_discard_moves(const GameState& state, MoveList& moves) {
    int cp = state.current_player;
    struct Key {
        Card card;
        uint64_t pile_hash;
    };

    MoveList result;
    Key seen[MAX_LEGAL_MOVES];
    int seen_count = 0;

    for (int i = 0; i < moves.size(); i++) {
        const Move& m = moves[i];
        Card card = get_move_card(state, m);
        int di = m.target_discard_index();
        uint64_t ph = hash_discard_pile(state, cp, di);

        bool dup = false;
        for (int j = 0; j < seen_count; j++) {
            if (seen[j].card == card && seen[j].pile_hash == ph) {
                dup = true;
                break;
            }
        }
        if (!dup) {
            seen[seen_count++] = {card, ph};
            result.push_back(m);
        }
    }
    moves = result;
}

// ---- Chain generation via BFS ----

struct BuildPath {
    GameState state;
    Move moves[TurnAction::MAX_MOVES];
    int num_moves;
};

std::vector<TurnAction> generate_turn_actions(
    const GameState& state, std::mt19937& rng, int max_actions) {

    int cp = state.current_player;
    std::vector<TurnAction> actions;
    std::unordered_set<uint64_t> visited;

    // BFS over build sequences
    std::queue<BuildPath> queue;
    BuildPath start;
    start.state = state;
    start.num_moves = 0;
    queue.push(start);
    visited.insert(hash_player_state(state, cp));

    // Collect leaf states (maximal build-outs and the initial state)
    std::vector<BuildPath> leaves;

    // Always include the starting state as a leaf (immediate discard option)
    leaves.push_back(start);

    int max_explored = 500;
    int explored = 0;

    while (!queue.empty() && explored < max_explored) {
        BuildPath bp = queue.front();
        queue.pop();
        explored++;

        // Get all non-discard legal moves
        MoveList all_moves;
        get_legal_moves(bp.state, all_moves);

        MoveList build_moves;
        for (int i = 0; i < all_moves.size(); i++) {
            if (!all_moves[i].is_discard()) {
                build_moves.push_back(all_moves[i]);
            }
        }

        // Collapse equivalent builds
        collapse_build_moves(bp.state, build_moves);

        if (build_moves.empty()) {
            // No more builds possible - this is a maximal leaf
            // (already added to leaves when first seen, or add now)
            bool found = false;
            uint64_t h = hash_player_state(bp.state, cp);
            for (const auto& leaf : leaves) {
                if (hash_player_state(leaf.state, cp) == h) {
                    found = true;
                    break;
                }
            }
            if (!found) leaves.push_back(bp);
            continue;
        }

        // Explore each unique build move
        for (int i = 0; i < build_moves.size(); i++) {
            if (bp.num_moves >= TurnAction::MAX_MOVES - 1) continue; // leave room for discard

            BuildPath next;
            next.state = bp.state;
            for (int j = 0; j < bp.num_moves; j++) next.moves[j] = bp.moves[j];
            next.moves[bp.num_moves] = build_moves[i];
            next.num_moves = bp.num_moves + 1;

            Game::apply_move_to_state(next.state, build_moves[i], &rng);

            uint64_t h = hash_player_state(next.state, cp);
            if (visited.count(h)) continue;
            visited.insert(h);

            queue.push(next);
            leaves.push_back(next); // every reachable state is a potential leaf
        }
    }

    // For each leaf state, generate discard options
    std::unordered_set<uint64_t> action_hashes;

    for (const auto& leaf : leaves) {
        MoveList all_moves;
        get_legal_moves(leaf.state, all_moves);

        MoveList discard_moves;
        for (int i = 0; i < all_moves.size(); i++) {
            if (all_moves[i].is_discard()) {
                discard_moves.push_back(all_moves[i]);
            }
        }

        collapse_discard_moves(leaf.state, discard_moves);

        for (int i = 0; i < discard_moves.size(); i++) {
            TurnAction ta;
            for (int j = 0; j < leaf.num_moves; j++) ta.add(leaf.moves[j]);
            ta.add(discard_moves[i]);

            // Dedup entire actions by hashing the resulting state.
            // Hash BOTH players since the discard affects the current player's piles.
            GameState result_state = leaf.state;
            Game::apply_move_to_state(result_state, discard_moves[i]);
            uint64_t h = hash_player_state(result_state, cp) * 1000003 +
                         hash_player_state(result_state, 1 - cp);
            if (action_hashes.count(h)) continue;
            action_hashes.insert(h);

            actions.push_back(ta);
            if (static_cast<int>(actions.size()) >= max_actions) goto done;
        }

        // If no discards (hand empty, turn continues), add as partial action
        if (discard_moves.empty() && leaf.num_moves > 0) {
            TurnAction ta;
            for (int j = 0; j < leaf.num_moves; j++) ta.add(leaf.moves[j]);
            actions.push_back(ta);
            if (static_cast<int>(actions.size()) >= max_actions) goto done;
        }
    }

done:
    return actions;
}

// ---- Apply turn action ----

bool apply_turn_action(GameState& state, const TurnAction& action,
                       std::mt19937* rng) {
    for (int i = 0; i < action.num_moves; i++) {
        if (!Game::apply_move_to_state(state, action.moves[i], rng)) {
            return false;
        }
    }
    return true;
}

// ---- Static evaluation ----

double static_eval(const GameState& state, int perspective,
                   int root_my_stock, int root_opp_stock) {
    int my = perspective;
    int opp = 1 - perspective;

    // Stock progress (dominant signal)
    double my_progress = root_my_stock - state.players[my].stock_size();
    double opp_progress = root_opp_stock - state.players[opp].stock_size();
    double score = my_progress - opp_progress;

    // Win/loss bonus
    if (state.game_over) {
        return state.winner == my ? 100.0 : -100.0;
    }

    // Building pile proximity to my stock top
    if (!state.players[my].stock_empty()) {
        Card my_stock = state.players[my].stock_top();
        if (is_skipbo(my_stock)) {
            score += 0.8; // skipbo always playable next turn
        } else {
            int min_gap = 13;
            for (int b = 0; b < NUM_BUILDING_PILES; b++) {
                int needs = state.building_pile_count[b] + 1;
                int gap = static_cast<int>(my_stock) - needs;
                if (gap >= 0 && gap < min_gap) min_gap = gap;
            }
            if (min_gap <= 12) {
                score += (6.0 - min_gap) * 0.15;
            }
        }
    }

    // Opponent proximity (penalty)
    if (!state.players[opp].stock_empty()) {
        Card opp_stock = state.players[opp].stock_top();
        if (is_skipbo(opp_stock)) {
            score -= 0.8;
        } else {
            int min_gap = 13;
            for (int b = 0; b < NUM_BUILDING_PILES; b++) {
                int needs = state.building_pile_count[b] + 1;
                int gap = static_cast<int>(opp_stock) - needs;
                if (gap >= 0 && gap < min_gap) min_gap = gap;
            }
            if (min_gap <= 12) {
                score -= (6.0 - min_gap) * 0.15;
            }
        }
    }

    // Hand playability bonus (minor)
    const auto& me = state.players[my];
    int playable = 0;
    for (int h = 0; h < me.hand_count; h++) {
        Card c = me.hand[h];
        for (int b = 0; b < NUM_BUILDING_PILES; b++) {
            if (state.can_play_on_building(c, b)) { playable++; break; }
        }
    }
    score += playable * 0.05;

    return score;
}

} // namespace skipbo
