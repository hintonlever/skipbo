#include "ai/neural_net.h"
#include "ai/nn_encoding.h"
#include <cmath>
#include <algorithm>
#include <cassert>
#include <stdexcept>

namespace skipbo {

std::vector<DenseLayer> NeuralNet::parse_layers(
        const std::vector<float>& flat_weights,
        const std::vector<std::pair<int, int>>& layer_sizes) {
    std::vector<DenseLayer> layers;
    size_t offset = 0;

    for (const auto& [in_sz, out_sz] : layer_sizes) {
        DenseLayer layer;
        layer.in_size = in_sz;
        layer.out_size = out_sz;

        size_t w_count = static_cast<size_t>(in_sz) * out_sz;
        size_t b_count = static_cast<size_t>(out_sz);

        if (offset + w_count + b_count > flat_weights.size()) {
            throw std::runtime_error("NeuralNet: insufficient weights for layer");
        }

        layer.weights.assign(flat_weights.begin() + offset,
                             flat_weights.begin() + offset + w_count);
        offset += w_count;
        layer.bias.assign(flat_weights.begin() + offset,
                          flat_weights.begin() + offset + b_count);
        offset += b_count;

        layers.push_back(std::move(layer));
    }
    return layers;
}

void NeuralNet::load_value_network(const std::vector<float>& flat_weights) {
    // 158 -> 128 -> 64 -> 1
    value_layers_ = parse_layers(flat_weights, {
        {STATE_ENCODING_SIZE, 128}, {128, 64}, {64, 1}
    });
}

void NeuralNet::load_policy_network(const std::vector<float>& flat_weights) {
    // 187 -> 128 -> 64 -> 1
    int input_size = STATE_ENCODING_SIZE + CHAIN_ENCODING_SIZE;
    policy_layers_ = parse_layers(flat_weights, {
        {input_size, 128}, {128, 64}, {64, 1}
    });
}

void NeuralNet::forward(const std::vector<DenseLayer>& layers,
                        const float* input, float* output,
                        int final_activation) {
    // We need scratch buffers for intermediate results.
    // Max hidden size is 128 for our architectures.
    float buf_a[256];
    float buf_b[256];

    const float* current_input = input;
    float* current_output = buf_a;

    for (size_t li = 0; li < layers.size(); li++) {
        const auto& layer = layers[li];
        bool is_last = (li == layers.size() - 1);

        // Use output buffer directly for last layer
        if (is_last) {
            current_output = output;
        }

        // Matrix multiply: output = weights * input + bias
        for (int o = 0; o < layer.out_size; o++) {
            float sum = layer.bias[o];
            const float* w_row = layer.weights.data() + o * layer.in_size;
            for (int i = 0; i < layer.in_size; i++) {
                sum += w_row[i] * current_input[i];
            }

            if (is_last) {
                // Apply final activation
                if (final_activation == 1) {
                    sum = std::tanh(sum);
                }
                current_output[o] = sum;
            } else {
                // ReLU for hidden layers
                current_output[o] = sum > 0.0f ? sum : 0.0f;
            }
        }

        // Swap buffers for next layer
        if (!is_last) {
            current_input = current_output;
            current_output = (current_input == buf_a) ? buf_b : buf_a;
        }
    }
}

float NeuralNet::evaluate_value(const float* state) const {
    assert(!value_layers_.empty());
    float result;
    forward(value_layers_, state, &result, 1); // tanh
    return result;
}

float NeuralNet::evaluate_policy(const float* state, const float* chain) const {
    assert(!policy_layers_.empty());
    // Concatenate state + chain
    float input[STATE_ENCODING_SIZE + CHAIN_ENCODING_SIZE];
    std::copy(state, state + STATE_ENCODING_SIZE, input);
    std::copy(chain, chain + CHAIN_ENCODING_SIZE, input + STATE_ENCODING_SIZE);
    float result;
    forward(policy_layers_, input, &result, 0); // no activation
    return result;
}

void NeuralNet::evaluate_policy_batch(const float* state, const float* chains,
                                       int num_chains, float* priors_out) const {
    if (num_chains == 0) return;

    // Score each chain
    float max_score = -1e30f;
    for (int i = 0; i < num_chains; i++) {
        float score = evaluate_policy(state, chains + i * CHAIN_ENCODING_SIZE);
        priors_out[i] = score;
        if (score > max_score) max_score = score;
    }

    // Softmax with numerical stability
    float sum = 0.0f;
    for (int i = 0; i < num_chains; i++) {
        priors_out[i] = std::exp(priors_out[i] - max_score);
        sum += priors_out[i];
    }
    if (sum > 0.0f) {
        for (int i = 0; i < num_chains; i++) {
            priors_out[i] /= sum;
        }
    } else {
        // Uniform fallback
        float uniform = 1.0f / num_chains;
        for (int i = 0; i < num_chains; i++) {
            priors_out[i] = uniform;
        }
    }
}

} // namespace skipbo
