#pragma once

#include <torch/torch.h>
#include <deque>
#include <vector>
#include "common/types.h"

namespace sentio {
namespace features {

class PPOFeatureExtractor {
public:
    static constexpr size_t FEATURE_DIM = 16;
    static constexpr size_t HISTORY_WINDOW = 20;

    struct FeatureConfig {
        bool normalize_features = true;
        size_t sma_fast = 5;
        size_t sma_slow = 20;
        size_t rsi_period = 14;
        size_t atr_period = 14;
    };

    PPOFeatureExtractor();
    explicit PPOFeatureExtractor(const FeatureConfig& config);

    // Extract deterministic 16-dim features; throws if history insufficient
    torch::Tensor extract_features(const ::sentio::Bar& current_bar,
                                   const std::deque<::sentio::Bar>& history);

private:
    FeatureConfig config_;

    // Helpers
    static double sma(const std::deque<::sentio::Bar>& h, size_t n);
    static double stddev(const std::deque<::sentio::Bar>& h, size_t n);
    static double atr(const std::deque<::sentio::Bar>& h, size_t n);
};

} // namespace features
} // namespace sentio


