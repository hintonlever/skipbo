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

    // Collect build moves first — always play builds before discarding
    Move buf[64];
    int n = collect_build_moves(state, buf, 48);

    // Only add discard options if no build moves available
    if (n == 0) {
        for (int h = 0; h < player.hand_count && n < 64; ++h) {
            auto src = static_cast<MoveSource>(static_cast<int>(MoveSource::Hand0) + h);
            for (int d = 0; d < NUM_DISCARD_PILES && n < 64; ++d) {
                buf[n++] = {src, static_cast<MoveTarget>(
                    static_cast<int>(MoveTarget::DiscardPile0) + d)};
            }
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
    int max_turns = rollout_depth * 2; // per-player turns * 2 players
    int consecutive_passes = 0;
    int turns = 0;
    while (!state.game_over && turns < max_turns && consecutive_passes < 4) {
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
        Game::apply_move_to_state(state, m, &rng);
        if (m.is_discard()) turns++;
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

    // Accumulate raw total_reward and visits across all determinizations
    std::map<size_t, double> move_total_reward;
    std::map<size_t, int> move_total_visits;

    for (int d = 0; d < config_.num_determinizations; ++d) {
        GameState det_state = Determinizer::sample(
            observable_state, observable_state.current_player, rng_);

        MCTSNode root(legal_moves);
        root.acting_player = static_cast<uint8_t>(observable_state.current_player);
        int my_id = observable_state.current_player;
        int root_my_stock = observable_state.players[my_id].stock_size();
        int root_opp_stock = observable_state.players[1 - my_id].stock_size();

        for (int iter = 0; iter < config_.iterations_per_det; ++iter) {
            GameState sim_state = det_state;
            MCTSNode* node = &root;
            int depth = 0;

            // SELECT — depth counts complete turns (discards); limit is per player,
            // so max_tree_depth=3 means 3 turns each = 6 discards total.
            int max_discards = config_.max_tree_depth * 2;
            while (node->fully_expanded() && !node->is_leaf() && depth < max_discards) {
                node = node->select_child();
                Game::apply_move_to_state(sim_state, node->move, &rng_);
                if (node->move.is_discard()) depth++;
            }

            // EXPAND
            if (!node->fully_expanded() && !sim_state.game_over && depth < max_discards) {
                uint8_t actor = sim_state.current_player;
                std::uniform_int_distribution<int> dist(
                    0, node->untried_moves.size() - 1);
                int idx = dist(rng_);
                Move expand_move = node->untried_moves[idx];
                Game::apply_move_to_state(sim_state, expand_move, &rng_);
                MoveList next_moves;
                get_legal_moves(sim_state, next_moves);
                node = node->add_child(expand_move, next_moves);
                node->acting_player = actor;
            }

            // ROLLOUT — reward is from root player's perspective
            double reward = rollout(sim_state, my_id, config_.rollout_heuristic_rate,
                                    config_.rollout_depth, root_my_stock, root_opp_stock, rng_);

            // BACKPROPAGATE — negate reward at opponent nodes so UCB1
            // selects the best move for whoever is acting at each level
            while (node != nullptr) {
                node->visits++;
                node->total_reward += (node->acting_player == my_id) ? reward : -reward;
                node = node->parent;
            }
        }

        // Accumulate raw reward and visits from this determinization
        for (const auto& child : root.children) {
            for (size_t i = 0; i < legal_moves.size(); ++i) {
                if (legal_moves[i] == child->move) {
                    move_total_reward[i] += child->total_reward;
                    move_total_visits[i] += child->visits;
                    break;
                }
            }
        }
    }

    // Build results — total_reward / total_visits across all determinizations
    std::vector<MoveAnalysis> results;
    results.reserve(legal_moves.size());
    for (size_t i = 0; i < legal_moves.size(); ++i) {
        int visits = move_total_visits.count(i) ? move_total_visits[i] : 0;
        double score = visits > 0 ? move_total_reward[i] / visits : 0.0;
        results.push_back({legal_moves[i], score, visits});
    }
    return results;
}

// Recursively serialize MCTS tree in DFS order.
// Each node: [parentIdx, source, target, visits, avgReward*1000, actingPlayer]
// Rewards are always from the root player's perspective.
static void serialize_node(const MCTSNode& node, int parent_idx, int depth,
                           int max_depth, int top_n, int root_player,
                           std::vector<int>& out) {
    int my_idx = static_cast<int>(out.size() / 6);

    out.push_back(parent_idx);
    out.push_back(parent_idx == -1 ? -1 : static_cast<int>(node.move.source));
    out.push_back(parent_idx == -1 ? -1 : static_cast<int>(node.move.target));
    out.push_back(node.visits);
    // Convert reward to root player's perspective (opponent nodes store negated)
    double raw = node.visits > 0 ? node.total_reward / node.visits : 0.0;
    if (node.acting_player != root_player && parent_idx != -1) raw = -raw;
    out.push_back(static_cast<int>(raw * 1000));
    out.push_back(static_cast<int>(node.acting_player));

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
        serialize_node(*sorted[i], my_idx, depth + 1, max_depth, top_n, root_player, out);
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
    root.acting_player = static_cast<uint8_t>(observable_state.current_player);
    int my_id = observable_state.current_player;
    int root_my_stock = observable_state.players[my_id].stock_size();
    int root_opp_stock = observable_state.players[1 - my_id].stock_size();

    for (int iter = 0; iter < config_.iterations_per_det; ++iter) {
        GameState sim_state = det_state;
        MCTSNode* node = &root;
        int depth = 0;

        int max_discards = config_.max_tree_depth * 2;
        while (node->fully_expanded() && !node->is_leaf() && depth < max_discards) {
            node = node->select_child();
            Game::apply_move_to_state(sim_state, node->move);
            if (node->move.is_discard()) depth++;
        }

        if (!node->fully_expanded() && !sim_state.game_over && depth < max_discards) {
            uint8_t actor = sim_state.current_player;
            std::uniform_int_distribution<int> dist(0, node->untried_moves.size() - 1);
            int idx = dist(rng_);
            Move expand_move = node->untried_moves[idx];
            Game::apply_move_to_state(sim_state, expand_move);
            MoveList next_moves;
            get_legal_moves(sim_state, next_moves);
            node = node->add_child(expand_move, next_moves);
            node->acting_player = actor;
        }

        double reward = rollout(sim_state, my_id, config_.rollout_heuristic_rate,
                                config_.rollout_depth, root_my_stock, root_opp_stock, rng_);

        while (node != nullptr) {
            node->visits++;
            node->total_reward += (node->acting_player == my_id) ? reward : -reward;
            node = node->parent;
        }
    }

    // Serialize the tree
    std::vector<int> result;
    serialize_node(root, -1, 0, viz_max_depth, viz_top_n, my_id, result);
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
