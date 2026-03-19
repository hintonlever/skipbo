#include "tournament/elo.h"
#include <cmath>

namespace skipbo {

double elo_expected(double rating_a, double rating_b) {
    return 1.0 / (1.0 + std::pow(10.0, (rating_b - rating_a) / 400.0));
}

void elo_update(EloRating& a, EloRating& b, int winner) {
    double expected_a = elo_expected(a.rating, b.rating);
    double score_a = (winner == 0) ? 1.0 : 0.0;

    a.rating += a.k_factor() * (score_a - expected_a);
    b.rating += b.k_factor() * ((1.0 - score_a) - (1.0 - expected_a));

    a.games_played++;
    b.games_played++;
}

} // namespace skipbo
