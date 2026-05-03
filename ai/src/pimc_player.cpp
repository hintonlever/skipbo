#include "ai/pimc_player.h"
#include "ai/determinizer.h"
#include "engine/game.h"
#include "engine/rules.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <utility>

namespace skipbo {

namespace {

struct PIMCNode {
    GameState state{};
    int player_to_move = 0;
    bool terminal = false;
    double terminal_reward = 0.0;  // valid only if terminal
    int visits = 0;
    double value_sum = 0.0;        // from perspective player's POV
    int parent = -1;
    Move move_from_parent{MoveSource::Hand0, MoveTarget::BuildingPile0};
    std::vector<int> children;
    std::vector<Move> untried;
};

double evaluate_terminal(const GameState& state, int perspective) {
    if (state.game_over) {
        return (state.winner == perspective) ? 1.0 : -1.0;
    }
    int s_p = state.players[perspective].stock_size();
    int s_o = state.players[1 - perspective].stock_size();
    if (s_p < s_o) return 1.0;
    if (s_o < s_p) return -1.0;
    return 0.0;
}

Move uniform_random_policy(const GameState&, const std::vector<Move>& moves, std::mt19937& rng) {
    std::uniform_int_distribution<size_t> d(0, moves.size() - 1);
    return moves[d(rng)];
}

double random_rollout(GameState state, int perspective, std::mt19937& rng,
                      const PIMCRolloutPolicy& policy, int cap_turns) {
    int turns = 0;
    while (!state.game_over && turns < cap_turns) {
        auto moves = get_legal_moves(state);
        if (moves.empty()) break;  // no legal moves: treat as terminal-by-stock-count
        Move m = policy(state, moves, rng);
        Game::apply_move_to_state(state, m, &rng);
        if (m.is_discard()) ++turns;
    }
    return evaluate_terminal(state, perspective);
}

class PIMCSearch {
public:
    PIMCSearch(GameState root_state, int perspective, const PIMCConfig& cfg, std::mt19937& rng)
        : perspective_(perspective), cfg_(cfg), rng_(rng) {
        nodes_.reserve(static_cast<size_t>(cfg.n_simulations) + 8);
        nodes_.emplace_back();
        PIMCNode& root = nodes_.back();
        root.state = std::move(root_state);
        root.player_to_move = root.state.current_player;
        root.terminal = root.state.game_over;
        if (root.terminal) {
            root.terminal_reward = evaluate_terminal(root.state, perspective_);
        } else {
            root.untried = get_legal_moves(root.state);
        }
    }

    void run() {
        for (int i = 0; i < cfg_.n_simulations; ++i) {
            int leaf = select_and_expand(0);
            double reward = nodes_[leaf].terminal
                ? nodes_[leaf].terminal_reward
                : random_rollout(nodes_[leaf].state, perspective_, rng_,
                                 cfg_.rollout_policy, cfg_.rollout_cap_turns);
            backprop(leaf, reward);
        }
    }

    const std::vector<PIMCNode>& nodes() const { return nodes_; }

private:
    int select_and_expand(int idx) {
        while (true) {
            PIMCNode& n = nodes_[idx];
            if (n.terminal) return idx;
            if (!n.untried.empty()) return expand(idx);
            if (n.children.empty()) {
                // No untried, no children: dead end without game_over -> stalemate.
                n.terminal = true;
                n.terminal_reward = evaluate_terminal(n.state, perspective_);
                return idx;
            }
            idx = best_ucb_child(idx);
        }
    }

    int expand(int idx) {
        // Pop a random untried move.
        std::uniform_int_distribution<size_t> d(0, nodes_[idx].untried.size() - 1);
        size_t pick = d(rng_);
        Move move = nodes_[idx].untried[pick];
        nodes_[idx].untried[pick] = nodes_[idx].untried.back();
        nodes_[idx].untried.pop_back();

        GameState new_state = nodes_[idx].state;
        Game::apply_move_to_state(new_state, move, &rng_);

        nodes_.emplace_back();
        int child_idx = static_cast<int>(nodes_.size()) - 1;
        nodes_[idx].children.push_back(child_idx);

        PIMCNode& child = nodes_[child_idx];
        child.state = std::move(new_state);
        child.player_to_move = child.state.current_player;
        child.parent = idx;
        child.move_from_parent = move;
        child.terminal = child.state.game_over;
        if (child.terminal) {
            child.terminal_reward = evaluate_terminal(child.state, perspective_);
        } else {
            child.untried = get_legal_moves(child.state);
        }
        return child_idx;
    }

    int best_ucb_child(int idx) {
        const PIMCNode& parent = nodes_[idx];
        double log_parent = std::log(static_cast<double>(std::max(1, parent.visits)));
        bool parent_is_perspective = (parent.player_to_move == perspective_);

        int best = parent.children[0];
        double best_score = -1e18;
        for (int ci : parent.children) {
            const PIMCNode& c = nodes_[ci];
            double mean = (c.visits > 0) ? c.value_sum / c.visits : 0.0;
            // mean is stored from perspective player's POV; opponent maximizes -mean.
            if (!parent_is_perspective) mean = -mean;
            double explore = cfg_.ucb_c * std::sqrt(log_parent / std::max(1, c.visits));
            double score = mean + explore;
            if (score > best_score) {
                best_score = score;
                best = ci;
            }
        }
        return best;
    }

    void backprop(int idx, double reward) {
        while (idx >= 0) {
            PIMCNode& n = nodes_[idx];
            n.visits++;
            n.value_sum += reward;
            idx = n.parent;
        }
    }

    int perspective_;
    const PIMCConfig& cfg_;
    std::mt19937& rng_;
    std::vector<PIMCNode> nodes_;
};

}  // namespace

PIMCPlayer::PIMCPlayer(uint64_t seed, PIMCConfig config, std::string name)
    : config_(std::move(config)), rng_(seed), name_(std::move(name)) {
    if (!config_.rollout_policy) {
        config_.rollout_policy = &uniform_random_policy;
    }
}

Move PIMCPlayer::choose_move(const GameState& observable, const std::vector<Move>& legal) {
    assert(!legal.empty());
    if (legal.size() == 1) return legal[0];

    int perspective = observable.current_player;

    // Aggregate root-child visits and value across determinized worlds, indexed by `legal` index.
    std::vector<std::pair<int, double>> agg(legal.size(), {0, 0.0});

    for (int w = 0; w < config_.n_worlds; ++w) {
        GameState world = Determinizer::sample(observable, perspective, rng_);
        PIMCSearch search(std::move(world), perspective, config_, rng_);
        search.run();

        const auto& nodes = search.nodes();
        for (int ci : nodes[0].children) {
            const auto& c = nodes[ci];
            for (size_t i = 0; i < legal.size(); ++i) {
                if (legal[i] == c.move_from_parent) {
                    agg[i].first += c.visits;
                    agg[i].second += c.value_sum;
                    break;
                }
            }
        }
    }

    // Most-robust: pick by max aggregated visits. Tie-break on aggregated mean value.
    int best = 0;
    for (size_t i = 1; i < legal.size(); ++i) {
        if (agg[i].first > agg[best].first) {
            best = static_cast<int>(i);
        } else if (agg[i].first == agg[best].first) {
            double mi = agg[i].first > 0 ? agg[i].second / agg[i].first : 0.0;
            double mb = agg[best].first > 0 ? agg[best].second / agg[best].first : 0.0;
            if (mi > mb) best = static_cast<int>(i);
        }
    }
    return legal[best];
}

} // namespace skipbo
