#pragma once

#include <torch/torch.h>
#include <vector>
#include <cmath>
#include <iostream>
#include <iomanip>

namespace sentio::training {

/**
 * Quality-Enforced Loss Function
 * Prevents training collapse by:
 * 1. Enforcing prediction diversity (anti-collapse)
 * 2. Rewarding predictions away from neutral 
 * 3. Ensuring confidence correlates with prediction quality
 * 4. Maintaining confidence diversity
 */
class QualityEnforcedLoss {
public:
    explicit QualityEnforcedLoss(
        double base_weight = 1.0,
        double diversity_weight = 0.3,
        double confidence_weight = 0.2,
        double edge_weight = 0.5
    ) : base_weight_(base_weight),
        diversity_weight_(diversity_weight),
        confidence_weight_(confidence_weight),
        edge_weight_(edge_weight) {}

    struct LossComponents {
        torch::Tensor total;
        torch::Tensor prediction_loss;
        torch::Tensor diversity_bonus;
        torch::Tensor confidence_penalty;
        torch::Tensor edge_reward;
        
        // Statistics for monitoring
        double prediction_std;
        double mean_prediction;
        double mean_confidence;
        double edge_strength;
    };
    
    // The core loss calculation logic
    LossComponents compute_loss(torch::Tensor predictions, torch::Tensor confidence, torch::Tensor targets);
    
    // ✅ FIXED: Declarations for the implemented functions in the .cpp file
    void adjust_weights_for_epoch(int epoch, const LossComponents& last_components);
    void print_loss_breakdown(const LossComponents& components) const;
    
    // 🔧 ENHANCED: Redesigned confidence loss calculation

private:
    // These weights are now dynamically adjusted by adjust_weights_for_epoch
    double base_weight_ = 1.0;
    double diversity_weight_ = 0.3;
    double confidence_weight_ = 0.2;
    double edge_weight_ = 0.5;
    
    // 🔧 ENHANCED: Adaptive diversity enforcement tracking
    double min_diversity_ = 0.05;  // Minimum acceptable std
    double diversity_momentum_ = 0.9;  // Exponential moving average
    torch::Tensor running_std_;  // Running diversity average for stability
};

} // namespace sentio::training