#pragma once

#include "engine/game.h"
#include "engine/rules.h"
#include "ai/random_player.h"
#include "ai/heuristic_player.h"
#include "ai/heuristic_random_discard_player.h"
#include "ai/mcts_player.h"
#include "ai/nn_mcts_player.h"
#include "ai/nn_encoding.h"
#include <vector>
#include <random>
#include <queue>
#include <unordered_set>

namespace skipbo {

// Simplified API for JavaScript consumption via Embind.
// Manages a single game: human (player 0) vs AI (player 1).
class WasmGameController {
public:
    explicit WasmGameController(int seed)
        : game_(static_cast<uint64_t>(seed)),
          ai_(static_cast<uint64_t>(seed + 1000)),
          rng_(static_cast<uint64_t>(seed)) {
        game_.setup();
    }

    // --- State queries ---

    bool isGameOver() const { return game_.is_game_over(); }
    int getWinner() const { return game_.winner(); }
    int getCurrentPlayer() const { return game_.current_player(); }

    // Human player's hand (player 0)
    std::vector<int> getHand() const {
        const auto& p = game_.state().players[0];
        std::vector<int> hand;
        for (int i = 0; i < p.hand_count; ++i) {
            hand.push_back(static_cast<int>(p.hand[i]));
        }
        return hand;
    }

    // Stock pile top card for a player (-1 if empty)
    int getStockTop(int player) const {
        const auto& p = game_.state().players[player];
        return p.stock_empty() ? -1 : static_cast<int>(p.stock_top());
    }

    int getStockSize(int player) const {
        return game_.state().players[player].stock_size();
    }

    // Building pile top card (-1 if empty)
    int getBuildingPileTop(int pile) const {
        uint8_t cnt = game_.state().building_pile_count[pile];
        return cnt == 0 ? -1 : static_cast<int>(cnt);
    }

    int getBuildingPileSize(int pile) const {
        return static_cast<int>(game_.state().building_pile_count[pile]);
    }

    // Discard pile top card (-1 if empty)
    int getDiscardTop(int player, int pile) const {
        const auto& dp = game_.state().players[player].discard_piles[pile];
        return dp.empty() ? -1 : static_cast<int>(dp.back());
    }

    int getDiscardSize(int player, int pile) const {
        return static_cast<int>(game_.state().players[player].discard_piles[pile].size());
    }

    // All cards in a discard pile (bottom to top)
    std::vector<int> getDiscardPile(int player, int pile) const {
        const auto& dp = game_.state().players[player].discard_piles[pile];
        std::vector<int> cards;
        cards.reserve(dp.size());
        for (Card c : dp) cards.push_back(static_cast<int>(c));
        return cards;
    }

    // --- Legal moves ---

    // Returns legal moves as flat array: [source0, target0, source1, target1, ...]
    std::vector<int> getLegalMoves() const {
        auto moves = get_legal_moves(game_.state());
        std::vector<int> result;
        result.reserve(moves.size() * 2);
        for (const auto& m : moves) {
            result.push_back(static_cast<int>(m.source));
            result.push_back(static_cast<int>(m.target));
        }
        return result;
    }

    // --- Actions ---

    // Apply a move (source, target as ints). Returns true if successful.
    bool applyMove(int source, int target) {
        Move m{static_cast<MoveSource>(source), static_cast<MoveTarget>(target)};
        // Track Skip-Bo card usage before applying (card may be removed)
        if (!m.is_discard()) {
            Card card = CARD_NONE;
            const auto& player = game_.state().players[game_.current_player()];
            if (m.is_from_hand()) {
                int hi = m.hand_index();
                if (hi < player.hand_count) card = player.hand[hi];
            } else if (m.is_from_stock()) {
                card = player.stock_top();
            } else if (m.is_from_discard()) {
                card = player.discard_top(m.source_discard_index());
            }
            if (is_skipbo(card)) skipbo_played_[game_.current_player()]++;
        }
        return game_.apply_move(m);
    }

    // Let the MCTS AI play its turn (player 1). Returns when AI's turn is done.
    // Returns a flat array of moves the AI made: [source0, target0, source1, target1, ...]
    std::vector<int> playAITurn(int iterations, int determinizations, int turnDepth) {
        MCTSConfig config;
        config.iterations_per_det = iterations;
        config.num_determinizations = determinizations;
        config.max_turn_depth = turnDepth;
        ai_.set_config(config);

        return playAITurnWith(ai_);
    }

