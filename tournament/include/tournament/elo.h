#pragma once

namespace skipbo {

constexpr double ELO_ANCHOR_RATING = 1000.0;  // random baseline anchored here
constexpr double ELO_K_FACTOR = 16.0;

struct EloRating {
    double rating = ELO_ANCHOR_RATING;
    int games_played = 0;
};

double elo_expected(double rating_a, double rating_b);

// Update both ratings after one game. score_a in [0, 1]: 1=a wins, 0=b wins.
void elo_update(EloRating& a, EloRating& b, double score_a);

// Convenience: pass winner index (0 or 1).
void elo_update_winner(EloRating& a, EloRating& b, int winner);

// One-sided update: only `r` moves; `anchor_rating` is treated as fixed.
void elo_update_against_anchor(EloRating& r, double anchor_rating, double score);

} // namespace skipbo
