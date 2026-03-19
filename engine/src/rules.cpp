#include "engine/rules.h"

namespace skipbo {

static void add_building_moves(const GameState& state, MoveSource source, Card card,
                                std::vector<Move>& moves) {
    if (card == CARD_NONE) return;
    for (int b = 0; b < NUM_BUILDING_PILES; ++b) {
        if (state.can_play_on_building(card, b)) {
            moves.push_back({source, static_cast<MoveTarget>(
                static_cast<int>(MoveTarget::BuildingPile0) + b)});
        }
    }
}

void get_legal_moves(const GameState& state, std::vector<Move>& moves) {
    if (state.game_over) return;

    const auto& player = state.players[state.current_player];

    // Hand cards -> building piles or discard piles
    for (int h = 0; h < player.hand_count; ++h) {
        Card card = player.hand[h];
        auto source = static_cast<MoveSource>(static_cast<int>(MoveSource::Hand0) + h);

        // Can play on building piles
        add_building_moves(state, source, card, moves);

        // Can discard to any discard pile (ends turn)
        for (int d = 0; d < NUM_DISCARD_PILES; ++d) {
            moves.push_back({source, static_cast<MoveTarget>(
                static_cast<int>(MoveTarget::DiscardPile0) + d)});
        }
    }

    // Stock pile top -> building piles only
    if (!player.stock_empty()) {
        add_building_moves(state, MoveSource::StockPile, player.stock_top(), moves);
    }

    // Discard pile tops -> building piles only
    for (int d = 0; d < NUM_DISCARD_PILES; ++d) {
        if (!player.discard_empty(d)) {
            auto source = static_cast<MoveSource>(
                static_cast<int>(MoveSource::DiscardPile0) + d);
            add_building_moves(state, source, player.discard_top(d), moves);
        }
    }
}

std::vector<Move> get_legal_moves(const GameState& state) {
    std::vector<Move> moves;
    get_legal_moves(state, moves);
    return moves;
}

bool is_legal_move(const GameState& state, const Move& move) {
    auto moves = get_legal_moves(state);
    for (const auto& m : moves) {
        if (m == move) return true;
    }
    return false;
}

} // namespace skipbo
