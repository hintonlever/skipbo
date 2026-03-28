#pragma once

#include "engine/game_state.h"
#include "ai/turn_action.h"

namespace skipbo {

// Dimensions of encoded vectors
constexpr int STATE_ENCODING_SIZE = 158;
constexpr int CHAIN_ENCODING_SIZE = 29;

// Encode observable game state into 158 floats from the perspective player's POV.
// Layout:
//   [0..12]    Hand histogram (count of each card type 0-12)
//   [13..25]   My stock top one-hot (13 card types, all zero if empty)
//   [26]       My stock size / 15
//   [27..78]   My 4 discard tops one-hot (4 x 13)
//   [79..82]   My 4 discard sizes / 20
//   [83..95]   Opponent stock top one-hot
//   [96]       Opponent stock size / 15
//   [97..148]  Opponent 4 discard tops one-hot (4 x 13)
//   [149..152] Opponent 4 discard sizes / 20
//   [153..156] Building pile next-needed / 12
//   [157]      Draw pile size / 162
void encode_state(const GameState& state, int perspective, float* out);

// Encode a TurnAction (chain) relative to the pre-action state into 29 floats.
// Layout:
//   [0]        Num builds / 12
//   [1..4]     Building pile deltas (count change per pile) / 12
//   [5]        Stock card played (binary)
//   [6]        Hand cards used / 5
//   [7..10]    Which discard pile tops were used as source (binary)
//   [11..14]   Discard target one-hot (4 piles)
//   [15]       Hand empty after builds, no discard (binary)
//   [16..28]   Build card histogram (13 card types played)
void encode_chain(const GameState& state, const TurnAction& action, float* out);

} // namespace skipbo
