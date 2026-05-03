#include "tournament/tournament.h"
#include "tournament/match.h"
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <thread>
#include <vector>

namespace skipbo {

namespace {

void write_csv_header(std::ofstream& out) {
    out << "bot_a,bot_b,seed,a_was_first,winner_seat,winner_a_or_b,n_turns,stalemate,duration_ms\n";
}

void write_csv_row(std::ofstream& out, const MatchResult& r) {
    out << r.bot_a_id << ',' << r.bot_b_id << ',' << r.seed << ','
        << (r.a_was_first ? 1 : 0) << ',' << r.winner_seat << ','
        << r.winner_a_or_b << ',' << r.n_turns << ','
        << (r.stalemate ? 1 : 0) << ','
        << std::fixed << std::setprecision(3) << r.duration_ms << '\n';
}

}  // namespace

TournamentSummary run_tournament(const Bot& bot_a, const Bot& bot_b,
                                 const TournamentConfig& cfg) {
    TournamentSummary summary;
    summary.bot_a_id = bot_a.id;
    summary.bot_b_id = bot_b.id;

    int total_games = cfg.n_pairings * 2;
    int n_threads = cfg.n_threads > 0
        ? cfg.n_threads
        : static_cast<int>(std::max(1u, std::thread::hardware_concurrency()));

    // Open CSV. If appending and the file exists, skip the header.
    bool need_header = true;
    if (cfg.append_csv && std::filesystem::exists(cfg.results_csv)) {
        need_header = false;
    }
    std::ofstream csv(cfg.results_csv, cfg.append_csv ? std::ios::app : std::ios::trunc);
    if (!csv) {
        std::cerr << "Error: cannot open " << cfg.results_csv << " for writing\n";
        return summary;
    }
    if (need_header) write_csv_header(csv);

    std::mutex csv_mutex;
    std::atomic<int> next_game{0};
    std::atomic<int> games_done{0};
    std::vector<MatchResult> ordered_results(total_games);

    auto wall_start = std::chrono::steady_clock::now();

    auto worker = [&]() {
        while (true) {
            int game_idx = next_game.fetch_add(1);
            if (game_idx >= total_games) break;

            int pairing = game_idx / 2;
            bool a_was_first = (game_idx % 2 == 0);
            uint64_t seed = cfg.base_seed + static_cast<uint64_t>(pairing);

            MatchResult r = run_match(bot_a, bot_b, seed, a_was_first);
            ordered_results[game_idx] = r;

            {
                std::lock_guard<std::mutex> lk(csv_mutex);
                write_csv_row(csv, r);
                csv.flush();
            }

            int done = games_done.fetch_add(1) + 1;
            if (cfg.progress_every > 0 && (done % cfg.progress_every == 0 || done == total_games)) {
                std::lock_guard<std::mutex> lk(csv_mutex);
                std::cerr << "\r[" << bot_a.id << " vs " << bot_b.id << "] "
                          << done << "/" << total_games << " games" << std::flush;
            }
        }
    };

    std::vector<std::thread> threads;
    threads.reserve(n_threads);
    for (int t = 0; t < n_threads; ++t) threads.emplace_back(worker);
    for (auto& t : threads) t.join();

    if (cfg.progress_every > 0) std::cerr << '\n';

    // Aggregate in deterministic order so Elo updates are reproducible.
    for (const auto& r : ordered_results) {
        summary.total_games++;
        summary.wins[r.winner_a_or_b]++;
        if (r.stalemate) summary.stalemates++;
        summary.total_duration_ms += r.duration_ms;
        elo_update_winner(summary.elo_a, summary.elo_b, r.winner_a_or_b);
    }

    auto wall_end = std::chrono::steady_clock::now();
    summary.wall_seconds = std::chrono::duration<double>(wall_end - wall_start).count();

    return summary;
}

void print_summary(const TournamentSummary& s) {
    std::cout << "\n=== " << s.bot_a_id << " vs " << s.bot_b_id << " ===\n"
              << "  games:       " << s.total_games << " (stalemates: " << s.stalemates << ")\n"
              << std::fixed << std::setprecision(1)
              << "  " << std::left << std::setw(16) << s.bot_a_id
              << " wins: " << std::setw(5) << s.wins[0]
              << " (" << std::setw(5) << std::setprecision(1)
              << 100.0 * s.wins[0] / std::max(1, s.total_games) << "%)"
              << "  Elo: " << std::setprecision(1) << s.elo_a.rating << '\n'
              << "  " << std::left << std::setw(16) << s.bot_b_id
              << " wins: " << std::setw(5) << s.wins[1]
              << " (" << std::setw(5) << std::setprecision(1)
              << 100.0 * s.wins[1] / std::max(1, s.total_games) << "%)"
              << "  Elo: " << std::setprecision(1) << s.elo_b.rating << '\n'
              << "  total game-time: " << std::setprecision(1) << s.total_duration_ms << " ms"
              << "  wall: " << std::setprecision(2) << s.wall_seconds << " s"
              << "  avg/game: " << std::setprecision(2)
              << s.total_duration_ms / std::max(1, s.total_games) << " ms\n";
}

} // namespace skipbo
