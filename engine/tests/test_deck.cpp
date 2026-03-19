#include <catch2/catch_test_macros.hpp>
#include "engine/deck.h"
#include <algorithm>
#include <map>

using namespace skipbo;

TEST_CASE("Full deck has 162 cards", "[deck]") {
    auto deck = Deck::create_full();
    REQUIRE(deck.remaining() == 162);
}

TEST_CASE("Full deck has correct distribution", "[deck]") {
    auto deck = Deck::create_full();
    std::map<Card, int> counts;
    while (!deck.empty()) {
        counts[deck.draw()]++;
    }
    // 12 copies of each numbered card
    for (Card v = CARD_MIN; v <= CARD_MAX; ++v) {
        REQUIRE(counts[v] == CARDS_PER_VALUE);
    }
    // 18 SkipBo wilds
    REQUIRE(counts[CARD_SKIPBO] == NUM_SKIPBO);
}

TEST_CASE("Shuffle produces different orderings", "[deck]") {
    auto d1 = Deck::create_full();
    auto d2 = Deck::create_full();
    std::mt19937 rng1(42);
    std::mt19937 rng2(99);
    d1.shuffle(rng1);
    d2.shuffle(rng2);

    // Very unlikely to be identical with different seeds
    bool all_same = true;
    auto& c1 = d1.cards();
    auto& c2 = d2.cards();
    for (size_t i = 0; i < c1.size(); ++i) {
        if (c1[i] != c2[i]) { all_same = false; break; }
    }
    REQUIRE_FALSE(all_same);
}

TEST_CASE("Draw depletes deck", "[deck]") {
    auto deck = Deck::create_full();
    auto cards = deck.draw_n(162);
    REQUIRE(cards.size() == 162);
    REQUIRE(deck.empty());
}
