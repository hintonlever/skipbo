#include "engine/game.h"
#include "engine/rules.h"
#include <algorithm>
#include <cassert>

namespace skipbo {

Game::Game(uint64_t seed) : rng_(seed) {}

void Game::setup() {
    // Create and shuffle deck
    auto full_cards = std::vector<Card>();
    full_cards.reserve(TOTAL_CARDS);
    for (Card v = CARD_MIN; v <= CARD_MAX; ++v) {
        for (int i = 0; i < CARDS_PER_VALUE; ++i) {
            full_cards.push_back(v);
        }
    }
    for (int i = 0; i < NUM_SKIPBO; ++i) {
        full_cards.push_back(CARD_SKIPBO);
    }
    std::shuffle(full_cards.begin(), full_cards.end(), rng_);

    // Deal stock piles (30 cards each)
    int idx = 0;
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
    state_.draw_pile.assign(full_cards.begin() + idx, full_cards.end());

    // Clear building piles
    for (auto& bp : state_.building_piles) bp.clear();

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
    auto& bp = state_.building_piles[pile];
    if (!bp.empty() && bp.size() >= static_cast<size_t>(CARD_MAX)) {
        // Building pile is complete (reached 12). Recycle into draw pile.
        state_.draw_pile.insert(state_.draw_pile.end(), bp.begin(), bp.end());
        std::shuffle(state_.draw_pile.begin(), state_.draw_pile.end(), rng_);
        bp.clear();
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
        // Play onto a building pile
        int bi = move.target_building_index();
        Card effective = card;
        if (is_skipbo(card)) {
            effective = state.building_pile_needs(bi);
        }
        state.building_piles[bi].push_back(effective);

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

bool Game::apply_move_to_state(GameState& state, const Move& move) {
    uint8_t acting_player = state.current_player;
    if (!move_card(state, move)) return false;

    // Check win
    if (state.players[acting_player].stock_empty()) {
        state.game_over = true;
        state.winner = acting_player;
        return true;
    }

    // Building pile completion (simplified: just clear, no recycling)
    if (!move.is_discard()) {
        int bi = move.target_building_index();
        auto& bp = state.building_piles[bi];
        if (bp.size() >= static_cast<size_t>(CARD_MAX)) {
            bp.clear();
        }
    }

    // If discard, switch player
    if (move.is_discard()) {
        state.current_player = 1 - state.current_player;
    }

    return true;
}

} // namespace skipbo
