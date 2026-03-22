#pragma once

#include <cstdint>
#include <string>
#include <array>

namespace skipbo {

// Card values: 0 = SkipBo wild, 1-12 = numbered cards
// Stored as uint8_t for compact GameState
using Card = uint8_t;

constexpr Card CARD_SKIPBO = 0;
constexpr Card CARD_MIN = 1;
constexpr Card CARD_MAX = 12;
constexpr Card CARD_NONE = 255; // sentinel for empty slots

constexpr int CARDS_PER_VALUE = 12;  // 12 copies of each numbered card
constexpr int NUM_SKIPBO = 18;       // 18 Skip-Bo wilds
constexpr int TOTAL_CARDS = CARDS_PER_VALUE * CARD_MAX + NUM_SKIPBO; // 162

inline bool is_skipbo(Card c) { return c == CARD_SKIPBO; }
inline bool is_numbered(Card c) { return c >= CARD_MIN && c <= CARD_MAX; }
inline bool is_valid_card(Card c) { return c == CARD_SKIPBO || is_numbered(c); }

std::string card_to_string(Card c);

// Fixed-capacity stack of cards — zero heap allocations, trivially copyable.
// Used in GameState to eliminate std::vector overhead on copy.
template<int Capacity>
struct CardStack {
    Card data[Capacity];
    uint8_t count = 0;

    CardStack() = default;
    CardStack(std::initializer_list<Card> init) : count(0) {
        for (Card c : init) data[count++] = c;
    }

    bool empty() const { return count == 0; }
    int size() const { return count; }
    Card back() const { return data[count - 1]; }
    Card& back() { return data[count - 1]; }
    Card operator[](int i) const { return data[i]; }
    Card& operator[](int i) { return data[i]; }

    void push_back(Card c) { data[count++] = c; }
    void pop_back() { --count; }
    void clear() { count = 0; }
    void resize(int n) { count = static_cast<uint8_t>(n); }

    Card* begin() { return data; }
    Card* end() { return data + count; }
    const Card* begin() const { return data; }
    const Card* end() const { return data + count; }

    bool operator==(const CardStack& o) const {
        if (count != o.count) return false;
        for (uint8_t i = 0; i < count; ++i)
            if (data[i] != o.data[i]) return false;
        return true;
    }
    bool operator!=(const CardStack& o) const { return !(*this == o); }
};

} // namespace skipbo
