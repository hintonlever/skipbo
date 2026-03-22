#include "engine/rules.h"

namespace skipbo {

// Templated core so both vector and MoveList paths share logic
template<typename MoveContainer>
static void generate_legal_moves(const GameState& state, MoveContainer& moves) {
    if (state.game_over) return;

    const auto& player = state.players[state.current_player];

    // Hand cards -> building piles or discard piles
    for (int h = 0; h < player.hand_count; ++h) {
        Card card = player.hand[h];
        auto source = static_cast<MoveSource>(static_cast<int>(MoveSource::Hand0) + h);

        // Can play on building piles
        if (card != CARD_NONE) {
            for (int b = 0; b < NUM_BUILDING_PILES; ++b) {
                if (state.can_play_on_building(card, b)) {
                    moves.push_back({source, static_cast<MoveTarget>(
                        static_cast<int>(MoveTarget::BuildingPile0) + b)});
                }
            }
        }

        // Can discard to any discard pile (ends turn)
        for (int d = 0; d < NUM_DISCARD_PILES; ++d) {
            moves.push_back({source, static_cast<MoveTarget>(
                static_cast<int>(MoveTarget::DiscardPile0) + d)});
        }
    }

    // Stock pile top -> building piles only
    if (!player.stock_empty()) {
        Card card = player.stock_top();
        for (int b = 0; b < NUM_BUILDING_PILES; ++b) {
            if (state.can_play_on_building(card, b)) {
                moves.push_back({MoveSource::StockPile, static_cast<MoveTarget>(
                    static_cast<int>(MoveTarget::BuildingPile0) + b)});
            }
        }
    }

    // Discard pile tops -> building piles only
    for (int d = 0; d < NUM_DISCARD_PILES; ++d) {
        if (!player.discard_empty(d)) {
            Card card = player.discard_top(d);
            auto source = static_cast<MoveSource>(
                static_cast<int>(MoveSource::DiscardPile0) + d);
            for (int b = 0; b < NUM_BUILDING_PILES; ++b) {
                if (state.can_play_on_building(card, b)) {
                    moves.push_back({source, static_cast<MoveTarget>(
                        static_cast<int>(MoveTarget::BuildingPile0) + b)});
                }
            }
        }
    }
}

void get_legal_moves(const GameState& state, std::vector<Move>& moves) {
    generate_legal_moves(state, moves);
}

std::vector<Move> get_legal_moves(const GameState& state) {
    std::vector<Move> moves;
    generate_legal_moves(state, moves);
    return moves;
}

void get_legal_moves(const GameState& state, MoveList& moves) {
    generate_legal_moves(state, moves);
}

bool is_legal_move(const GameState& state, const Move& move) {
    if (state.game_over) return false;
    const auto& player = state.players[state.current_player];

    // Discard target: only from hand
    if (move.is_discard()) {
        return move.is_from_hand() && move.hand_index() < player.hand_count;
    }

    // Building target: get card and check
    Card card = CARD_NONE;
    if (move.is_from_hand()) {
        int hi = move.hand_index();
        if (hi >= player.hand_count) return false;
        card = player.hand[hi];
    } else if (move.is_from_stock()) {
        if (player.stock_empty()) return false;
        card = player.stock_top();
    } else if (move.is_from_discard()) {
        int di = move.source_discard_index();
        if (player.discard_empty(di)) return false;
        card = player.discard_top(di);
    }

    if (card == CARD_NONE) return false;
    return state.can_play_on_building(card, move.target_building_index());
}

} // namespace skipbo
