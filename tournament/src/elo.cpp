#include "tournament/elo.h"
#include <cmath>

namespace skipbo {

double elo_expected(double rating_a, double rating_b) {
    return 1.0 / (1.0 + std::pow(10.0, (rating_b - rating_a) / 400.0));
}

void elo_update(EloRating& a, EloRating& b, double score_a) {
    double expected_a = elo_expected(a.rating, b.rating);
    a.rating += ELO_K_FACTOR * (score_a - expected_a);
    b.rating += ELO_K_FACTOR * ((1.0 - score_a) - (1.0 - expected_a));
    a.games_played++;
    b.games_played++;
}

void elo_update_winner(EloRating& a, EloRating& b, int winner) {
    elo_update(a, b, winner == 0 ? 1.0 : 0.0);
}

void elo_update_against_anchor(EloRating& r, double anchor_rating, double score) {
    double expected = elo_expected(r.rating, anchor_rating);
    r.rating += ELO_K_FACTOR * (score - expected);
    r.games_played++;
}

} // namespace skipbo
