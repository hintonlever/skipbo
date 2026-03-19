#pragma once

#include "engine/card.h"
#include <array>
#include <vector>
#include <cstdint>

namespace skipbo {

constexpr int NUM_PLAYERS = 2;
constexpr int HAND_SIZE = 5;
constexpr int NUM_DISCARD_PILES = 4;
constexpr int NUM_BUILDING_PILES = 4;
constexpr int STOCK_PILE_SIZE = 30;

struct PlayerState {
    std::vector<Card> stock_pile;    // top card is back()
    std::array<Card, HAND_SIZE> hand;
    uint8_t hand_count = 0;          // number of cards in hand (packed left)
    std::array<std::vector<Card>, NUM_DISCARD_PILES> discard_piles;

    Card stock_top() const;
    bool stock_empty() const { return stock_pile.empty(); }
    int stock_size() const { return static_cast<int>(stock_pile.size()); }
    Card discard_top(int pile) const;
    bool discard_empty(int pile) const { return discard_piles[pile].empty(); }

    void remove_hand_card(int index);
};

struct GameState {
    std::array<PlayerState, NUM_PLAYERS> players;
    std::array<std::vector<Card>, NUM_BUILDING_PILES> building_piles;
    std::vector<Card> draw_pile;
    uint8_t current_player = 0;
    bool game_over = false;
    int8_t winner = -1;

    // Get the value needed next on a building pile (1 if empty, top+1 otherwise)
    Card building_pile_needs(int pile) const;

    // Check if a card can be played on a building pile
    bool can_play_on_building(Card card, int pile) const;
};

} // namespace skipbo
