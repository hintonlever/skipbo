#pragma once

#include "engine/card.h"
#include <vector>
#include <random>
#include <cassert>

namespace skipbo {

class Deck {
public:
    // Create a full 162-card deck
    static Deck create_full();

    void shuffle(std::mt19937& rng);
    Card draw();
    std::vector<Card> draw_n(int n);
    bool empty() const { return cards_.empty(); }
    size_t remaining() const { return cards_.size(); }

    // Recycle completed building pile cards back into the deck
    void recycle(const std::vector<Card>& cards, std::mt19937& rng);

    const std::vector<Card>& cards() const { return cards_; }

private:
    std::vector<Card> cards_;
};

} // namespace skipbo
