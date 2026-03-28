#include "ai/nn_encoding.h"
#include <cstring>

namespace skipbo {

namespace {

// Write one-hot encoding for a card value at the given offset.
// Card 0 (SkipBo) maps to index 0, cards 1-12 map to indices 1-12.
// CARD_NONE or empty produces all zeros.
void write_card_one_hot(float* out, int offset, Card card) {
    for (int i = 0; i < 13; i++) out[offset + i] = 0.0f;
    if (card != CARD_NONE && card <= CARD_MAX) {
        out[offset + card] = 1.0f;
    }
}

// Encode a single player's non-hand features into the output at the given offset.
// Writes: stock top one-hot (13) + stock size (1) + 4 discard tops (52) + 4 discard sizes (4) = 70 floats
void encode_player(const PlayerState& ps, float* out, int offset) {
    // Stock top one-hot (13)
    Card st = ps.stock_empty() ? CARD_NONE : ps.stock_top();
    write_card_one_hot(out, offset, st);
    offset += 13;

    // Stock size normalized
    out[offset++] = static_cast<float>(ps.stock_size()) / 15.0f;

    // 4 discard pile tops one-hot (4 x 13 = 52)
    for (int d = 0; d < NUM_DISCARD_PILES; d++) {
        Card dt = ps.discard_empty(d) ? CARD_NONE : ps.discard_top(d);
        write_card_one_hot(out, offset, dt);
        offset += 13;
    }

    // 4 discard pile sizes normalized
    for (int d = 0; d < NUM_DISCARD_PILES; d++) {
        out[offset++] = static_cast<float>(ps.discard_piles[d].size()) / 20.0f;
    }
}

} // anonymous namespace

void encode_state(const GameState& state, int perspective, float* out) {
    std::memset(out, 0, STATE_ENCODING_SIZE * sizeof(float));

    const PlayerState& me = state.players[perspective];
    const PlayerState& opp = state.players[1 - perspective];

    int offset = 0;

    // [0..12] Hand histogram: count of each card type in hand
    for (int i = 0; i < HAND_SIZE; i++) {
        Card c = me.hand[i];
        if (c != CARD_NONE && c <= CARD_MAX) {
            out[c] += 1.0f;
        }
    }
    offset += 13;

    // [13..82] My player features (70 floats)
    encode_player(me, out, offset);
    offset += 70;

    // [83..152] Opponent player features (70 floats)
    encode_player(opp, out, offset);
    offset += 70;

    // [153..156] Building pile next-needed / 12
    for (int b = 0; b < NUM_BUILDING_PILES; b++) {
        out[offset++] = static_cast<float>(state.building_pile_needs(b)) / 12.0f;
    }

    // [157] Draw pile size / 162
    out[offset++] = static_cast<float>(state.draw_pile.size()) / 162.0f;
}

void encode_chain(const GameState& state, const TurnAction& action, float* out) {
    std::memset(out, 0, CHAIN_ENCODING_SIZE * sizeof(float));

    int num_builds = action.num_builds();

    // [0] Num builds / 12
    out[0] = static_cast<float>(num_builds) / 12.0f;

    // To compute building pile deltas, simulate the action on a copy
    // and compare building pile counts before/after.
    // But we can compute deltas directly from the moves: count builds per pile.
    int pile_deltas[NUM_BUILDING_PILES] = {};
    bool stock_played = false;
    int hand_cards_used = 0;
    bool discard_used[NUM_DISCARD_PILES] = {};
    int card_histogram[13] = {}; // cards 0-12 played as builds

    for (int i = 0; i < num_builds; i++) {
        const Move& m = action.moves[i];
        Card card = action.cards[i];

        // Target building pile
        int pile = m.target_building_index();
        pile_deltas[pile]++;

        // Source tracking
        if (m.is_from_stock()) {
            stock_played = true;
        } else if (m.is_from_hand()) {
            hand_cards_used++;
        } else if (m.is_from_discard()) {
            discard_used[m.source_discard_index()] = true;
        }

        // Card histogram
        if (card != CARD_NONE && card <= CARD_MAX) {
            card_histogram[card]++;
        }
    }

    // [1..4] Building pile deltas / 12
    for (int b = 0; b < NUM_BUILDING_PILES; b++) {
        out[1 + b] = static_cast<float>(pile_deltas[b]) / 12.0f;
    }

    // [5] Stock card played
    out[5] = stock_played ? 1.0f : 0.0f;

    // [6] Hand cards used / 5
    out[6] = static_cast<float>(hand_cards_used) / 5.0f;

    // [7..10] Which discard pile tops were used
    for (int d = 0; d < NUM_DISCARD_PILES; d++) {
        out[7 + d] = discard_used[d] ? 1.0f : 0.0f;
    }

    // [11..14] Discard target one-hot (4 piles)
    // [15] Hand empty after builds (no discard)
    if (action.has_discard()) {
        int tgt = action.discard().target_discard_index();
        out[11 + tgt] = 1.0f;
    } else {
        out[15] = 1.0f; // hand empty, no discard needed
    }

    // [16..28] Build card histogram
    for (int c = 0; c <= CARD_MAX; c++) {
        out[16 + c] = static_cast<float>(card_histogram[c]);
    }
}

} // namespace skipbo
