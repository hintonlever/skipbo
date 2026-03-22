#include "engine/game_state.h"

namespace skipbo {

Card PlayerState::stock_top() const {
    if (stock_pile.empty()) return CARD_NONE;
    return stock_pile.back();
}

Card PlayerState::discard_top(int pile) const {
    if (discard_piles[pile].empty()) return CARD_NONE;
    return discard_piles[pile].back();
}

void PlayerState::remove_hand_card(int index) {
    for (int i = index; i < hand_count - 1; ++i) {
        hand[i] = hand[i + 1];
    }
    --hand_count;
    hand[hand_count] = CARD_NONE;
}

} // namespace skipbo
