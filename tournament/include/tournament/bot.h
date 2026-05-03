#pragma once

#include "ai/player.h"
#include <functional>
#include <memory>
#include <string>
#include <cstdint>

namespace skipbo {

// A bot is identified by `id` and constructed per-match via `factory`.
// The factory takes a deterministic player_seed so bot decisions are reproducible.
struct Bot {
    std::string id;
    std::string family;  // "random", "heuristic", "pimc", "ismcts", ...
    std::function<std::unique_ptr<Player>(uint64_t player_seed)> factory;
};

// Derive a bot's per-match RNG seed from the match seed and a salt.
// salt = 0 for the player at seat 0, salt = 1 for seat 1.
inline uint64_t derive_player_seed(uint64_t match_seed, uint32_t salt) {
    // SplitMix64 mix for low-correlation per-bot streams.
    uint64_t z = match_seed + 0x9E3779B97F4A7C15ULL * (salt + 1);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

} // namespace skipbo
