// File: src/backend/enhanced_position_state_machine.cpp
#include "backend/enhanced_position_state_machine.h"
#include "common/utils.h"
#include "backend/adaptive_trading_mechanism.h"
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <chrono>

namespace sentio {

EnhancedPositionStateMachine::EnhancedPositionStateMachine(
    std::shared_ptr<backend::DynamicHysteresisManager> hysteresis_mgr,
    std::shared_ptr<backend::DynamicAllocationManager> allocation_mgr,
    const EnhancedConfig& config)
    : PositionStateMachine(),
      hysteresis_manager_(hysteresis_mgr),
      allocation_manager_(allocation_mgr),
      config_(config),
      current_state_(State::CASH_ONLY),
      previous_state_(State::CASH_ONLY),
      bars_in_position_(0),
      total_bars_processed_(0),
      current_regime_("UNKNOWN"),
      regime_bars_count_(0) {
    
    // Create default managers if not provided
    if (!hysteresis_manager_ && config_.enable_hysteresis) {
        backend::DynamicHysteresisManager::HysteresisConfig h_config{};
        hysteresis_manager_ = std::make_shared<backend::DynamicHysteresisManager>(h_config);
    }
    if (!allocation_manager_ && config_.enable_dynamic_allocation) {
        backend::DynamicAllocationManager::AllocationConfig a_config{};
        allocation_manager_ = std::make_shared<backend::DynamicAllocationManager>(a_config);
    }
}

PositionStateMachine::StateTransition EnhancedPositionStateMachine::get_optimal_transition(
    const PortfolioState& current_portfolio,
    const SignalOutput& signal,
    const MarketState& market_conditions,
    double confidence_threshold) {
    
    // Use enhanced version internally
    auto enhanced = get_enhanced_transition(current_portfolio, signal, market_conditions);
    
    // Convert to base class transition
    StateTransition base_transition;
    base_transition.current_state = enhanced.current_state;
    base_transition.signal_type = enhanced.signal_type;
    base_transition.target_state = enhanced.target_state;
    base_transition.optimal_action = enhanced.optimal_action;
    base_transition.theoretical_basis = enhanced.theoretical_basis;
    base_transition.expected_return = enhanced.expected_return;
    base_transition.risk_score = enhanced.risk_score;
    base_transition.confidence = enhanced.confidence;
    
    return base_transition;
}

EnhancedPositionStateMachine::EnhancedTransition 
EnhancedPositionStateMachine::get_enhanced_transition(
    const PortfolioState& current_portfolio,
    const SignalOutput& signal,
    const MarketState& market_conditions) {
    
    total_bars_processed_++;
    
    // Update signal history for hysteresis
    if (hysteresis_manager_ && config_.enable_hysteresis) {
        hysteresis_manager_->update_signal_history(signal);
    }
    
    // 1. Determine current state
    State current_state = determine_current_state(current_portfolio);
    
    if (current_state == State::INVALID) {
        EnhancedTransition emergency;
        emergency.current_state = State::INVALID;
        emergency.signal_type = SignalType::NEUTRAL;
        emergency.target_state = State::CASH_ONLY;
        emergency.optimal_action = "Emergency liquidation";
        emergency.theoretical_basis = "Invalid state detected - risk containment";
        emergency.expected_return = 0.0;
        emergency.risk_score = 0.0;
        emergency.confidence = 1.0;
        return emergency;
    }
    
    // Update position tracking
    update_position_tracking(current_state);
    
    // 2. Get DYNAMIC thresholds based on current state (HYSTERESIS!)
    backend::DynamicHysteresisManager::DynamicThresholds thresholds;
    if (hysteresis_manager_ && config_.enable_hysteresis) {
        thresholds = hysteresis_manager_->get_thresholds(current_state, signal, bars_in_position_);
    } else {
        // Fallback to static thresholds (hardcoded to match base PSM)
        constexpr double DEFAULT_BUY = 0.55;
        constexpr double DEFAULT_SELL = 0.45;
        constexpr double STRONG_MARGIN = 0.15;
        constexpr double CONFIDENCE_MIN = 0.7;
        
        thresholds.buy_threshold = DEFAULT_BUY;
        thresholds.sell_threshold = DEFAULT_SELL;
        thresholds.strong_buy_threshold = DEFAULT_BUY + STRONG_MARGIN;
        thresholds.strong_sell_threshold = DEFAULT_SELL - STRONG_MARGIN;
        thresholds.confidence_threshold = CONFIDENCE_MIN;
        thresholds.signal_variance = 0.0;
        thresholds.signal_mean = 0.5;
        thresholds.signal_momentum = 0.0;
        thresholds.regime = "UNKNOWN";
    }
    
    // Store regime
    current_regime_ = thresholds.regime;
    
    // 3. Adapt confidence based on recent performance
    // Confidence adaptation removed - using signal strength from probability only
    SignalOutput adjusted_signal = signal;
    
    // Log threshold adjustments
    if (config_.log_threshold_changes) {
        std::stringstream ss;
        ss << "DYNAMIC THRESHOLDS:"
           << " State=" << static_cast<int>(current_state)
           << " Buy=" << std::fixed << std::setprecision(3) << thresholds.buy_threshold
           << " Sell=" << thresholds.sell_threshold
           << " Confidence=" << thresholds.confidence_threshold
           << " Variance=" << thresholds.signal_variance
           << " Momentum=" << thresholds.signal_momentum
           << " Regime=" << thresholds.regime
           << " BarsInPos=" << bars_in_position_;
        utils::log_info(ss.str());
    }
    
    // 4. Classify signal using DYNAMIC thresholds
    SignalType signal_type = classify_signal_with_hysteresis(adjusted_signal, thresholds);
    
    // Check for forced transition due to position age
    if (config_.track_bars_in_position && should_force_transition(current_state)) {
        if (is_long_state(current_state)) {
            signal_type = SignalType::WEAK_SELL;  // Force exit from aged long
        } else if (is_short_state(current_state)) {
            signal_type = SignalType::WEAK_BUY;   // Force exit from aged short
        }
    }
    
    // Handle NEUTRAL signal
    if (signal_type == SignalType::NEUTRAL) {
        EnhancedTransition hold;
        hold.current_state = current_state;
        hold.signal_type = signal_type;
        hold.target_state = current_state;
        hold.optimal_action = "Hold position";
        hold.theoretical_basis = "Signal in neutral zone";
        hold.expected_return = 0.0;
        hold.risk_score = 0.0;
        hold.confidence = 0.5;
        hold.thresholds_used = thresholds;
        hold.bars_in_current_position = bars_in_position_;
        hold.regime = thresholds.regime;
        hold.original_probability = signal.probability;
        hold.adjusted_probability = signal.probability;
        // Confidence tracking removed
        return hold;
    }
    
    // 5. Determine transition using base class logic (FIXED!)
    StateTransition base_transition = get_base_transition(current_state, signal_type);
    
    // Track statistics for monitoring
    stats_.total_signals++;
    if (signal_type == SignalType::STRONG_SELL || signal_type == SignalType::WEAK_SELL) {
        stats_.short_signals++;
        if (is_short_state(base_transition.target_state)) {
            stats_.short_transitions++;
        }
    } else if (signal_type == SignalType::STRONG_BUY || signal_type == SignalType::WEAK_BUY) {
        stats_.long_signals++;
        if (is_long_state(base_transition.target_state)) {
            stats_.long_transitions++;
        }
    }
    if (base_transition.target_state != current_state) {
        stats_.transitions_triggered++;
    }
    
    // Log the transition for debugging
    if (config_.log_threshold_changes && base_transition.target_state != current_state) {
        std::stringstream ss;
        ss << "STATE TRANSITION: "
           << state_to_string(current_state) << " -> " 
           << state_to_string(base_transition.target_state)
           << " (Signal: " << signal_type_to_string(signal_type) << ")"
           << " [Stats: " << stats_.short_transitions << "/" << stats_.short_signals 
           << " shorts, " << stats_.long_transitions << "/" << stats_.long_signals << " longs]";
        utils::log_info(ss.str());
    }
    
    // 6. Create enhanced transition with allocation info
    double available_capital = current_portfolio.cash_balance;
    
    // Add liquidation value of current positions if transitioning
    if (base_transition.target_state != current_state) {
        for (const auto& [symbol, position] : current_portfolio.positions) {
            available_capital += position.quantity * position.current_price;
        }
    }
    
    EnhancedTransition enhanced = create_enhanced_transition(
        base_transition, adjusted_signal, thresholds, available_capital, market_conditions);
    
    // Store adjustment metadata
    enhanced.original_probability = signal.probability;
    enhanced.adjusted_probability = signal.probability;
    // Confidence tracking removed
    enhanced.bars_in_current_position = bars_in_position_;
    enhanced.regime = thresholds.regime;
    
    // Calculate current P&L if in position
    if (current_state != State::CASH_ONLY && !current_portfolio.positions.empty()) {
        double total_pnl = 0.0;
        for (const auto& [symbol, position] : current_portfolio.positions) {
            total_pnl += (position.current_price - position.avg_price) * position.quantity;
        }
        enhanced.position_pnl = total_pnl;
    }
    
    return enhanced;
}

void EnhancedPositionStateMachine::update_signal_history(const SignalOutput& signal) {
    if (hysteresis_manager_) {
        hysteresis_manager_->update_signal_history(signal);
    }
}

void EnhancedPositionStateMachine::update_position_tracking(State new_state) {
    if (new_state != current_state_) {
        previous_state_ = current_state_;
        current_state_ = new_state;
        bars_in_position_ = 0;
    } else {
        bars_in_position_++;
    }
}

void EnhancedPositionStateMachine::record_trade_result(double pnl, bool was_profitable) {
    if (!config_.track_performance) return;
    
    TradeResult result;
    result.pnl = pnl;
    result.profitable = was_profitable;
    result.timestamp = std::chrono::system_clock::now().time_since_epoch().count();
    
    recent_trades_.push_back(result);
    
    // Maintain window size
    while (recent_trades_.size() > static_cast<size_t>(config_.performance_window)) {
        recent_trades_.pop_front();
    }
}

double EnhancedPositionStateMachine::get_recent_win_rate() const {
    if (recent_trades_.empty()) return 0.5;  // Default assumption
    
    int wins = 0;
    for (const auto& trade : recent_trades_) {
        if (trade.profitable) wins++;
    }
    
    return static_cast<double>(wins) / recent_trades_.size();
}

double EnhancedPositionStateMachine::get_recent_avg_pnl() const {
    if (recent_trades_.empty()) return 0.0;
    
    double total_pnl = 0.0;
    for (const auto& trade : recent_trades_) {
        total_pnl += trade.pnl;
    }
    
    return total_pnl / recent_trades_.size();
}

PositionStateMachine::SignalType EnhancedPositionStateMachine::classify_signal_with_hysteresis(
    const SignalOutput& signal,
    const backend::DynamicHysteresisManager::DynamicThresholds& thresholds) const {
    
    // Confidence filter removed - using signal strength from probability only
    
    // Classify using DYNAMIC thresholds (HYSTERESIS!)
    double p = signal.probability;
    
    if (p > thresholds.strong_buy_threshold)  return SignalType::STRONG_BUY;
    if (p > thresholds.buy_threshold)         return SignalType::WEAK_BUY;
    if (p < thresholds.strong_sell_threshold) return SignalType::STRONG_SELL;
    if (p < thresholds.sell_threshold)        return SignalType::WEAK_SELL;
    
    return SignalType::NEUTRAL;
}

// adapt_confidence() function removed - confidence no longer used

bool EnhancedPositionStateMachine::should_force_transition(State current_state) const {
    if (!config_.track_bars_in_position) return false;
    
    // Don't force if in cash
    if (current_state == State::CASH_ONLY) return false;
    
    // Force re-evaluation after max bars
    return bars_in_position_ >= config_.max_bars_in_position;
}

bool EnhancedPositionStateMachine::is_dual_state(State state) const {
    return state == State::QQQ_TQQQ || state == State::PSQ_SQQQ;
}

bool EnhancedPositionStateMachine::is_long_state(State state) const {
    return state == State::QQQ_ONLY || 
           state == State::TQQQ_ONLY || 
           state == State::QQQ_TQQQ;
}

bool EnhancedPositionStateMachine::is_short_state(State state) const {
    return state == State::PSQ_ONLY || 
           state == State::SQQQ_ONLY || 
           state == State::PSQ_SQQQ;
}

EnhancedPositionStateMachine::EnhancedTransition 
EnhancedPositionStateMachine::create_enhanced_transition(
    const StateTransition& base_transition,
    const SignalOutput& signal,
    const backend::DynamicHysteresisManager::DynamicThresholds& thresholds,
    double available_capital,
    const MarketState& market) {
    
    EnhancedTransition enhanced;
    
    // Copy base transition fields
    enhanced.current_state = base_transition.current_state;
    enhanced.signal_type = base_transition.signal_type;
    enhanced.target_state = base_transition.target_state;
    enhanced.optimal_action = base_transition.optimal_action;
    enhanced.theoretical_basis = base_transition.theoretical_basis;
    enhanced.expected_return = base_transition.expected_return;
    enhanced.risk_score = base_transition.risk_score;
    enhanced.confidence = base_transition.confidence;
    
    // Add threshold info
    enhanced.thresholds_used = thresholds;
    
    // Calculate allocation if transitioning to dual state
    if (allocation_manager_ && is_dual_state(enhanced.target_state)) {
        // FIX: Use actual market price from MarketState
        // All symbols use same underlying price (QQQ close price)
        double current_price = market.current_price > 0.0 ? market.current_price : 100.0;
        double price_base = current_price;
        double price_leveraged = current_price;  // Same price for all
        
        // Market conditions for allocation
        backend::DynamicAllocationManager::MarketConditions alloc_market;
        alloc_market.current_volatility = market.volatility;
        alloc_market.volatility_percentile = 50.0;
        alloc_market.trend_strength = thresholds.signal_momentum;
        alloc_market.market_regime = thresholds.regime;
        
        // Calculate allocation
        enhanced.allocation = allocation_manager_->calculate_dual_allocation(
            enhanced.target_state,
            signal,
            available_capital,
            price_base,
            price_leveraged,
            alloc_market
        );
        
        // Update theoretical basis with allocation info
        enhanced.theoretical_basis += " | " + enhanced.allocation.allocation_rationale;
    }
    // For single positions, use simple allocation
    else if (allocation_manager_ && enhanced.target_state != State::CASH_ONLY) {
        std::string symbol;
        bool is_leveraged = false;
        // FIX: Use actual market price for all symbols
        double current_price = market.current_price > 0.0 ? market.current_price : 100.0;
        double price = current_price;
        
        switch (enhanced.target_state) {
            case State::QQQ_ONLY:
                symbol = "QQQ";
                break;
            case State::TQQQ_ONLY:
                symbol = "TQQQ";
                is_leveraged = true;
                break;
            case State::PSQ_ONLY:
                symbol = "PSQ";
                break;
            case State::SQQQ_ONLY:
                symbol = "SQQQ";
                is_leveraged = true;
                break;
            default:
                break;
        }
        
        if (!symbol.empty()) {
            enhanced.allocation = allocation_manager_->calculate_single_allocation(
                symbol,
                signal,
                available_capital,
                price,
                is_leveraged
            );
        }
    }
    
    return enhanced;
}

void EnhancedPositionStateMachine::log_threshold_changes(
    const backend::DynamicHysteresisManager::DynamicThresholds& old_thresholds,
    const backend::DynamicHysteresisManager::DynamicThresholds& new_thresholds) const {
    
    std::stringstream ss;
    ss << "THRESHOLD CHANGES:"
       << " Buy: " << old_thresholds.buy_threshold << "→" << new_thresholds.buy_threshold
       << " Sell: " << old_thresholds.sell_threshold << "→" << new_thresholds.sell_threshold
       << " Confidence: " << old_thresholds.confidence_threshold << "→" << new_thresholds.confidence_threshold
       << " Regime: " << old_thresholds.regime << "→" << new_thresholds.regime;
    
    utils::log_debug(ss.str());
}

} // namespace sentio

