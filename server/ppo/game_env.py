"""
Skip-Bo game environment for RL training.
Implements the core game rules matching the C++ engine.
"""
from __future__ import annotations

import random
from dataclasses import dataclass, field
from typing import Optional

# Card constants
CARD_SKIPBO = 0
CARD_MIN = 1
CARD_MAX = 12
CARD_NONE = 255

CARDS_PER_VALUE = 12
NUM_SKIPBO = 18
TOTAL_CARDS = CARDS_PER_VALUE * CARD_MAX + NUM_SKIPBO  # 162

NUM_PLAYERS = 2
HAND_SIZE = 5
NUM_DISCARD_PILES = 4
NUM_BUILDING_PILES = 4
STOCK_PILE_SIZE = 15

# Action space: 10 sources x 8 targets = 80
NUM_SOURCES = 10  # Hand0-4, StockPile, DiscardPile0-3
NUM_TARGETS = 8   # BuildingPile0-3, DiscardPile0-3
NUM_ACTIONS = NUM_SOURCES * NUM_TARGETS  # 80


def action_to_source_target(action: int) -> tuple[int, int]:
    """Convert flat action index to (source, target)."""
    return action // NUM_TARGETS, action % NUM_TARGETS


def source_target_to_action(source: int, target: int) -> int:
    """Convert (source, target) to flat action index."""
    return source * NUM_TARGETS + target


def is_discard_action(action: int) -> bool:
    """A discard targets DiscardPile0-3 (target indices 4-7)."""
    _, target = action_to_source_target(action)
    return target >= 4


@dataclass
class PlayerState:
    stock_pile: list[int] = field(default_factory=list)
    hand: list[int] = field(default_factory=lambda: [CARD_NONE] * HAND_SIZE)
    hand_count: int = 0
    discard_piles: list[list[int]] = field(
        default_factory=lambda: [[] for _ in range(NUM_DISCARD_PILES)]
    )

    def stock_top(self) -> int:
        return self.stock_pile[-1] if self.stock_pile else CARD_NONE

    def stock_empty(self) -> bool:
        return len(self.stock_pile) == 0

    def stock_size(self) -> int:
        return len(self.stock_pile)

    def discard_top(self, pile: int) -> int:
        return self.discard_piles[pile][-1] if self.discard_piles[pile] else CARD_NONE

    def discard_empty(self, pile: int) -> bool:
        return len(self.discard_piles[pile]) == 0

    def remove_hand_card(self, index: int):
        """Remove card at index, shift remaining left."""
        for i in range(index, self.hand_count - 1):
            self.hand[i] = self.hand[i + 1]
        self.hand_count -= 1
        self.hand[self.hand_count] = CARD_NONE


