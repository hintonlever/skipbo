#include "engine/game.h"
#include "engine/rules.h"
#include <algorithm>
#include <cassert>

namespace skipbo {

Game::Game(uint64_t seed) : rng_(seed) {}

void Game::setup() {
    // Create and shuffle deck
    Card full_cards[TOTAL_CARDS];
    int idx = 0;
    for (Card v = CARD_MIN; v <= CARD_MAX; ++v) {
        for (int i = 0; i < CARDS_PER_VALUE; ++i) {
            full_cards[idx++] = v;
        }
    }
    for (int i = 0; i < NUM_SKIPBO; ++i) {
        full_cards[idx++] = CARD_SKIPBO;
    }
    std::shuffle(full_cards, full_cards + TOTAL_CARDS, rng_);

    // Deal stock piles (30 cards each)
    idx = 0;
    for (int p = 0; p < NUM_PLAYERS; ++p) {
        state_.players[p].stock_pile.resize(STOCK_PILE_SIZE);
        for (int i = 0; i < STOCK_PILE_SIZE; ++i) {
            state_.players[p].stock_pile[i] = full_cards[idx++];
        }
        state_.players[p].hand = {CARD_NONE, CARD_NONE, CARD_NONE, CARD_NONE, CARD_NONE};
        state_.players[p].hand_count = 0;
        for (auto& dp : state_.players[p].discard_piles) dp.clear();
    }

    // Remaining cards go to draw pile
    state_.draw_pile.clear();
    for (int i = idx; i < TOTAL_CARDS; ++i) {
        state_.draw_pile.push_back(full_cards[i]);
    }

    // Clear building piles
    state_.building_pile_count = {};

    state_.current_player = 0;
    state_.game_over = false;
    state_.winner = -1;

    // Draw initial hands
    start_turn();
}

void Game::start_turn() {
    auto& player = state_.players[state_.current_player];
    int need = HAND_SIZE - player.hand_count;
    if (need > 0) {
        draw_cards_for_player(state_.current_player, need);
    }
}

void Game::draw_cards_for_player(int player_id, int count) {
    auto& player = state_.players[player_id];
    for (int i = 0; i < count; ++i) {
        if (state_.draw_pile.empty()) break;
        Card c = state_.draw_pile.back();
        state_.draw_pile.pop_back();
        player.hand[player.hand_count++] = c;
    }
}

void Game::pass_turn() {
    state_.current_player = 1 - state_.current_player;
    start_turn();
}

void Game::check_building_pile_completion(int pile) {
    if (state_.building_pile_count[pile] >= CARD_MAX) {
        // Building pile is complete (reached 12). Recycle cards 1-12 into draw pile.
        for (Card c = CARD_MIN; c <= CARD_MAX; ++c) {
            state_.draw_pile.push_back(c);
        }
        std::shuffle(state_.draw_pile.begin(), state_.draw_pile.end(), rng_);
        state_.building_pile_count[pile] = 0;
    }
}

// Move the card from source to target. No side effects (no player switch,
// no game_over check, no building pile completion).
static bool move_card(GameState& state, const Move& move) {
    auto& player = state.players[state.current_player];

    // Get the card being played
    Card card = CARD_NONE;
    if (move.is_from_hand()) {
        int hi = move.hand_index();
        if (hi >= player.hand_count) return false;
        card = player.hand[hi];
    } else if (move.is_from_stock()) {
        card = player.stock_top();
    } else if (move.is_from_discard()) {
        int di = move.source_discard_index();
        card = player.discard_top(di);
    }

    if (card == CARD_NONE) return false;

    // Place the card on the target
    if (move.is_discard()) {
        // Discard from hand to discard pile
        int di = move.target_discard_index();
        player.discard_piles[di].push_back(card);
        player.remove_hand_card(move.hand_index());
    } else {
        // Play onto a building pile — just increment count
        int bi = move.target_building_index();
        state.building_pile_count[bi]++;

        // Remove from source
        if (move.is_from_hand()) {
            player.remove_hand_card(move.hand_index());
        } else if (move.is_from_stock()) {
            player.stock_pile.pop_back();
        } else if (move.is_from_discard()) {
            int di = move.source_discard_index();
            player.discard_piles[di].pop_back();
        }
    }

    return true;
}

bool Game::apply_move(const Move& move) {
    if (!is_legal_move(state_, move)) return false;

    uint8_t acting_player = state_.current_player;
    move_card(state_, move);

    // Check building pile completion
    if (!move.is_discard()) {
        int bpile = move.target_building_index();
        check_building_pile_completion(bpile);
    }

    // Check win condition: acting player's stock pile empty
    if (state_.players[acting_player].stock_empty()) {
        state_.game_over = true;
        state_.winner = acting_player;
        return true;
    }

    // If discard, end turn and switch player
    if (move.is_discard()) {
        state_.current_player = 1 - state_.current_player;
        start_turn();
        return true;
    }

    // If all hand cards played, draw 5 more
    if (state_.players[acting_player].hand_count == 0) {
        draw_cards_for_player(acting_player, HAND_SIZE);
    }

    return true;
}

// Draw cards from draw pile into a player's hand (for simulation path).
static void draw_cards_from_pile(GameState& state, int player_id, int count) {
    auto& player = state.players[player_id];
    for (int i = 0; i < count; ++i) {
        if (state.draw_pile.empty()) break;
        player.hand[player.hand_count++] = state.draw_pile.back();
        state.draw_pile.pop_back();
    }
}

bool Game::apply_move_to_state(GameState& state, const Move& move,
                                std::mt19937* rng) {
    uint8_t acting_player = state.current_player;
    if (!move_card(state, move)) return false;

    // Check win
    if (state.players[acting_player].stock_empty()) {
        state.game_over = true;
        state.winner = acting_player;
        return true;
    }

    // Building pile completion — recycle into draw pile
    if (!move.is_discard()) {
        int bi = move.target_building_index();
        if (state.building_pile_count[bi] >= CARD_MAX) {
            for (Card c = CARD_MIN; c <= CARD_MAX; ++c) {
                state.draw_pile.push_back(c);
            }
            if (rng) {
                std::shuffle(state.draw_pile.begin(), state.draw_pile.end(), *rng);
            }
            state.building_pile_count[bi] = 0;
        }
    }

    // If all hand cards played, draw 5 more (same turn continues)
    if (!move.is_discard() && state.players[acting_player].hand_count == 0) {
        draw_cards_from_pile(state, acting_player, HAND_SIZE);
    }

    // If discard, switch player and draw cards for new player
    if (move.is_discard()) {
        state.current_player = 1 - state.current_player;
        int need = HAND_SIZE - state.players[state.current_player].hand_count;
        if (need > 0) {
            draw_cards_from_pile(state, state.current_player, need);
        }
    }

    return true;
}

} // namespace skipbo
