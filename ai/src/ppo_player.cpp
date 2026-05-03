#include "ai/ppo_player.h"
#include "ai/nn_encoding.h"
#include <cassert>
#include <stdexcept>

namespace skipbo {

// Number of sources (Hand0-4, StockPile, DiscardPile0-3) = 10
// Number of targets (BuildingPile0-3, DiscardPile0-3) = 8
static constexpr int NUM_SOURCES = 10;
static constexpr int NUM_TARGETS = 8;
static constexpr int NUM_ACTIONS = NUM_SOURCES * NUM_TARGETS; // 80

// PPONet implementation

void PPONet::load(const std::vector<float>& flat_weights) {
    // Architecture: 158 -> 256 -> 128 -> 80
    struct LayerSpec { int in, out; };
    LayerSpec specs[] = {
        {STATE_ENCODING_SIZE, 256},
        {256, 128},
        {128, NUM_ACTIONS}
    };

    layers_.clear();
    size_t offset = 0;

    for (const auto& spec : specs) {
        PPODenseLayer layer;
        layer.in_size = spec.in;
        layer.out_size = spec.out;

        size_t w_count = static_cast<size_t>(spec.in) * spec.out;
        size_t b_count = static_cast<size_t>(spec.out);

        if (offset + w_count + b_count > flat_weights.size()) {
            throw std::runtime_error("PPONet: insufficient weights");
        }

        layer.weights.assign(flat_weights.begin() + offset,
                             flat_weights.begin() + offset + w_count);
        offset += w_count;
        layer.bias.assign(flat_weights.begin() + offset,
                          flat_weights.begin() + offset + b_count);
        offset += b_count;

        layers_.push_back(std::move(layer));
    }
}

void PPONet::forward(const float* state, float* logits) const {
    assert(!layers_.empty());

    // Scratch buffers (max hidden = 256)
    float buf_a[300];
    float buf_b[300];

    const float* input = state;
    float* output = buf_a;

    for (size_t li = 0; li < layers_.size(); li++) {
        const auto& layer = layers_[li];
        bool is_last = (li == layers_.size() - 1);

        if (is_last) {
            output = logits;
        }

        for (int o = 0; o < layer.out_size; o++) {
            float sum = layer.bias[o];
            const float* w_row = layer.weights.data() + o * layer.in_size;
            for (int i = 0; i < layer.in_size; i++) {
                sum += w_row[i] * input[i];
            }
            if (!is_last) {
                output[o] = sum > 0.0f ? sum : 0.0f; // ReLU
            } else {
                output[o] = sum; // raw logits
            }
        }

        if (!is_last) {
            input = output;
            output = (input == buf_a) ? buf_b : buf_a;
        }
    }
}

// PPOPlayer implementation

PPOPlayer::PPOPlayer(const PPONet& net) : net_(net) {}

int PPOPlayer::move_to_action_index(const Move& m) {
    int source = static_cast<int>(m.source);  // 0-9
    int target = static_cast<int>(m.target);  // 0-7
    return source * NUM_TARGETS + target;
}

Move PPOPlayer::action_index_to_move(int idx) {
    int source = idx / NUM_TARGETS;
    int target = idx % NUM_TARGETS;
    return Move{static_cast<MoveSource>(source), static_cast<MoveTarget>(target)};
}

Move PPOPlayer::choose_move(const GameState& observable_state,
                            const std::vector<Move>& legal_moves) {
    assert(!legal_moves.empty());

    // Encode state from current player's perspective
    float state_enc[STATE_ENCODING_SIZE];
    encode_state(observable_state, observable_state.current_player, state_enc);

    // Run forward pass
    float logits[NUM_ACTIONS];
    net_.forward(state_enc, logits);

    // Find best legal move
    float best_score = -1e30f;
    int best_idx = -1;

    for (const auto& move : legal_moves) {
        int action_idx = move_to_action_index(move);
        if (logits[action_idx] > best_score) {
            best_score = logits[action_idx];
            best_idx = action_idx;
        }
    }

    assert(best_idx >= 0);
    // Return the actual legal move (preserves hand indices etc.)
    for (const auto& move : legal_moves) {
        if (move_to_action_index(move) == best_idx) {
            return move;
        }
    }

    // Fallback (should not reach)
    return legal_moves[0];
}

} // namespace skipbo