class SkipBoEnv:
    """Skip-Bo environment for 2 players."""

    def __init__(self, seed: Optional[int] = None):
        self.rng = random.Random(seed)
        self.players: list[PlayerState] = [PlayerState(), PlayerState()]
        self.building_pile_count: list[int] = [0] * NUM_BUILDING_PILES
        self.draw_pile: list[int] = []
        self.current_player: int = 0
        self.game_over: bool = False
        self.winner: int = -1
        self._consecutive_passes: int = 0
        self._total_moves: int = 0

    def reset(self, seed: Optional[int] = None) -> None:
        if seed is not None:
            self.rng = random.Random(seed)

        # Build and shuffle deck
        deck: list[int] = []
        for v in range(CARD_MIN, CARD_MAX + 1):
            deck.extend([v] * CARDS_PER_VALUE)
        deck.extend([CARD_SKIPBO] * NUM_SKIPBO)
        self.rng.shuffle(deck)

        idx = 0
        self.players = [PlayerState(), PlayerState()]
        for p in range(NUM_PLAYERS):
            self.players[p].stock_pile = deck[idx : idx + STOCK_PILE_SIZE]
            idx += STOCK_PILE_SIZE
            self.players[p].hand = [CARD_NONE] * HAND_SIZE
            self.players[p].hand_count = 0

        self.draw_pile = deck[idx:]
        self.building_pile_count = [0] * NUM_BUILDING_PILES
        self.current_player = 0
        self.game_over = False
        self.winner = -1
        self._consecutive_passes = 0
        self._total_moves = 0

        self._start_turn()

    def _start_turn(self):
        player = self.players[self.current_player]
        need = HAND_SIZE - player.hand_count
        if need > 0:
            self._draw_cards(self.current_player, need)

    def _draw_cards(self, player_id: int, count: int):
        player = self.players[player_id]
        for _ in range(count):
            if not self.draw_pile:
                break
            card = self.draw_pile.pop()
            player.hand[player.hand_count] = card
            player.hand_count += 1

    def can_play_on_building(self, card: int, pile: int) -> bool:
        cnt = self.building_pile_count[pile]
        if cnt >= CARD_MAX:
            return False
        if card == CARD_SKIPBO:
            return True
        return card == cnt + 1

    def building_pile_needs(self, pile: int) -> int:
        return self.building_pile_count[pile] + 1

    def get_legal_actions(self) -> list[int]:
        """Return list of legal action indices for current player."""
        if self.game_over:
            return []

        player = self.players[self.current_player]
        actions: list[int] = []

        # Hand cards -> building piles or discard piles
        for h in range(player.hand_count):
            card = player.hand[h]
            if card == CARD_NONE:
                continue
            source = h  # Hand0-4
            # Building piles
            for b in range(NUM_BUILDING_PILES):
                if self.can_play_on_building(card, b):
                    actions.append(source_target_to_action(source, b))
            # Discard piles
            for d in range(NUM_DISCARD_PILES):
                actions.append(source_target_to_action(source, 4 + d))

        # Stock pile top -> building piles only
        if not player.stock_empty():
            card = player.stock_top()
            source = 5  # StockPile
            for b in range(NUM_BUILDING_PILES):
                if self.can_play_on_building(card, b):
                    actions.append(source_target_to_action(source, b))

        # Discard pile tops -> building piles only
        for d in range(NUM_DISCARD_PILES):
            if not player.discard_empty(d):
                card = player.discard_top(d)
                source = 6 + d  # DiscardPile0-3
                for b in range(NUM_BUILDING_PILES):
                    if self.can_play_on_building(card, b):
                        actions.append(source_target_to_action(source, b))

        return actions

    def get_legal_mask(self) -> list[bool]:
        """Return boolean mask of size NUM_ACTIONS."""
        mask = [False] * NUM_ACTIONS
        for a in self.get_legal_actions():
            mask[a] = True
        return mask

    def step(self, action: int) -> tuple[bool, int]:
        """
        Apply an action. Returns (game_over, winner).
        A discard action ends the turn and switches players.
        """
        assert not self.game_over
        self._total_moves += 1

        source, target = action_to_source_target(action)
        player = self.players[self.current_player]
        acting_player = self.current_player

        # Get the card being played
        card = CARD_NONE
        if source < 5:  # Hand
            card = player.hand[source]
        elif source == 5:  # Stock
            card = player.stock_top()
        else:  # Discard pile
            d = source - 6
            card = player.discard_top(d)

        assert card != CARD_NONE

        is_discard = target >= 4

        if is_discard:
            # Hand -> discard pile
            d = target - 4
            player.discard_piles[d].append(card)
            player.remove_hand_card(source)
        else:
            # Play onto building pile
            b = target
            self.building_pile_count[b] += 1

            # Remove from source
            if source < 5:
                player.remove_hand_card(source)
            elif source == 5:
                player.stock_pile.pop()
            else:
                d = source - 6
                player.discard_piles[d].pop()

        # Check building pile completion
        if not is_discard:
            b = target
            if self.building_pile_count[b] >= CARD_MAX:
                # Recycle: cards go back to draw pile
                for c in range(CARD_MIN, CARD_MAX + 1):
                    self.draw_pile.append(c)
                self.rng.shuffle(self.draw_pile)
                self.building_pile_count[b] = 0

        # Check win
        if player.stock_empty():
            self.game_over = True
            self.winner = acting_player
            return True, self.winner

        # Handle turn end (discard)
        if is_discard:
            self._consecutive_passes = 0
            self.current_player = 1 - self.current_player
            self._start_turn()

            # Stalemate check
            if self._total_moves > 5000:
                self.game_over = True
                s0 = self.players[0].stock_size()
                s1 = self.players[1].stock_size()
                self.winner = 0 if s0 <= s1 else 1
                return True, self.winner
        else:
            # If hand empty after build, draw 5 more
            if player.hand_count == 0:
                self._draw_cards(acting_player, HAND_SIZE)

        return self.game_over, self.winner

    def pass_turn(self):
        """Pass when no legal moves (shouldn't happen normally since discard is always legal)."""
        self._consecutive_passes += 1
        self.current_player = 1 - self.current_player
        self._start_turn()

        if self._consecutive_passes >= 4:
            self.game_over = True
            s0 = self.players[0].stock_size()
            s1 = self.players[1].stock_size()
            self.winner = 0 if s0 <= s1 else 1
