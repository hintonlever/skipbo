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

} // namespace skipbo