    // Let the heuristic AI play its turn (player 1).
    std::vector<int> playHeuristicAITurn() {
        return playAITurnWith(heuristic_ai_);
    }

    // Pass the current player's turn (used when no legal moves)
    void passTurn() {
        game_.pass_turn();
    }

    // --- Stats ---

    int getSkipBoPlayed(int player) const { return skipbo_played_[player]; }

    // --- Analysis ---

    // Analyze chains for the current player. Returns flat array:
    // [numChains, numMoves1, src, tgt, card, src, tgt, card, ..., visits, reward*1000,
    //             numMoves2, src, tgt, card, ..., visits, reward*1000, ...]
    // Card values are resolved at chain generation time (not from current snapshot).
    std::vector<int> analyzeChains(int iterations, int determinizations, int turnDepth) {
        MCTSConfig config;
        config.iterations_per_det = iterations;
        config.num_determinizations = determinizations;
        config.max_turn_depth = turnDepth;
        ai_.set_config(config);

        auto chains = ai_.analyze_chains(game_.state());

        std::vector<int> result;
        result.push_back(static_cast<int>(chains.size()));
        for (const auto& ca : chains) {
            result.push_back(ca.action.num_moves);
            for (int i = 0; i < ca.action.num_moves; i++) {
                result.push_back(static_cast<int>(ca.action.moves[i].source));
                result.push_back(static_cast<int>(ca.action.moves[i].target));
                result.push_back(static_cast<int>(ca.action.cards[i]));
            }
            result.push_back(ca.total_visits);
            result.push_back(static_cast<int>(ca.reward * 1000));
        }
        return result;
    }

