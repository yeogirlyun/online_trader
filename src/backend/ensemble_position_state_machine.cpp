#include "backend/ensemble_position_state_machine.h"
#include "common/utils.h"
#include <algorithm>
#include <numeric>
#include <cmath>

namespace sentio {

EnsemblePositionStateMachine::EnsemblePositionStateMachine() 
    : PositionStateMachine() {
    
    // Initialize performance tracking
    for (int horizon : {1, 5, 10}) {
        horizon_accuracy_[horizon] = 0.5;  // Start neutral
        horizon_pnl_[horizon] = 0.0;
        horizon_trade_count_[horizon] = 0;
    }
    
    utils::log_info("EnsemblePSM initialized with multi-horizon support");
}

EnsemblePositionStateMachine::EnsembleTransition 
EnsemblePositionStateMachine::get_ensemble_transition(
    const PortfolioState& current_portfolio,
    const EnsembleSignal& ensemble_signal,
    const MarketState& market_conditions,
    uint64_t current_bar_id) {
    
    EnsembleTransition transition;
    
    // First, check if we need to close any positions
    auto closeable = get_closeable_positions(current_bar_id);
    if (!closeable.empty()) {
        utils::log_info("Closing " + std::to_string(closeable.size()) + 
                       " positions at target horizons");
        
        // Determine current state for base PSM
        State current_state = determine_current_state(current_portfolio);
        
        transition.current_state = current_state;
        transition.target_state = State::CASH_ONLY;  // Temporary, will recalculate
        transition.optimal_action = "Close matured positions";
        
        // Mark positions for closing
        for (auto pos : closeable) {
            transition.horizon_positions.push_back(pos);
            pos.is_active = false;
        }
    }
    
    // Check signal agreement across horizons
    transition.has_consensus = ensemble_signal.signal_agreement >= MIN_AGREEMENT;
    
    if (!transition.has_consensus && get_active_positions().empty()) {
        // No consensus and no positions - stay in cash
        transition.current_state = State::CASH_ONLY;
        transition.target_state = State::CASH_ONLY;
        transition.optimal_action = "No consensus - hold cash";
        transition.theoretical_basis = "Disagreement across horizons (" + 
                                      std::to_string(ensemble_signal.signal_agreement) + ")";
        return transition;
    }
    
    // Calculate allocations based on signal strength and agreement
    auto allocations = calculate_horizon_allocations(ensemble_signal);
    transition.horizon_allocations = allocations;
    
    // Determine dominant horizon (strongest signal)
    int dominant_horizon = 0;
    double max_weight = 0.0;
    for (size_t i = 0; i < ensemble_signal.horizon_bars.size(); ++i) {
        double weight = ensemble_signal.horizon_weights[i] * 
                       std::abs(ensemble_signal.horizon_signals[i].probability - 0.5);
        if (weight > max_weight) {
            max_weight = weight;
            dominant_horizon = ensemble_signal.horizon_bars[i];
        }
    }
    transition.dominant_horizon = dominant_horizon;
    
    // Create positions for each horizon that agrees
    for (size_t i = 0; i < ensemble_signal.horizon_signals.size(); ++i) {
        const auto& signal = ensemble_signal.horizon_signals[i];
        int horizon = ensemble_signal.horizon_bars[i];
        
        // Skip if this horizon disagrees with consensus
        if (signal.signal_type != ensemble_signal.consensus_signal) {
            continue;
        }
        
        // Check if we already have a position for this horizon
        bool has_existing = false;
        for (const auto& pos : positions_by_horizon_[horizon]) {
            if (pos.is_active && pos.exit_bar_id > current_bar_id) {
                has_existing = true;
                break;
            }
        }
        
        if (!has_existing && allocations[horizon] > 0) {
            HorizonPosition new_pos;
            new_pos.symbol = signal.symbol;
            new_pos.horizon_bars = horizon;
            new_pos.entry_bar_id = current_bar_id;
            new_pos.exit_bar_id = current_bar_id + horizon;
            new_pos.predicted_return = (signal.probability - 0.5) * 2.0 / std::sqrt(horizon);
            new_pos.position_weight = allocations[horizon];
            new_pos.signal_type = signal.signal_type;
            new_pos.is_active = true;
            
            transition.horizon_positions.push_back(new_pos);
        }
    }
    
    // Calculate total position size
    transition.total_position_size = 0.0;
    for (const auto& [horizon, allocation] : allocations) {
        transition.total_position_size += allocation;
    }
    
    // Determine target state based on consensus and positions
    if (ensemble_signal.consensus_signal == sentio::SignalType::LONG) {
        if (transition.total_position_size > 0.6) {
            transition.target_state = State::TQQQ_ONLY;  // High conviction long
        } else {
            transition.target_state = State::QQQ_ONLY;   // Normal long
        }
    } else if (ensemble_signal.consensus_signal == sentio::SignalType::SHORT) {
        if (transition.total_position_size > 0.6) {
            transition.target_state = State::SQQQ_ONLY;  // High conviction short
        } else {
            transition.target_state = State::PSQ_ONLY;   // Normal short
        }
    } else {
        transition.target_state = State::CASH_ONLY;
    }
    
    // Set metadata
    transition.confidence = ensemble_signal.confidence;
    transition.expected_return = ensemble_signal.weighted_probability - 0.5;
    transition.risk_score = calculate_ensemble_risk(transition.horizon_positions);
    
    std::string action_detail = "Ensemble: ";
    for (const auto& [h, a] : allocations) {
        action_detail += std::to_string(h) + "bar=" + 
                        std::to_string(static_cast<int>(a * 100)) + "% ";
    }
    transition.optimal_action = action_detail;
    
    return transition;
}

EnsemblePositionStateMachine::EnsembleSignal 
EnsemblePositionStateMachine::aggregate_signals(
    const std::map<int, SignalOutput>& horizon_signals,
    const std::map<int, double>& horizon_weights) {
    
    EnsembleSignal ensemble;
    
    // Extract signals and weights
    for (const auto& [horizon, signal] : horizon_signals) {
        ensemble.horizon_signals.push_back(signal);
        ensemble.horizon_bars.push_back(horizon);
        
        double weight = horizon_weights.count(horizon) ? horizon_weights.at(horizon) : 1.0;
        // Adjust weight by historical performance
        if (horizon_trade_count_[horizon] > 10) {
            weight *= (0.5 + horizon_accuracy_[horizon]);  // Scale by accuracy
        }
        ensemble.horizon_weights.push_back(weight);
    }
    
    // Calculate weighted probability
    double total_weight = 0.0;
    ensemble.weighted_probability = 0.0;
    
    for (size_t i = 0; i < ensemble.horizon_signals.size(); ++i) {
        double w = ensemble.horizon_weights[i];
        ensemble.weighted_probability += ensemble.horizon_signals[i].probability * w;
        total_weight += w;
    }
    
    if (total_weight > 0) {
        ensemble.weighted_probability /= total_weight;
    }
    
    // Determine consensus signal
    ensemble.consensus_signal = determine_consensus(ensemble.horizon_signals, 
                                                   ensemble.horizon_weights);
    
    // Calculate agreement (0-1)
    ensemble.signal_agreement = calculate_agreement(ensemble.horizon_signals);
    
    // Calculate confidence based on agreement and signal strength
    double signal_strength = std::abs(ensemble.weighted_probability - 0.5) * 2.0;
    ensemble.confidence = signal_strength * ensemble.signal_agreement;
    
    utils::log_debug("Ensemble aggregation: prob=" + 
                    std::to_string(ensemble.weighted_probability) +
                    ", agreement=" + std::to_string(ensemble.signal_agreement) +
                    ", consensus=" + (ensemble.consensus_signal == sentio::SignalType::LONG ? "LONG" :
                                     ensemble.consensus_signal == sentio::SignalType::SHORT ? "SHORT" : 
                                     "NEUTRAL"));
    
    return ensemble;
}

std::map<int, double> EnsemblePositionStateMachine::calculate_horizon_allocations(
    const EnsembleSignal& signal) {
    
    std::map<int, double> allocations;
    
    // Start with base allocation for each agreeing horizon
    for (size_t i = 0; i < signal.horizon_signals.size(); ++i) {
        int horizon = signal.horizon_bars[i];
        
        if (signal.horizon_signals[i].signal_type == signal.consensus_signal) {
            // Base allocation weighted by historical performance
            double perf_weight = 0.5;  // Default
            if (horizon_trade_count_[horizon] > 5) {
                perf_weight = horizon_accuracy_[horizon];
            }
            
            allocations[horizon] = BASE_ALLOCATION * perf_weight;
        } else {
            allocations[horizon] = 0.0;
        }
    }
    
    // Add consensus bonus if high agreement
    if (signal.signal_agreement > 0.8) {
        double bonus_per_horizon = CONSENSUS_BONUS / allocations.size();
        for (auto& [horizon, alloc] : allocations) {
            alloc += bonus_per_horizon;
        }
    }
    
    // Normalize to not exceed max position
    double total = 0.0;
    for (const auto& [h, a] : allocations) {
        total += a;
    }
    
    double max_position = get_maximum_position_size();
    if (total > max_position) {
        double scale = max_position / total;
        for (auto& [h, a] : allocations) {
            a *= scale;
        }
    }
    
    return allocations;
}

void EnsemblePositionStateMachine::update_horizon_positions(uint64_t current_bar_id, 
                                                           double current_price) {
    // Update each horizon's positions
    for (auto& [horizon, positions] : positions_by_horizon_) {
        for (auto& pos : positions) {
            if (pos.is_active && current_bar_id >= pos.exit_bar_id) {
                // Calculate realized return
                double realized_return = (current_price - pos.entry_price) / pos.entry_price;
                
                // If SHORT position, reverse the return
                if (pos.signal_type == sentio::SignalType::SHORT) {
                    realized_return = -realized_return;
                }
                
                // Update performance tracking
                update_horizon_performance(horizon, realized_return);
                
                // Mark as inactive
                pos.is_active = false;
                
                utils::log_info("Closed " + std::to_string(horizon) + "-bar position: " +
                              "return=" + std::to_string(realized_return * 100) + "%");
            }
        }
        
        // Remove inactive positions
        positions.erase(
            std::remove_if(positions.begin(), positions.end(),
                          [](const HorizonPosition& p) { return !p.is_active; }),
            positions.end()
        );
    }
}

std::vector<EnsemblePositionStateMachine::HorizonPosition> 
EnsemblePositionStateMachine::get_active_positions() const {
    std::vector<HorizonPosition> active;
    
    for (const auto& [horizon, positions] : positions_by_horizon_) {
        for (const auto& pos : positions) {
            if (pos.is_active) {
                active.push_back(pos);
            }
        }
    }
    
    return active;
}

std::vector<EnsemblePositionStateMachine::HorizonPosition> 
EnsemblePositionStateMachine::get_closeable_positions(uint64_t current_bar_id) const {
    std::vector<HorizonPosition> closeable;
    
    for (const auto& [horizon, positions] : positions_by_horizon_) {
        for (const auto& pos : positions) {
            if (pos.is_active && current_bar_id >= pos.exit_bar_id) {
                closeable.push_back(pos);
            }
        }
    }
    
    return closeable;
}

double EnsemblePositionStateMachine::calculate_ensemble_risk(
    const std::vector<HorizonPosition>& positions) const {
    
    if (positions.empty()) return 0.0;
    
    // Risk increases with:
    // 1. Total position size
    // 2. Disagreement across horizons
    // 3. Longer horizons (more uncertainty)
    
    double total_weight = 0.0;
    double weighted_horizon = 0.0;
    
    for (const auto& pos : positions) {
        total_weight += pos.position_weight;
        weighted_horizon += pos.horizon_bars * pos.position_weight;
    }
    
    double avg_horizon = weighted_horizon / std::max(0.01, total_weight);
    
    // Risk score (0-1)
    double position_risk = total_weight;  // Already 0-1
    double horizon_risk = avg_horizon / 10.0;  // Normalize by max horizon
    
    return std::min(1.0, position_risk * 0.7 + horizon_risk * 0.3);
}

double EnsemblePositionStateMachine::get_maximum_position_size() const {
    // Dynamic max position based on recent performance
    double base_max = 0.8;  // 80% max
    
    // Calculate recent win rate across all horizons
    double total_accuracy = 0.0;
    double total_trades = 0.0;
    
    for (const auto& [horizon, accuracy] : horizon_accuracy_) {
        if (horizon_trade_count_.at(horizon) > 0) {
            total_accuracy += accuracy * horizon_trade_count_.at(horizon);
            total_trades += horizon_trade_count_.at(horizon);
        }
    }
    
    if (total_trades > 10) {
        double avg_accuracy = total_accuracy / total_trades;
        // Scale max position by performance
        if (avg_accuracy > 0.55) {
            base_max = std::min(0.95, base_max + (avg_accuracy - 0.55) * 2.0);
        } else if (avg_accuracy < 0.45) {
            base_max = std::max(0.5, base_max - (0.45 - avg_accuracy) * 2.0);
        }
    }
    
    return base_max;
}

sentio::SignalType EnsemblePositionStateMachine::determine_consensus(
    const std::vector<SignalOutput>& signals,
    const std::vector<double>& weights) const {
    
    double long_weight = 0.0;
    double short_weight = 0.0;
    double neutral_weight = 0.0;
    
    for (size_t i = 0; i < signals.size(); ++i) {
        double w = weights[i];
        switch (signals[i].signal_type) {
            case sentio::SignalType::LONG:
                long_weight += w;
                break;
            case sentio::SignalType::SHORT:
                short_weight += w;
                break;
            case sentio::SignalType::NEUTRAL:
                neutral_weight += w;
                break;
        }
    }
    
    // Require clear majority
    double total = long_weight + short_weight + neutral_weight;
    if (long_weight / total > 0.5) return sentio::SignalType::LONG;
    if (short_weight / total > 0.5) return sentio::SignalType::SHORT;
    return sentio::SignalType::NEUTRAL;
}

double EnsemblePositionStateMachine::calculate_agreement(
    const std::vector<SignalOutput>& signals) const {
    
    if (signals.size() <= 1) return 1.0;
    
    // Count how many signals agree with each other
    int agreements = 0;
    int comparisons = 0;
    
    for (size_t i = 0; i < signals.size(); ++i) {
        for (size_t j = i + 1; j < signals.size(); ++j) {
            comparisons++;
            if (signals[i].signal_type == signals[j].signal_type) {
                agreements++;
            }
        }
    }
    
    return comparisons > 0 ? static_cast<double>(agreements) / comparisons : 0.0;
}

void EnsemblePositionStateMachine::update_horizon_performance(int horizon, double pnl) {
    horizon_pnl_[horizon] += pnl;
    horizon_trade_count_[horizon]++;
    
    // Update accuracy (exponentially weighted)
    double was_correct = pnl > 0 ? 1.0 : 0.0;
    double alpha = 0.1;  // Learning rate
    horizon_accuracy_[horizon] = (1 - alpha) * horizon_accuracy_[horizon] + alpha * was_correct;
    
    utils::log_info("Horizon " + std::to_string(horizon) + " performance: " +
                   "accuracy=" + std::to_string(horizon_accuracy_[horizon]) +
                   ", total_pnl=" + std::to_string(horizon_pnl_[horizon]) +
                   ", trades=" + std::to_string(horizon_trade_count_[horizon]));
}

bool EnsemblePositionStateMachine::should_override_hold(
    const EnsembleSignal& signal, uint64_t current_bar_id) const {
    
    // Override hold if:
    // 1. Very high agreement (>90%)
    // 2. Strong signal from best-performing horizon
    // 3. No conflicting positions
    
    if (signal.signal_agreement > 0.9 && signal.confidence > 0.7) {
        return true;
    }
    
    // Check if best performer strongly agrees
    int best_horizon = 0;
    double best_accuracy = 0.0;
    for (const auto& [h, acc] : horizon_accuracy_) {
        if (horizon_trade_count_.at(h) > 10 && acc > best_accuracy) {
            best_accuracy = acc;
            best_horizon = h;
        }
    }
    
    if (best_accuracy > 0.6) {
        // Find signal from best horizon
        for (size_t i = 0; i < signal.horizon_bars.size(); ++i) {
            if (signal.horizon_bars[i] == best_horizon) {
                double signal_strength = std::abs(signal.horizon_signals[i].probability - 0.5);
                if (signal_strength > 0.3) {  // Strong signal from best performer
                    return true;
                }
            }
        }
    }
    
    return false;
}

} // namespace sentio
