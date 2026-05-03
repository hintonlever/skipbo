"""
State encoding for PPO, matching the C++ nn_encoding.cpp format (158 floats).
"""
from __future__ import annotations

import numpy as np

from .game_env import (
    CARD_MAX,
    CARD_NONE,
    CARD_SKIPBO,
    HAND_SIZE,
    NUM_BUILDING_PILES,
    NUM_DISCARD_PILES,
    TOTAL_CARDS,
    PlayerState,
    SkipBoEnv,
)

STATE_SIZE = 158


def _write_card_one_hot(out: np.ndarray, offset: int, card: int):
    """One-hot encode a card. SkipBo=0, 1-12 numbered, CARD_NONE=all zeros."""
    if card != CARD_NONE and card <= CARD_MAX:
        out[offset + card] = 1.0


def _encode_player(ps: PlayerState, out: np.ndarray, offset: int) -> int:
    """Encode a single player's non-hand features. Returns new offset."""
    # Stock top one-hot (13)
    st = ps.stock_top() if not ps.stock_empty() else CARD_NONE
    _write_card_one_hot(out, offset, st)
    offset += 13

    # Stock size normalized
    out[offset] = ps.stock_size() / 15.0
    offset += 1

    # 4 discard pile tops one-hot (4 x 13 = 52)
    for d in range(NUM_DISCARD_PILES):
        dt = ps.discard_top(d) if not ps.discard_empty(d) else CARD_NONE
        _write_card_one_hot(out, offset, dt)
        offset += 13

    # 4 discard pile sizes normalized
    for d in range(NUM_DISCARD_PILES):
        out[offset] = len(ps.discard_piles[d]) / 20.0
        offset += 1

    return offset


def encode_state(env: SkipBoEnv, perspective: int) -> np.ndarray:
    """Encode game state from perspective player's viewpoint. Returns 158 floats."""
    out = np.zeros(STATE_SIZE, dtype=np.float32)

    me = env.players[perspective]
    opp = env.players[1 - perspective]

    offset = 0

    # [0..12] Hand histogram
    for i in range(me.hand_count):
        c = me.hand[i]
        if c != CARD_NONE and c <= CARD_MAX:
            out[c] += 1.0
    offset += 13

    # [13..82] My player features (70 floats)
    offset = _encode_player(me, out, offset)

    # [83..152] Opponent player features (70 floats)
    offset = _encode_player(opp, out, offset)

    # [153..156] Building pile next-needed / 12
    for b in range(NUM_BUILDING_PILES):
        out[offset] = env.building_pile_needs(b) / 12.0
        offset += 1

    # [157] Draw pile size / 162
    out[offset] = len(env.draw_pile) / float(TOTAL_CARDS)

    return out
