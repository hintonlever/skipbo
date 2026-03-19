#include "engine/deck.h"
#include <algorithm>

namespace skipbo {

Deck Deck::create_full() {
    Deck d;
    d.cards_.reserve(TOTAL_CARDS);
    for (Card v = CARD_MIN; v <= CARD_MAX; ++v) {
        for (int i = 0; i < CARDS_PER_VALUE; ++i) {
            d.cards_.push_back(v);
        }
    }
    for (int i = 0; i < NUM_SKIPBO; ++i) {
        d.cards_.push_back(CARD_SKIPBO);
    }
    return d;
}

void Deck::shuffle(std::mt19937& rng) {
    std::shuffle(cards_.begin(), cards_.end(), rng);
}

Card Deck::draw() {
    assert(!cards_.empty());
    Card c = cards_.back();
    cards_.pop_back();
    return c;
}

std::vector<Card> Deck::draw_n(int n) {
    std::vector<Card> result;
    result.reserve(n);
    for (int i = 0; i < n && !cards_.empty(); ++i) {
        result.push_back(draw());
    }
    return result;
}

void Deck::recycle(const std::vector<Card>& cards, std::mt19937& rng) {
    cards_.insert(cards_.end(), cards.begin(), cards.end());
    std::shuffle(cards_.begin(), cards_.end(), rng);
}

} // namespace skipbo