    // Enumerate the full move tree from the current state.
    // Each node: [parentIdx, source, target, card, nodeType]
    // nodeType: 0=build, 1=stock_play (clears stock - terminal), 2=discard (end turn), 3=hand_empty (draw 5)
    // Root node has parentIdx=-1, source=-1, target=-1, card=-1, nodeType=-1
    std::vector<int> getMoveTree() {
        const auto& state = game_.state();
        int cp = state.current_player;
        const int FIELDS = 5;

        struct TreeBuildNode {
            GameState state;
            int parent_idx;
        };

        std::vector<int> out;
        // Root node
        out.push_back(-1); out.push_back(-1); out.push_back(-1); out.push_back(-1); out.push_back(-1);

        // BFS
        std::queue<TreeBuildNode> queue;
        queue.push({state, 0});

        // State dedup to avoid infinite/redundant expansion
        std::unordered_set<uint64_t> visited;

        int max_nodes = 2000;

        while (!queue.empty() && static_cast<int>(out.size()) / FIELDS < max_nodes) {
            auto [cur_state, parent] = queue.front();
            queue.pop();

            MoveList all_moves;
            get_legal_moves(cur_state, all_moves);

            // Separate builds and discards
            MoveList build_moves, discard_moves;
            for (int i = 0; i < all_moves.size(); i++) {
                if (all_moves[i].is_discard()) discard_moves.push_back(all_moves[i]);
                else build_moves.push_back(all_moves[i]);
            }

            // Collapse equivalent builds
            // Group by (card_value, pile_count, source_kind)
            {
                struct Key { Card card; uint8_t cnt; uint8_t sk; };
                MoveList collapsed;
                Key seen[64]; int sc = 0;
                for (int i = 0; i < build_moves.size(); i++) {
                    const auto& m = build_moves[i];
                    const auto& p = cur_state.players[cp];
                    Card card = m.is_from_hand() ? p.hand[m.hand_index()] :
                                m.is_from_stock() ? p.stock_top() :
                                p.discard_top(m.source_discard_index());
                    uint8_t cnt = cur_state.building_pile_count[m.target_building_index()];
                    uint8_t sk = m.is_from_hand() ? 0 : m.is_from_stock() ? 1 : (2 + m.source_discard_index());
                    bool dup = false;
                    for (int j = 0; j < sc; j++)
                        if (seen[j].card == card && seen[j].cnt == cnt && seen[j].sk == sk) { dup = true; break; }
                    if (!dup) { seen[sc++] = {card, cnt, sk}; collapsed.push_back(m); }
                }
                build_moves = collapsed;
            }

            // Add build move nodes
            for (int i = 0; i < build_moves.size(); i++) {
                if (static_cast<int>(out.size()) / FIELDS >= max_nodes) break;
                const auto& m = build_moves[i];
                const auto& p = cur_state.players[cp];
                Card card = m.is_from_hand() ? p.hand[m.hand_index()] :
                            m.is_from_stock() ? p.stock_top() :
                            p.discard_top(m.source_discard_index());

                int my_idx = static_cast<int>(out.size()) / FIELDS;
                bool is_stock_play = m.is_from_stock();
                int node_type = is_stock_play ? 1 : 0;

                out.push_back(parent);
                out.push_back(static_cast<int>(m.source));
                out.push_back(static_cast<int>(m.target));
                out.push_back(static_cast<int>(card));
                out.push_back(node_type);

                // If stock play, it's terminal (stock card cleared, next card unknown)
                if (is_stock_play) continue;

                // Apply move and continue tree
                GameState next = cur_state;
                Game::apply_move_to_state(next, m);

                // If hand is empty after this move, it's a hand-empty terminal
                if (next.players[cp].hand_count == 0) {
                    // Mark this node as hand_empty (type 3) instead of regular build
                    out[my_idx * FIELDS + 4] = 3;
                    continue;
                }

                // Dedup by state hash
                uint64_t h = 0;
                for (int b = 0; b < NUM_BUILDING_PILES; b++) h = h * 31 + next.building_pile_count[b];
                const auto& np = next.players[cp];
                Card sh[HAND_SIZE]; int hn = np.hand_count;
                for (int x = 0; x < hn; x++) sh[x] = np.hand[x];
                std::sort(sh, sh + hn);
                for (int x = 0; x < hn; x++) h = h * 31 + sh[x];
                h = h * 31 + hn + np.stock_size() * 1000;
                for (int d = 0; d < NUM_DISCARD_PILES; d++)
                    h = h * 31 + (np.discard_empty(d) ? 255 : np.discard_top(d));

                if (!visited.count(h)) {
                    visited.insert(h);
                    queue.push({next, my_idx});
                }
            }

            // Add discard option nodes (always terminal)
            // Collapse: group by (card, pile_content_hash)
            {
                struct Key { Card card; uint64_t ph; };
                MoveList collapsed;
                Key seen[64]; int sc = 0;
                for (int i = 0; i < discard_moves.size(); i++) {
                    const auto& m = discard_moves[i];
                    Card card = cur_state.players[cp].hand[m.hand_index()];
                    int di = m.target_discard_index();
                    const auto& dp = cur_state.players[cp].discard_piles[di];
                    uint64_t ph = dp.size();
                    for (int x = 0; x < dp.size(); x++) ph = ph * 31 + dp[x];
                    bool dup = false;
                    for (int j = 0; j < sc; j++)
                        if (seen[j].card == card && seen[j].ph == ph) { dup = true; break; }
                    if (!dup) { seen[sc++] = {card, ph}; collapsed.push_back(m); }
                }
                discard_moves = collapsed;
            }

            for (int i = 0; i < discard_moves.size(); i++) {
                if (static_cast<int>(out.size()) / FIELDS >= max_nodes) break;
                const auto& m = discard_moves[i];
                Card card = cur_state.players[cp].hand[m.hand_index()];
                out.push_back(parent);
                out.push_back(static_cast<int>(m.source));
                out.push_back(static_cast<int>(m.target));
                out.push_back(static_cast<int>(card));
                out.push_back(2); // discard type
            }
        }

        return out;
    }

    // --- Neural Network AI ---

    // Load NN weights (flat float arrays from Python training)
    void loadNNWeights(std::vector<float> valueWeights, std::vector<float> policyWeights) {
        nn_.load_value_network(valueWeights);
        nn_.load_policy_network(policyWeights);
    }

    bool hasNNWeights() const {
        return nn_.has_value_network() && nn_.has_policy_network();
    }

    // Let the NN-MCTS AI play its turn. Requires weights to be loaded.
    std::vector<int> playNNAITurn(int iterations, int determinizations, int turnDepth, float cpuct) {
        if (!hasNNWeights()) return {};
        NNMCTSConfig config;
        config.iterations_per_det = iterations;
        config.num_determinizations = determinizations;
        config.max_turn_depth = turnDepth;
        config.c_puct = cpuct;
        NNMCTSPlayer nn_ai(rng_(), config, nn_);
        return playAITurnWith(nn_ai);
    }

