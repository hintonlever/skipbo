#pragma once

#include "engine/card.h"
#include <array>
#include <cstdint>

namespace skipbo {

constexpr int NUM_PLAYERS = 2;
constexpr int HAND_SIZE = 5;
constexpr int NUM_DISCARD_PILES = 4;
constexpr int NUM_BUILDING_PILES = 4;
constexpr int STOCK_PILE_SIZE = 15;

struct PlayerState {
    CardStack<STOCK_PILE_SIZE> stock_pile;
    std::array<Card, HAND_SIZE> hand{CARD_NONE, CARD_NONE, CARD_NONE, CARD_NONE, CARD_NONE};
    uint8_t hand_count = 0;
    std::array<CardStack<TOTAL_CARDS>, NUM_DISCARD_PILES> discard_piles;

    Card stock_top() const;
    bool stock_empty() const { return stock_pile.empty(); }
    int stock_size() const { return stock_pile.size(); }
    Card discard_top(int pile) const;
    bool discard_empty(int pile) const { return discard_piles[pile].empty(); }

    void remove_hand_card(int index);
};

struct GameState {
    std::array<PlayerState, NUM_PLAYERS> players;
    // Building piles store only their count (0-12). Cards are always 1..count.
    // This avoids storing/copying actual card values since they're deterministic.
    std::array<uint8_t, NUM_BUILDING_PILES> building_pile_count{};
    CardStack<TOTAL_CARDS> draw_pile;
    uint8_t current_player = 0;
    bool game_over = false;
    int8_t winner = -1;

    // Get the value needed next on a building pile (1 if empty, count+1 otherwise)
    Card building_pile_needs(int pile) const {
        return static_cast<Card>(building_pile_count[pile] + 1);
    }

    // Check if a card can be played on a building pile
    bool can_play_on_building(Card card, int pile) const {
        uint8_t cnt = building_pile_count[pile];
        if (cnt >= CARD_MAX) return false; // pile is full (shouldn't happen, recycled at 12)
        if (is_skipbo(card)) return true;
        return card == cnt + 1;
    }
};

} // namespace skipbo
