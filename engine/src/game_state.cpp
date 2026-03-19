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
    // Shift remaining cards left to fill the gap
    for (int i = index; i < hand_count - 1; ++i) {
        hand[i] = hand[i + 1];
    }
    --hand_count;
    hand[hand_count] = CARD_NONE;
}

Card GameState::building_pile_needs(int pile) const {
    if (building_piles[pile].empty()) return CARD_MIN; // needs a 1
    Card top = building_piles[pile].back();
    return top + 1; // needs the next number
}

bool GameState::can_play_on_building(Card card, int pile) const {
    if (is_skipbo(card)) return true; // wild can play on any building pile
    return card == building_pile_needs(pile);
}

} // namespace skipbo