    // Analyze chains using NN-MCTS for the current player
    std::vector<int> analyzeNNChains(int iterations, int determinizations, int turnDepth, float cpuct) {
        if (!hasNNWeights()) return {0}; // empty chain list
        NNMCTSConfig config;
        config.iterations_per_det = iterations;
        config.num_determinizations = determinizations;
        config.max_turn_depth = turnDepth;
        config.c_puct = cpuct;
        NNMCTSPlayer nn_ai(rng_(), config, nn_);

        auto chains = nn_ai.analyze_chains(game_.state());

        std::vector<int> result;
        result.push_back(static_cast<int>(chains.size()));
        for (const auto& ca : chains) {
            result.push_back(ca.action.num_moves);
            for (int i = 0; i < ca.action.num_moves; i++) {
                result.push_back(static_cast<int>(ca.action.moves[i].source));
                result.push_back(static_cast<int>(ca.action.moves[i].target));
                result.push_back(static_cast<int>(ca.action.cards[i]));
            }
            result.push_back(ca.total_visits);
            result.push_back(static_cast<int>(ca.reward * 1000));
        }
        return result;
    }

    // Legacy: per-move analysis
    std::vector<int> analyzeMoves(int iterations, int determinizations, int turnDepth) {
        MCTSConfig config;
        config.iterations_per_det = iterations;
        config.num_determinizations = determinizations;
        config.max_turn_depth = turnDepth;
        ai_.set_config(config);

        auto legal = get_legal_moves(game_.state());
        auto analysis = ai_.analyze_moves(game_.state(), legal);

        std::vector<int> result;
        result.reserve(analysis.size() * 3);
        for (const auto& a : analysis) {
            result.push_back(static_cast<int>(a.move.source));
            result.push_back(static_cast<int>(a.move.target));
            result.push_back(static_cast<int>(a.win_probability * 1000));
        }
        return result;
    }

private:
    Game game_;
    MCTSPlayer ai_;
    HeuristicPlayer heuristic_ai_;
    NeuralNet nn_;
    std::mt19937 rng_;
    std::array<int, NUM_PLAYERS> skipbo_played_ = {0, 0};

