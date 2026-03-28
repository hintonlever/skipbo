#pragma once

#include <vector>

namespace skipbo {

struct DenseLayer {
    std::vector<float> weights; // row-major [out_size x in_size]
    std::vector<float> bias;    // [out_size]
    int in_size = 0;
    int out_size = 0;
};

// Lightweight neural network for inference only.
// Supports loading flat weight vectors and running forward passes.
// No external dependencies — just matrix multiply + ReLU/Tanh.
class NeuralNet {
public:
    NeuralNet() = default;

    // Load networks from flat weight vectors.
    // Format: [w1, b1, w2, b2, w3, b3] concatenated.
    // Value network: 158->128->64->1 (tanh output)
    // Policy network: 187->128->64->1 (raw score output)
    void load_value_network(const std::vector<float>& flat_weights);
    void load_policy_network(const std::vector<float>& flat_weights);

    // Value network: state -> win probability in [-1, +1]
    float evaluate_value(const float* state) const;

    // Policy network: score a single chain given state
    float evaluate_policy(const float* state, const float* chain) const;

    // Score all chains and return softmax probabilities.
    // chains: packed array of num_chains * 29 floats
    // priors_out: array of num_chains floats (softmax probabilities)
    void evaluate_policy_batch(const float* state, const float* chains,
                               int num_chains, float* priors_out) const;

    bool has_value_network() const { return !value_layers_.empty(); }
    bool has_policy_network() const { return !policy_layers_.empty(); }

private:
    std::vector<DenseLayer> value_layers_;
    std::vector<DenseLayer> policy_layers_;

    // Parse flat weights into layer structure given architecture
    static std::vector<DenseLayer> parse_layers(
        const std::vector<float>& flat_weights,
        const std::vector<std::pair<int, int>>& layer_sizes);

    // Forward pass through layers. ReLU on hidden layers.
    // final_activation: 0=none, 1=tanh
    static void forward(const std::vector<DenseLayer>& layers,
                        const float* input, float* output,
                        int final_activation);
};

} // namespace skipbo
