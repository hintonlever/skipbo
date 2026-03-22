#include "ai/mcts_node.h"
#include <cassert>
#include <limits>

namespace skipbo {

MCTSNode::MCTSNode(const MoveList& legal_moves)
    : untried_moves(legal_moves) {}

MCTSNode::MCTSNode(const std::vector<Move>& legal_moves) {
    for (const auto& m : legal_moves) untried_moves.push_back(m);
}

double MCTSNode::ucb1(double exploration) const {
    if (visits == 0) return std::numeric_limits<double>::infinity();
    double exploit = total_reward / visits;
    double explore = exploration * std::sqrt(std::log(parent->visits) / visits);
    return exploit + explore;
}

MCTSNode* MCTSNode::select_child() {
    assert(!children.empty());
    MCTSNode* best = nullptr;
    double best_score = -1.0;
    for (auto& child : children) {
        double score = child->ucb1();
        if (score > best_score) {
            best_score = score;
            best = child.get();
        }
    }
    return best;
}

MCTSNode* MCTSNode::add_child(const Move& m, const MoveList& legal_moves) {
    auto child = std::make_unique<MCTSNode>(legal_moves);
    child->move = m;
    child->parent = this;

    // Remove from untried (swap-and-pop for O(1))
    for (int i = 0; i < untried_moves.size(); ++i) {
        if (untried_moves[i] == m) {
            untried_moves.swap_erase(i);
            break;
        }
    }

    MCTSNode* ptr = child.get();
    children.push_back(std::move(child));
    return ptr;
}

MCTSNode* MCTSNode::best_child() const {
    assert(!children.empty());
    MCTSNode* best = nullptr;
    int best_visits = -1;
    for (auto& child : children) {
        if (child->visits > best_visits) {
            best_visits = child->visits;
            best = child.get();
        }
    }
    return best;
}

} // namespace skipbo
