#pragma once

#include "engine/move.h"
#include <vector>
#include <memory>
#include <cmath>

namespace skipbo {

struct MCTSNode {
    Move move;
    MCTSNode* parent = nullptr;
    std::vector<std::unique_ptr<MCTSNode>> children;
    int visits = 0;
    double total_reward = 0.0;
    std::vector<Move> untried_moves;

    MCTSNode() = default;
    explicit MCTSNode(const std::vector<Move>& legal_moves);

    double ucb1(double exploration = 1.414) const;
    MCTSNode* select_child();
    MCTSNode* add_child(const Move& m, const std::vector<Move>& legal_moves);
    MCTSNode* best_child() const; // highest visit count

    bool fully_expanded() const { return untried_moves.empty(); }
    bool is_leaf() const { return children.empty(); }
};

} // namespace skipbo
