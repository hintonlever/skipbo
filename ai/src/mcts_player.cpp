#include "ai/mcts_player.h"
#include "ai/mcts_node.h"
#include "ai/determinizer.h"
#include "engine/rules.h"
#include "engine/game.h"
#include <cassert>
#include <map>
#include <algorithm>

namespace skipbo {

MCTSPlayer::MCTSPlayer(uint64_t seed, MCTSConfig config)
    : rng_(seed), config_(config) {}

// Fast heuristic move picker: directly scans the state for playable cards
// without building a full move list. Priority: stock→build, then any→build,
// then random discard.
static bool try_find_build_move(const GameState& state, Move& out,
                                 MoveSource source, Card card) {
    if (card == CARD_NONE) return false;
    for (int b = 0; b < NUM_BUILDING_PILES; ++b) {
        if (state.can_play_on_building(card, b)) {
            out = {source, static_cast<MoveTarget>(
                static_cast<int>(MoveTarget::BuildingPile0) + b)};
            return true;
        }
    }
    return false;
}

// Collect all building pile moves into a small fixed buffer. Returns count.
static int collect_build_moves(const GameState& state, Move* buf, int capacity) {
    int n = 0;
    const auto& player = state.players[state.current_player];
    // Stock
    if (!player.stock_empty()) {
        Card c = player.stock_top();
        for (int b = 0; b < NUM_BUILDING_PILES && n < capacity; ++b)
            if (state.can_play_on_building(c, b))
                buf[n++] = {MoveSource::StockPile, static_cast<MoveTarget>(
                    static_cast<int>(MoveTarget::BuildingPile0) + b)};
    }
    // Hand
    for (int h = 0; h < player.hand_count && n < capacity; ++h) {
        Card c = player.hand[h];
        auto src = static_cast<MoveSource>(static_cast<int>(MoveSource::Hand0) + h);
        for (int b = 0; b < NUM_BUILDING_PILES && n < capacity; ++b)
            if (state.can_play_on_building(c, b))
                buf[n++] = {src, static_cast<MoveTarget>(
                    static_cast<int>(MoveTarget::BuildingPile0) + b)};
    }
    // Discard piles
    for (int d = 0; d < NUM_DISCARD_PILES && n < capacity; ++d) {
        if (!player.discard_empty(d)) {
            Card c = player.discard_top(d);
            auto src = static_cast<MoveSource>(
                static_cast<int>(MoveSource::DiscardPile0) + d);
            for (int b = 0; b < NUM_BUILDING_PILES && n < capacity; ++b)
                if (state.can_play_on_building(c, b))
                    buf[n++] = {src, static_cast<MoveTarget>(
                        static_cast<int>(MoveTarget::BuildingPile0) + b)};
        }
    }
    return n;
}

static Move pick_heuristic_move(const GameState& state, std::mt19937& rng) {
    const auto& player = state.players[state.current_player];
    Move m;

    // Tier 1: stock → build (always take this)
    if (!player.stock_empty() && try_find_build_move(state, m, MoveSource::StockPile, player.stock_top()))
        return m;

    // Tier 2: any source → build (pick randomly from available)
    Move buf[64];
    int n = collect_build_moves(state, buf, 64);
    if (n > 0) {
        std::uniform_int_distribution<int> dist(0, n - 1);
        return buf[dist(rng)];
    }

    // Tier 3: random discard
    if (player.hand_count > 0) {
        std::uniform_int_distribution<int> h_dist(0, player.hand_count - 1);
        std::uniform_int_distribution<int> d_dist(0, NUM_DISCARD_PILES - 1);
        auto src = static_cast<MoveSource>(static_cast<int>(MoveSource::Hand0) + h_dist(rng));
        auto tgt = static_cast<MoveTarget>(static_cast<int>(MoveTarget::DiscardPile0) + d_dist(rng));
        return {src, tgt};
    }

    // No moves (shouldn't reach here in normal play)
    return {MoveSource::Hand0, MoveTarget::DiscardPile0};
}

static Move pick_random_move(const GameState& state, std::mt19937& rng) {
    const auto& player = state.players[state.current_player];

    // Collect build moves + discard moves into a compact list
    Move buf[64];
    int n = collect_build_moves(state, buf, 48);

    // Add discard options
    for (int h = 0; h < player.hand_count && n < 64; ++h) {
        auto src = static_cast<MoveSource>(static_cast<int>(MoveSource::Hand0) + h);
        for (int d = 0; d < NUM_DISCARD_PILES && n < 64; ++d) {
            buf[n++] = {src, static_cast<MoveTarget>(
                static_cast<int>(MoveTarget::DiscardPile0) + d)};
        }
    }
    if (n == 0) return {MoveSource::Hand0, MoveTarget::DiscardPile0};
    std::uniform_int_distribution<int> dist(0, n - 1);
    return buf[dist(rng)];
}

static double rollout(GameState state, int perspective, double heuristic_rate,
                      int rollout_depth, int root_my_stock, int root_opp_stock,
                      std::mt19937& rng) {
    std::uniform_real_distribution<double> coin(0.0, 1.0);
    int max_moves = rollout_depth * 2; // per-player depth * 2 players
    int consecutive_passes = 0;
    int moves_made = 0;
    while (!state.game_over && moves_made < max_moves && consecutive_passes < 4) {
        const auto& player = state.players[state.current_player];
        // Quick check: any hand cards or playable stock/discard?
        if (player.hand_count == 0 && player.stock_empty()) {
            bool has_move = false;
            for (int d = 0; d < NUM_DISCARD_PILES && !has_move; ++d)
                if (!player.discard_empty(d))
                    for (int b = 0; b < NUM_BUILDING_PILES && !has_move; ++b)
                        if (state.can_play_on_building(player.discard_top(d), b))
                            has_move = true;
            if (!has_move) {
                state.current_player = 1 - state.current_player;
                consecutive_passes++;
                continue;
            }
        }
        consecutive_passes = 0;
        Move m = (coin(rng) < heuristic_rate)
            ? pick_heuristic_move(state, rng)
            : pick_random_move(state, rng);
        Game::apply_move_to_state(state, m);
        moves_made++;
    }

    // Net stock card progress from root state: +1 per card I cleared, -1 per card opponent cleared.
    // A value of 0.5 means on average I clear half a card more than opponent.
    int my_progress = root_my_stock - state.players[perspective].stock_size();
    int opp_progress = root_opp_stock - state.players[1 - perspective].stock_size();
    return static_cast<double>(my_progress - opp_progress);
}

std::vector<MoveAnalysis> MCTSPlayer::analyze_moves(
        const GameState& observable_state,
        const std::vector<Move>& legal_moves) {

    if (legal_moves.empty()) return {};

    // Accumulate visit counts across determinizations
    std::map<size_t, double> move_scores;  // move index -> aggregated score
    std::map<size_t, int> move_total_visits;

    for (int d = 0; d < config_.num_determinizations; ++d) {
        GameState det_state = Determinizer::sample(
            observable_state, observable_state.current_player, rng_);

        MCTSNode root(legal_moves);
        int my_id = observable_state.current_player;
        int root_my_stock = observable_state.players[my_id].stock_size();
        int root_opp_stock = observable_state.players[1 - my_id].stock_size();

        for (int iter = 0; iter < config_.iterations_per_det; ++iter) {
            GameState sim_state = det_state;
            MCTSNode* node = &root;
            int depth = 0;

            // SELECT (limited by max_tree_depth)
            while (node->fully_expanded() && !node->is_leaf() && depth < config_.max_tree_depth) {
                node = node->select_child();
                Game::apply_move_to_state(sim_state, node->move);
                depth++;
            }

            // EXPAND
            if (!node->fully_expanded() && !sim_state.game_over && depth < config_.max_tree_depth) {
                std::uniform_int_distribution<int> dist(
                    0, node->untried_moves.size() - 1);
                int idx = dist(rng_);
                Move expand_move = node->untried_moves[idx];
                Game::apply_move_to_state(sim_state, expand_move);
                MoveList next_moves;
                get_legal_moves(sim_state, next_moves);
                node = node->add_child(expand_move, next_moves);
            }

            // ROLLOUT
            double reward = rollout(sim_state, my_id, config_.rollout_heuristic_rate,
                                    config_.rollout_depth, root_my_stock, root_opp_stock, rng_);

            // BACKPROPAGATE
            while (node != nullptr) {
                node->visits++;
                node->total_reward += reward;
                node = node->parent;
            }
        }

        // Aggregate average reward from this determinization
        for (const auto& child : root.children) {
            for (size_t i = 0; i < legal_moves.size(); ++i) {
                if (legal_moves[i] == child->move) {
                    if (child->visits > 0) {
                        move_scores[i] += child->total_reward / child->visits;
                    }
                    move_total_visits[i] += child->visits;
                    break;
                }
            }
        }
    }

    // Build results — average reward across determinizations
    std::vector<MoveAnalysis> results;
    results.reserve(legal_moves.size());
    for (size_t i = 0; i < legal_moves.size(); ++i) {
        int dets_with_visits = 0;
        // Count how many determinizations actually visited this move
        // (move_scores[i] is sum of per-det average rewards)
        double score = 0.0;
        if (move_total_visits.count(i) && move_total_visits[i] > 0) {
            score = move_scores.count(i) ? move_scores[i] / config_.num_determinizations : 0.0;
        }
        int visits = move_total_visits.count(i) ? move_total_visits[i] : 0;
        results.push_back({legal_moves[i], score, visits});
    }
    return results;
}

// Recursively serialize MCTS tree in DFS order.
// Each node: [parentIdx, source, target, visits, avgReward*1000]
static void serialize_node(const MCTSNode& node, int parent_idx, int depth,
                           int max_depth, int top_n, std::vector<int>& out) {
    int my_idx = static_cast<int>(out.size() / 5);

    out.push_back(parent_idx);
    out.push_back(parent_idx == -1 ? -1 : static_cast<int>(node.move.source));
    out.push_back(parent_idx == -1 ? -1 : static_cast<int>(node.move.target));
    out.push_back(node.visits);
    int avg_reward = node.visits > 0
        ? static_cast<int>(node.total_reward / node.visits * 1000)
        : 0;
    out.push_back(avg_reward);

    if (depth >= max_depth || node.children.empty()) return;

    // Sort children by visits descending, take top N
    std::vector<const MCTSNode*> sorted;
    sorted.reserve(node.children.size());
    for (const auto& c : node.children) {
        if (c->visits > 0) sorted.push_back(c.get());
    }
    std::sort(sorted.begin(), sorted.end(),
              [](const MCTSNode* a, const MCTSNode* b) { return a->visits > b->visits; });

    int n = std::min(top_n, static_cast<int>(sorted.size()));
    for (int i = 0; i < n; ++i) {
        serialize_node(*sorted[i], my_idx, depth + 1, max_depth, top_n, out);
    }
}

std::vector<int> MCTSPlayer::analyze_tree(
        const GameState& observable_state,
        const std::vector<Move>& legal_moves,
        int viz_max_depth, int viz_top_n) {

    if (legal_moves.empty()) return {};

    // Run a single determinization
    GameState det_state = Determinizer::sample(
        observable_state, observable_state.current_player, rng_);

    MCTSNode root(legal_moves);
    int my_id = observable_state.current_player;
    int root_my_stock = observable_state.players[my_id].stock_size();
    int root_opp_stock = observable_state.players[1 - my_id].stock_size();

    for (int iter = 0; iter < config_.iterations_per_det; ++iter) {
        GameState sim_state = det_state;
        MCTSNode* node = &root;
        int depth = 0;

        // SELECT
        while (node->fully_expanded() && !node->is_leaf() && depth < config_.max_tree_depth) {
            node = node->select_child();
            Game::apply_move_to_state(sim_state, node->move);
            depth++;
        }

        // EXPAND
        if (!node->fully_expanded() && !sim_state.game_over && depth < config_.max_tree_depth) {
            std::uniform_int_distribution<int> dist(0, node->untried_moves.size() - 1);
            int idx = dist(rng_);
            Move expand_move = node->untried_moves[idx];
            Game::apply_move_to_state(sim_state, expand_move);
            MoveList next_moves;
            get_legal_moves(sim_state, next_moves);
            node = node->add_child(expand_move, next_moves);
        }

        // ROLLOUT
        double reward = rollout(sim_state, my_id, config_.rollout_heuristic_rate,
                                config_.rollout_depth, root_my_stock, root_opp_stock, rng_);

        // BACKPROPAGATE
        while (node != nullptr) {
            node->visits++;
            node->total_reward += reward;
            node = node->parent;
        }
    }

    // Serialize the tree
    std::vector<int> result;
    serialize_node(root, -1, 0, viz_max_depth, viz_top_n, result);
    return result;
}

Move MCTSPlayer::choose_move(const GameState& observable_state,
                              const std::vector<Move>& legal_moves) {
    assert(!legal_moves.empty());

    if (legal_moves.size() == 1) return legal_moves[0];

    auto analysis = analyze_moves(observable_state, legal_moves);

    // Pick move with highest win probability
    size_t best = 0;
    for (size_t i = 1; i < analysis.size(); ++i) {
        if (analysis[i].win_probability > analysis[best].win_probability) {
            best = i;
        }
    }
    return analysis[best].move;
}

} // namespace skipbo