    // Shared logic: play a full AI turn with any Player implementation
    std::vector<int> playAITurnWith(Player& player) {
        std::vector<int> moves_made;

        while (!game_.is_game_over() && game_.current_player() == 1) {
            auto legal = get_legal_moves(game_.state());
            if (legal.empty()) {
                game_.pass_turn();
                break;
            }

            Move chosen = player.choose_move(game_.state(), legal);
            moves_made.push_back(static_cast<int>(chosen.source));
            moves_made.push_back(static_cast<int>(chosen.target));
            // Track Skip-Bo usage for AI
            if (!chosen.is_discard()) {
                Card card = CARD_NONE;
                const auto& p = game_.state().players[1];
                if (chosen.is_from_hand()) {
                    int hi = chosen.hand_index();
                    if (hi < p.hand_count) card = p.hand[hi];
                } else if (chosen.is_from_stock()) {
                    card = p.stock_top();
                } else if (chosen.is_from_discard()) {
                    card = p.discard_top(chosen.source_discard_index());
                }
                if (is_skipbo(card)) skipbo_played_[1]++;
            }
            game_.apply_move(chosen);
        }

        return moves_made;
    }
};

// Run a single AI vs AI match. aiType: 0=random, 1=heuristic, 2=mcts
// Returns [winner, turns, p0StockRemaining, p1StockRemaining]
inline std::vector<int> wasm_run_match(
    int p0Type, int p0Iters, int p0Dets, int p0Heuristic, int p0Rollout, int p0Tree,
    int p1Type, int p1Iters, int p1Dets, int p1Heuristic, int p1Rollout, int p1Tree,
    int seed)
{
    auto make_player = [](int type, int iters, int dets, int heuristic, int rollout, int tree,
                          uint64_t s) -> std::unique_ptr<Player> {
        switch (type) {
            case 1: return std::make_unique<HeuristicPlayer>();
            case 3: return std::make_unique<HeuristicRandomDiscardPlayer>();
            case 2: {
                MCTSConfig cfg;
                cfg.iterations_per_det = iters;
                cfg.num_determinizations = dets;
                cfg.max_turn_depth = tree;
                (void)heuristic; (void)rollout; // legacy params, no longer used
                return std::make_unique<MCTSPlayer>(s, cfg);
            }
            default: return std::make_unique<RandomPlayer>(s);
        }
    };

    // Use distinct seed domains to avoid correlation between game RNG and AI RNG.
    // Game seed and AI seeds must not overlap.
    auto p0 = make_player(p0Type, p0Iters, p0Dets, p0Heuristic, p0Rollout, p0Tree,
                           static_cast<uint64_t>(seed + 1000));
    auto p1 = make_player(p1Type, p1Iters, p1Dets, p1Heuristic, p1Rollout, p1Tree,
                           static_cast<uint64_t>(seed + 2000));

    Game game(static_cast<uint64_t>(seed));
    game.setup();

    Player* players[2] = {p0.get(), p1.get()};
    int turns = 0;
    int consecutive_passes = 0;

    while (!game.is_game_over() && turns < 5000 && consecutive_passes < 4) {
        auto moves = get_legal_moves(game.state());
        if (moves.empty()) {
            game.pass_turn();
            consecutive_passes++;
            continue;
        }
        consecutive_passes = 0;

        int cp = game.current_player();
        Move chosen = players[cp]->choose_move(game.state(), moves);
        game.apply_move(chosen);

        if (chosen.is_discard()) ++turns;
    }

    int winner = game.winner();
    if (!game.is_game_over()) {
        int s0 = game.state().players[0].stock_size();
        int s1 = game.state().players[1].stock_size();
        if (s0 < s1) winner = 0;
        else if (s1 < s0) winner = 1;
        else winner = seed % 2;  // true tie: coin flip based on seed
    }

    int s0 = game.state().players[0].stock_size();
    int s1 = game.state().players[1].stock_size();
    return {winner, turns, s0, s1};
}

// Run a match and log training data (state encodings + chain encodings + outcomes).
// Returns flat int array with all turn records.
// Format: [num_turns,
//   for each turn: 158 state ints (float*10000), num_chains,
//     for each chain: 29 chain ints (float*10000),
//   chosen_chain_idx, outcome (filled at end, +10000 or -10000)]
inline std::vector<int> wasm_run_match_logged(
    int p0Type, int p0Iters, int p0Dets, int p0Tree,
    int p1Type, int p1Iters, int p1Dets, int p1Tree,
    int seed, int maxChainsPerTurn)
{
    auto make_player = [](int type, int iters, int dets, int tree,
                          uint64_t s) -> std::unique_ptr<Player> {
        switch (type) {
            case 1: return std::make_unique<HeuristicPlayer>();
            case 3: return std::make_unique<HeuristicRandomDiscardPlayer>();
            case 2: {
                MCTSConfig cfg;
                cfg.iterations_per_det = iters;
                cfg.num_determinizations = dets;
                cfg.max_turn_depth = tree;
                return std::make_unique<MCTSPlayer>(s, cfg);
            }
            default: return std::make_unique<RandomPlayer>(s);
        }
    };

    auto p0 = make_player(p0Type, p0Iters, p0Dets, p0Tree,
                           static_cast<uint64_t>(seed + 1000));
    auto p1 = make_player(p1Type, p1Iters, p1Dets, p1Tree,
                           static_cast<uint64_t>(seed + 2000));

    Game game(static_cast<uint64_t>(seed));
    game.setup();
    std::mt19937 log_rng(static_cast<uint64_t>(seed + 3000));

    Player* players[2] = {p0.get(), p1.get()};
    int turns = 0;
    int consecutive_passes = 0;

    // Collect turn records: each stores (perspective_player, state_enc, chains, chosen_idx)
    struct TurnRecord {
        int perspective;
        float state[STATE_ENCODING_SIZE];
        std::vector<std::array<float, CHAIN_ENCODING_SIZE>> chains;
        int chosen_idx;
    };
    std::vector<TurnRecord> records;

    while (!game.is_game_over() && turns < 5000 && consecutive_passes < 4) {
        auto moves = get_legal_moves(game.state());
        if (moves.empty()) {
            game.pass_turn();
            consecutive_passes++;
            continue;
        }
        consecutive_passes = 0;

        int cp = game.current_player();

        // At the start of each turn (before any builds), log the state and chains
        bool is_turn_start = true;
        for (const auto& m : moves) {
            if (!m.is_discard()) { /* has build moves, could be mid-turn */ }
        }
        // Heuristic: if we have discard moves available, this is likely a decision point.
        // Log at every move for simplicity, but only once per turn by checking
        // if the previous move was a discard (or this is the first move).

        // Generate all turn actions for logging
        auto turn_actions = generate_turn_actions(game.state(), log_rng,
                                                  maxChainsPerTurn);

        if (!turn_actions.empty()) {
            TurnRecord rec;
            rec.perspective = cp;
            GameState pre_turn_state = game.state();
            encode_state(pre_turn_state, cp, rec.state);

            rec.chains.resize(turn_actions.size());
            for (size_t i = 0; i < turn_actions.size(); i++) {
                encode_chain(pre_turn_state, turn_actions[i],
                             rec.chains[i].data());
            }

            // Play out the turn and figure out which chain was chosen
            while (!game.is_game_over() && game.current_player() == cp) {
                auto legal = get_legal_moves(game.state());
                if (legal.empty()) { game.pass_turn(); break; }
                Move chosen = players[cp]->choose_move(game.state(), legal);
                game.apply_move(chosen);
                if (chosen.is_discard()) break;
                if (game.current_player() != cp) break;
            }

            // Match by result: apply each candidate chain to the pre-turn state
            // and compare building piles + discard pile state with actual result.
            const GameState& post_state = game.state();
            rec.chosen_idx = -1;
            for (size_t i = 0; i < turn_actions.size(); i++) {
                GameState sim = pre_turn_state;
                apply_turn_action(sim, turn_actions[i], &log_rng);

                // Compare building pile counts
                bool match = true;
                for (int b = 0; b < NUM_BUILDING_PILES; b++) {
                    if (sim.building_pile_count[b] != post_state.building_pile_count[b]) {
                        match = false; break;
                    }
                }
                if (!match) continue;

                // Compare current player's discard piles (top cards and sizes)
                const auto& sim_p = sim.players[cp];
                const auto& post_p = post_state.players[cp];
                for (int d = 0; d < NUM_DISCARD_PILES && match; d++) {
                    if (sim_p.discard_piles[d].size() != post_p.discard_piles[d].size()) {
                        match = false;
                    } else if (!sim_p.discard_piles[d].empty() && !post_p.discard_piles[d].empty()) {
                        if (sim_p.discard_piles[d].back() != post_p.discard_piles[d].back()) {
                            match = false;
                        }
                    }
                }
                if (!match) continue;

                // Compare stock sizes
                if (sim_p.stock_size() != post_p.stock_size()) continue;

                rec.chosen_idx = static_cast<int>(i);
                break;
            }

            records.push_back(std::move(rec));
            turns++;
            continue; // already played the turn above
        }

        // Fallback: no turn actions generated, just play the move normally
        Move chosen = players[cp]->choose_move(game.state(), moves);
        game.apply_move(chosen);
        if (chosen.is_discard()) ++turns;
    }

    // Determine winner
    int winner = game.winner();
    if (!game.is_game_over()) {
        int s0 = game.state().players[0].stock_size();
        int s1 = game.state().players[1].stock_size();
        if (s0 < s1) winner = 0;
        else if (s1 < s0) winner = 1;
        else winner = seed % 2;
    }

    // Build output
    std::vector<int> out;
    out.push_back(static_cast<int>(records.size()));

    for (const auto& rec : records) {
        // State encoding (158 ints)
        for (int i = 0; i < STATE_ENCODING_SIZE; i++) {
            out.push_back(static_cast<int>(rec.state[i] * 10000.0f));
        }
        // Num chains
        out.push_back(static_cast<int>(rec.chains.size()));
        // Chain encodings
        for (const auto& chain : rec.chains) {
            for (int i = 0; i < CHAIN_ENCODING_SIZE; i++) {
                out.push_back(static_cast<int>(chain[i] * 10000.0f));
            }
        }
        // Chosen chain index
        out.push_back(rec.chosen_idx);
        // Outcome from this player's perspective
        int outcome = (winner == rec.perspective) ? 10000 : -10000;
        out.push_back(outcome);
    }

    return out;
}

} // namespace skipbo
