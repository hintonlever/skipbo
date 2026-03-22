#include "ai/determinizer.h"
#include <algorithm>
#include <cassert>

namespace skipbo {

GameState Determinizer::sample(const GameState& state, int perspective,
                               std::mt19937& rng) {
    GameState result = state;
    int opponent = 1 - perspective;

    // Collect all cards that are hidden from the perspective player:
    // - opponent's hand
    // - opponent's stock pile (all except top card)
    // - draw pile
    // Use stack-allocated buffer instead of std::vector
    Card hidden[TOTAL_CARDS];
    int hidden_count = 0;

    // Opponent's hand cards
    auto& opp = result.players[opponent];
    for (int i = 0; i < opp.hand_count; ++i) {
        hidden[hidden_count++] = opp.hand[i];
    }

    // Opponent's stock pile interior (all except top)
    if (opp.stock_pile.size() > 1) {
        for (int i = 0; i < opp.stock_pile.size() - 1; ++i) {
            hidden[hidden_count++] = opp.stock_pile[i];
        }
    }

    // Draw pile
    for (int i = 0; i < result.draw_pile.size(); ++i) {
        hidden[hidden_count++] = result.draw_pile[i];
    }

    // Shuffle hidden cards
    std::shuffle(hidden, hidden + hidden_count, rng);

    // Redistribute hidden cards
    int idx = 0;

    // Opponent's hand
    for (int i = 0; i < opp.hand_count; ++i) {
        opp.hand[i] = hidden[idx++];
    }

    // Opponent's stock pile interior (preserve top)
    Card stock_top = CARD_NONE;
    if (!opp.stock_pile.empty()) {
        stock_top = opp.stock_pile.back();
    }
    if (opp.stock_pile.size() > 1) {
        for (int i = 0; i < opp.stock_pile.size() - 1; ++i) {
            opp.stock_pile[i] = hidden[idx++];
        }
    }
    if (!opp.stock_pile.empty()) {
        opp.stock_pile.back() = stock_top;
    }

    // Draw pile
    result.draw_pile.clear();
    while (idx < hidden_count) {
        result.draw_pile.push_back(hidden[idx++]);
    }

    return result;
}

} // namespace skipbo
