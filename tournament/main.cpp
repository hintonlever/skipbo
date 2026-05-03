#include "ai/pimc_player.h"
#include "ai/random_player.h"
#include "ai/rules_player.h"
#include "tournament/bot.h"
#include "tournament/tournament.h"
#include <cstdlib>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <string>

using namespace skipbo;

namespace {

Bot make_random_bot(const std::string& id = "random") {
    return Bot{
        id, "random",
        [](uint64_t player_seed) -> std::unique_ptr<Player> {
            return std::make_unique<RandomPlayer>(player_seed);
        },
    };
}

Bot make_rules_bot(const std::string& id, RulesConfig config) {
    return Bot{
        id, "rules",
        [config, id](uint64_t player_seed) -> std::unique_ptr<Player> {
            return std::make_unique<RulesPlayer>(player_seed, config, id);
        },
    };
}

Bot make_pimc_bot(const std::string& id, PIMCConfig config) {
    return Bot{
        id, "pimc",
        [config, id](uint64_t player_seed) -> std::unique_ptr<Player> {
            return std::make_unique<PIMCPlayer>(player_seed, config, id);
        },
    };
}

void print_help() {
    std::cout <<
        "Usage: skipbo_tournament [options]\n"
        "  --matchup A:B       run a single matchup (A and B are bot ids)\n"
        "                      available ids: random, rules_full,\n"
        "                                     rules_no_play_stock, rules_no_avoid_burying,\n"
        "                                     rules_no_short_discard, rules_no_block_opponent,\n"
        "                                     rules_no_save_wildcards,\n"
        "                                     pimc_fast (3w x 100s), pimc_default (10w x 500s), pimc_deep (20w x 2000s)\n"
        "                      omit to run the default suite (random/rules + small pimc smoke)\n"
        "  --pairings N        paired-seed pairings (each = 2 games) [default: 100]\n"
        "  --threads N         worker threads (0 = hardware) [default: 0]\n"
        "  --seed N            base seed [default: 12345]\n"
        "  --csv PATH          results CSV path [default: results.csv]\n"
        "  --append            append to CSV instead of overwriting (also appended across matchups in the default suite)\n"
        "  --progress N        print progress every N games (0 = silent) [default: 50]\n"
        "  --determinism-check rerun the same matchup twice and verify identical aggregates\n"
        "  --help              show this help\n";
}

std::map<std::string, Bot> build_registry() {
    std::map<std::string, Bot> reg;
    reg.emplace("random", make_random_bot("random"));

    RulesConfig full;
    reg.emplace("rules_full", make_rules_bot("rules_full", full));

    auto ablate = [&](const std::string& id, std::function<void(RulesConfig&)> mutate) {
        RulesConfig c = full;
        mutate(c);
        reg.emplace(id, make_rules_bot(id, c));
    };
    ablate("rules_no_play_stock",      [](RulesConfig& c){ c.enable_play_stock = false; });
    ablate("rules_no_avoid_burying",   [](RulesConfig& c){ c.enable_avoid_burying = false; });
    ablate("rules_no_short_discard",   [](RulesConfig& c){ c.enable_short_discard = false; });
    ablate("rules_no_block_opponent",  [](RulesConfig& c){ c.enable_block_opponent = false; });
    ablate("rules_no_save_wildcards",  [](RulesConfig& c){ c.enable_save_wildcards = false; });

    // PIMC variants. Tuning is for step-7 work; these are starter budgets.
    {
        PIMCConfig fast;
        fast.n_worlds = 3;
        fast.n_simulations = 100;
        reg.emplace("pimc_fast", make_pimc_bot("pimc_fast", fast));
    }
    {
        PIMCConfig def;  // defaults from PIMCConfig: 10 worlds x 500 sims
        reg.emplace("pimc_default", make_pimc_bot("pimc_default", def));
    }
    {
        PIMCConfig deep;
        deep.n_worlds = 20;
        deep.n_simulations = 2000;
        reg.emplace("pimc_deep", make_pimc_bot("pimc_deep", deep));
    }

    return reg;
}

bool parse_matchup(const std::string& spec, std::string& a, std::string& b) {
    auto pos = spec.find(':');
    if (pos == std::string::npos) return false;
    a = spec.substr(0, pos);
    b = spec.substr(pos + 1);
    return !a.empty() && !b.empty();
}

}  // namespace

