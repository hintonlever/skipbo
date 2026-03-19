#pragma once

namespace skipbo {

struct EloRating {
    double rating = 1500.0;
    int games_played = 0;

    double k_factor() const {
        return games_played < 30 ? 40.0 : 20.0;
    }
};

// Expected score for player a against player b
double elo_expected(double rating_a, double rating_b);

// Update ratings after a game. winner = 0 means a wins, 1 means b wins.
void elo_update(EloRating& a, EloRating& b, int winner);

} // namespace skipbo