int main(int argc, char** argv) {
    TournamentConfig cfg;
    bool determinism_check = false;
    std::string matchup_a, matchup_b;
    bool single_matchup = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--pairings" && i + 1 < argc) cfg.n_pairings = std::atoi(argv[++i]);
        else if (arg == "--threads" && i + 1 < argc) cfg.n_threads = std::atoi(argv[++i]);
        else if (arg == "--seed" && i + 1 < argc) cfg.base_seed = std::stoull(argv[++i]);
        else if (arg == "--csv" && i + 1 < argc) cfg.results_csv = argv[++i];
        else if (arg == "--append") cfg.append_csv = true;
        else if (arg == "--progress" && i + 1 < argc) cfg.progress_every = std::atoi(argv[++i]);
        else if (arg == "--determinism-check") determinism_check = true;
        else if (arg == "--matchup" && i + 1 < argc) {
            single_matchup = true;
            if (!parse_matchup(argv[++i], matchup_a, matchup_b)) {
                std::cerr << "--matchup expects A:B\n";
                return 1;
            }
        }
        else if (arg == "--help" || arg == "-h") { print_help(); return 0; }
        else { std::cerr << "Unknown arg: " << arg << "\n"; print_help(); return 1; }
    }

    auto registry = build_registry();
    auto get_bot = [&](const std::string& id) -> const Bot& {
        auto it = registry.find(id);
        if (it == registry.end()) {
            std::cerr << "Unknown bot id: " << id << "\n";
            std::exit(1);
        }
        return it->second;
    };

    bool first_in_suite = true;
    auto run_one = [&](const Bot& a, const Bot& b, const std::string& tag) {
        TournamentConfig c = cfg;
        c.append_csv = !first_in_suite;  // first matchup truncates+writes header; rest append
        first_in_suite = false;
        std::cout << "\n--- " << tag << " (" << a.id << " vs " << b.id << ") ---\n";
        auto s = run_tournament(a, b, c);
        print_summary(s);
        return s;
    };

    if (single_matchup) {
        const Bot& a = get_bot(matchup_a);
        const Bot& b = get_bot(matchup_b);
        std::cout << "Matchup: " << a.id << " vs " << b.id << "\n"
                  << "  pairings: " << cfg.n_pairings << " (=" << cfg.n_pairings * 2 << " games)\n"
                  << "  seed:     " << cfg.base_seed << "  csv: " << cfg.results_csv << '\n';
        auto s1 = run_tournament(a, b, cfg);
        print_summary(s1);

        if (determinism_check) {
            std::cout << "\n--- determinism check: rerunning with same config ---\n";
            TournamentConfig cfg2 = cfg;
            cfg2.results_csv = cfg.results_csv + ".rerun";
            cfg2.append_csv = false;
            auto s2 = run_tournament(a, b, cfg2);
            bool ok = (s1.wins[0] == s2.wins[0])
                      && (s1.wins[1] == s2.wins[1])
                      && (s1.stalemates == s2.stalemates);
            std::cout << (ok ? "OK: identical aggregate results across runs\n"
                             : "FAIL: results diverged across runs\n");
            return ok ? 0 : 2;
        }
        return 0;
    }

    std::cout << "Default suite (random/rules core + pimc_fast smoke):\n"
              << "  pairings: " << cfg.n_pairings << " (=" << cfg.n_pairings * 2 << " games each)\n"
              << "  seed:     " << cfg.base_seed << "  csv: " << cfg.results_csv << " (suite shares CSV)\n"
              << "  note: pimc_fast smoke uses cfg.pairings/5 to keep the suite quick; bump with --matchup for real runs\n";

    run_one(get_bot("random"), get_bot("random"), "random vs random");
    run_one(get_bot("rules_full"), get_bot("random"), "rules vs random");
    run_one(get_bot("rules_full"), get_bot("rules_full"), "rules vs rules");

    // PIMC smoke at reduced pairings to keep suite snappy (PIMC is much slower per game).
    int saved_pairings = cfg.n_pairings;
    cfg.n_pairings = std::max(10, saved_pairings / 5);
    run_one(get_bot("pimc_fast"), get_bot("random"),     "pimc_fast vs random  (smoke)");
    run_one(get_bot("pimc_fast"), get_bot("rules_full"), "pimc_fast vs rules   (smoke)");
    cfg.n_pairings = saved_pairings;

    return 0;
}
