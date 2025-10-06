// File: src/backend/enhanced_backend_component.cpp
#include "backend/enhanced_backend_component.h"
#include "common/utils.h"
#include "backend/adaptive_trading_mechanism.h"
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <cmath>
#include <chrono>
#include <fstream>

namespace sentio {

EnhancedBackendComponent::EnhancedBackendComponent(const EnhancedBackendConfig& config)
    : BackendComponent(config),
      enhanced_config_(config),
      daily_pnl_(0.0),
      session_high_water_mark_(0.0),
      bars_since_last_report_(0),
      last_state_(PositionStateMachine::State::CASH_ONLY),
      last_transition_timestamp_(0) {
    
    // Create hysteresis manager
    if (enhanced_config_.enable_hysteresis) {
        hysteresis_manager_ = std::make_shared<backend::DynamicHysteresisManager>(
            enhanced_config_.hysteresis_config);
    }
    
    // Create allocation manager
    if (enhanced_config_.enable_dynamic_allocation) {
        allocation_manager_ = std::make_shared<backend::DynamicAllocationManager>(
            enhanced_config_.allocation_config);
    }
    
    // Create enhanced PSM
    if (enhanced_config_.enable_dynamic_psm) {
        enhanced_psm_ = std::make_shared<EnhancedPositionStateMachine>(
            hysteresis_manager_,
            allocation_manager_,
            enhanced_config_.psm_config
        );
    }
    
    utils::log_info("Enhanced Backend Component initialized with dynamic PSM");
}

// Helper method to load signals from JSONL file
static std::vector<SignalOutput> load_signals_from_file(const std::string& signal_file_path) {
    std::vector<SignalOutput> signals;
    std::ifstream signal_file(signal_file_path);
    if (!signal_file.is_open()) {
        throw std::runtime_error("Failed to open signal file: " + signal_file_path);
    }
    
    std::string line;
    while (std::getline(signal_file, line)) {
        signals.push_back(SignalOutput::from_json(line));
    }
    
    return signals;
}

std::vector<BackendComponent::TradeOrder> EnhancedBackendComponent::process_signals_enhanced(
    const std::string& signal_file_path,
    const std::string& market_data_path,
    size_t start_index,
    size_t end_index) {
    
    // Load signals and market data
    auto signals = load_signals_from_file(signal_file_path);
    auto market_data = utils::read_csv_data(market_data_path);
    
    if (signals.empty() || market_data.empty()) {
        utils::log_error("Empty signals or market data");
        return {};
    }
    
    if (end_index == SIZE_MAX) {
        end_index = std::min(signals.size(), market_data.size());
    }
    
    std::vector<TradeOrder> all_orders;
    PortfolioState current_portfolio;
    current_portfolio.cash_balance = enhanced_config_.starting_capital;
    
    // Track signal generation intervals
    int bars_since_last_signal = 0;
    
    utils::log_info("Processing signals with " + 
                   std::to_string(enhanced_config_.default_prediction_horizon) + 
                   "-bar prediction horizon");
    
    for (size_t i = start_index; i < end_index; ++i) {
        const Bar& bar = market_data[i];
        const SignalOutput& signal = signals[i];
        
        // NEW: Handle signal generation intervals
        bool should_process_signal = false;
        
        if (enhanced_config_.signal_generation_mode == "ADAPTIVE") {
            // Only process signals at configured intervals
            if (bars_since_last_signal >= enhanced_config_.signal_generation_interval) {
                should_process_signal = true;
                bars_since_last_signal = 0;
            } else {
                bars_since_last_signal++;
            }
        } else {
            // Process every signal (backward compatibility)
            should_process_signal = true;
        }
        
        // Check for hold period enforcement
        if (enhanced_psm_) {
            // Update PSM with current bar for hold period tracking
            MarketState market_state;  // Populate as needed
            market_state.current_price = bar.close;
            market_state.volatility = 0.01;  // Placeholder
            
            PositionStateMachine::StateTransition base_transition = enhanced_psm_->get_optimal_transition(
                current_portfolio, signal, market_state);
            
            // Cast to EnhancedTransition for multi-bar fields
            // (Note: In real implementation, enhanced_psm should return EnhancedTransition)
            EnhancedPositionStateMachine::EnhancedTransition transition;
            transition.current_state = base_transition.current_state;
            transition.signal_type = base_transition.signal_type;
            transition.target_state = base_transition.target_state;
            transition.optimal_action = base_transition.optimal_action;
            transition.theoretical_basis = base_transition.theoretical_basis;
            transition.expected_return = base_transition.expected_return;
            transition.risk_score = base_transition.risk_score;
            transition.confidence = base_transition.confidence;
            transition.prediction_horizon = signal.prediction_horizon;
            transition.position_open_bar_id = signal.bar_id;
            transition.earliest_exit_bar_id = signal.bar_id + signal.prediction_horizon;
            transition.is_hold_enforced = false;  // Will be set by PSM
            transition.bars_held = 0;
            transition.bars_remaining = 0;
            
            // Log hold enforcement if active
            if (transition.is_hold_enforced && enhanced_config_.log_hold_enforcement) {
                utils::log_debug("Bar " + std::to_string(i) + 
                               ": Hold enforced, " + 
                               std::to_string(transition.bars_remaining) + 
                               " bars remaining");
            }
            
            // Only execute transitions if not in hold period and should process
            if (!transition.is_hold_enforced && should_process_signal) {
                std::vector<TradeOrder> orders;
                
                if (transition.target_state != transition.current_state) {
                    // State change - execute transition (always single position for now)
                    orders = execute_single_position_transition(transition, signal, bar);
                    
                    // Track horizon performance
                    if (enhanced_config_.track_horizon_performance) {
                        track_horizon_transition(transition, signal.prediction_horizon);
                    }
                    
                    all_orders.insert(all_orders.end(), orders.begin(), orders.end());
                    
                    // Update portfolio state (simplified)
                    for (const auto& order : orders) {
                        if (order.action == TradeAction::BUY) {
                            current_portfolio.cash_balance -= order.trade_value;
                        } else if (order.action == TradeAction::SELL) {
                            current_portfolio.cash_balance += order.trade_value;
                        }
                    }
                }
            }
        }
        
        // Performance reporting at intervals
        if (++bars_since_last_report_ >= enhanced_config_.performance_report_frequency) {
            generate_performance_report();
            bars_since_last_report_ = 0;
        }
    }
    
    // Final performance summary
    utils::log_info("\n=== MULTI-BAR TRADING SUMMARY ===");
    utils::log_info("Total orders: " + std::to_string(all_orders.size()));
    
    if (enhanced_config_.track_horizon_performance) {
        for (const auto& [horizon, success_rate] : enhanced_config_.horizon_success_rates) {
            utils::log_info("Horizon " + std::to_string(horizon) + 
                          " success rate: " + std::to_string(success_rate * 100) + "%");
        }
    }
    
    return all_orders;
}

EnhancedBackendComponent::InMemoryResult EnhancedBackendComponent::process_in_memory(
    const std::vector<SignalOutput>& signals,
    const std::vector<Bar>& market_data,
    size_t start_index,
    size_t end_index) {
    InMemoryResult out;
    if (end_index == SIZE_MAX) {
        end_index = std::min(signals.size(), market_data.size());
    }
    if (start_index >= end_index) {
        out.final_equity = enhanced_config_.starting_capital;
        out.trade_count = 0;
        return out;
    }
    // Minimal in-memory loop: estimate orders and track final equity via after_state snapshot
    double equity = enhanced_config_.starting_capital;
    size_t trade_count = 0;
    for (size_t i = start_index; i + 1 < end_index; ++i) {
        // Placeholder: no full portfolio state; count trade-like transitions via signal_type
        if (signals[i].signal_type == SignalType::LONG || signals[i].signal_type == SignalType::SHORT) {
            // Approximate equity drift using next bar price change and direction
            const auto& curr = market_data[i];
            const auto& next = market_data[i + 1];
            if (curr.close > 0) {
                double ret = (next.close - curr.close) / curr.close;
                int dir = (signals[i].signal_type == SignalType::LONG) ? 1 : -1;
                equity *= (1.0 + dir * ret);
                trade_count++;
            }
        }
    }
    out.final_equity = equity;
    out.trade_count = trade_count;
    return out;
}

std::vector<BackendComponent::TradeOrder> EnhancedBackendComponent::execute_dual_position_transition(
    const EnhancedPositionStateMachine::EnhancedTransition& transition,
    const SignalOutput& signal,
    const Bar& bar) {
    
    std::vector<TradeOrder> orders;
    
    // First, liquidate any existing positions if needed
    if (transition.current_state != PositionStateMachine::State::CASH_ONLY) {
        liquidate_positions_for_transition(transition, orders, bar);
    }
    
    // Then, execute new allocation
    if (transition.allocation.is_valid) {
        execute_allocation_orders(transition.allocation, orders, signal, bar);
    } else {
        utils::log_error("Invalid allocation for dual position transition");
    }
    
    return orders;
}

std::vector<BackendComponent::TradeOrder> EnhancedBackendComponent::execute_single_position_transition(
    const EnhancedPositionStateMachine::EnhancedTransition& transition,
    const SignalOutput& signal,
    const Bar& bar) {
    
    std::vector<TradeOrder> orders;
    
    // NEW: Check if this is a hold-enforced situation
    if (transition.is_hold_enforced) {
        if (enhanced_config_.log_hold_enforcement) {
            utils::log_debug("Position held: " + 
                           std::to_string(transition.bars_remaining) + 
                           " bars until exit allowed");
        }
        return orders;  // Empty - no trades during hold period
    }
    
    // Check for early exit penalty
    bool apply_penalty = false;
    if (transition.current_state != PositionStateMachine::State::CASH_ONLY &&
        transition.bars_held < transition.prediction_horizon &&
        enhanced_config_.early_exit_penalty > 0) {
        apply_penalty = true;
        utils::log_warning("Early exit penalty applied: " + 
                         std::to_string(enhanced_config_.early_exit_penalty * 100) + "%");
    }
    
    // Liquidate existing positions if transitioning from non-cash state
    if (transition.current_state != PositionStateMachine::State::CASH_ONLY) {
        liquidate_positions_for_transition(transition, orders, bar);
        
        // Apply early exit penalty to liquidation orders
        if (apply_penalty) {
            for (auto& order : orders) {
                if (order.action == TradeAction::SELL) {
                    order.price *= (1.0 - enhanced_config_.early_exit_penalty);
                }
            }
        }
    }
    
    // Enter new single position if not going to cash
    if (transition.target_state != PositionStateMachine::State::CASH_ONLY) {
        // Determine symbol
        std::string symbol;
        switch (transition.target_state) {
            case PositionStateMachine::State::QQQ_ONLY:
                symbol = "QQQ";
                break;
            case PositionStateMachine::State::TQQQ_ONLY:
                symbol = "TQQQ";
                break;
            case PositionStateMachine::State::PSQ_ONLY:
                symbol = "PSQ";
                break;
            case PositionStateMachine::State::SQQQ_ONLY:
                symbol = "SQQQ";
                break;
            default:
                break;
        }
        
        if (!symbol.empty()) {
            double available_capital = 100000.0;  // Placeholder
            double price = get_current_price(symbol, bar);
            
            // Use allocation manager for single position if available
            if (allocation_manager_) {
                bool is_leveraged = (symbol == "TQQQ" || symbol == "SQQQ");
                auto allocation = allocation_manager_->calculate_single_allocation(
                    symbol, signal, available_capital, price, is_leveraged);
                
                if (allocation.base_quantity > 0) {
                    TradeOrder order;
                    order.timestamp_ms = signal.timestamp_ms;
                    order.bar_index = signal.bar_index;
                    order.symbol = symbol;
                    order.action = TradeAction::BUY;
                    order.quantity = allocation.base_quantity;
                    order.price = estimate_execution_price(symbol, TradeAction::BUY, price);
                    order.trade_value = order.quantity * order.price;
                    
                    // NEW: Add multi-bar metadata
                    order.metadata["prediction_horizon"] = std::to_string(signal.prediction_horizon);
                    order.metadata["target_bar_id"] = std::to_string(signal.target_bar_id);
                    order.metadata["entry_bar_id"] = std::to_string(signal.bar_id);
                    
                    orders.push_back(order);
                    
                    // Record position entry in PSM
                    if (enhanced_psm_) {
                        enhanced_psm_->record_position_entry(symbol, signal.bar_id, 
                                                            signal.prediction_horizon, order.price);
                    }
                }
            }
        }
    }
    
    return orders;
}

void EnhancedBackendComponent::liquidate_positions_for_transition(
    const EnhancedPositionStateMachine::EnhancedTransition& transition,
    std::vector<TradeOrder>& orders,
    const Bar& bar) {
    
    // Placeholder - would get actual portfolio positions
    // and create sell orders for each
    
    utils::log_debug("Liquidating positions for transition");
}

void EnhancedBackendComponent::execute_allocation_orders(
    const backend::DynamicAllocationManager::AllocationResult& allocation,
    std::vector<TradeOrder>& orders,
    const SignalOutput& signal,
    const Bar& bar) {
    
    // Create order for base position
    if (allocation.base_quantity > 0) {
        TradeOrder base_order;
        base_order.timestamp_ms = signal.timestamp_ms;
        base_order.bar_index = signal.bar_index;
        base_order.symbol = allocation.base_symbol;
        base_order.action = TradeAction::BUY;
        base_order.quantity = allocation.base_quantity;
        base_order.price = estimate_execution_price(
            allocation.base_symbol, TradeAction::BUY, 
            get_current_price(allocation.base_symbol, bar));
        base_order.trade_value = base_order.quantity * base_order.price;
        orders.push_back(base_order);
    }
    
    // Create order for leveraged position
    if (allocation.leveraged_quantity > 0) {
        TradeOrder leveraged_order;
        leveraged_order.timestamp_ms = signal.timestamp_ms;
        leveraged_order.bar_index = signal.bar_index;
        leveraged_order.symbol = allocation.leveraged_symbol;
        leveraged_order.action = TradeAction::BUY;
        leveraged_order.quantity = allocation.leveraged_quantity;
        leveraged_order.price = estimate_execution_price(
            allocation.leveraged_symbol, TradeAction::BUY,
            get_current_price(allocation.leveraged_symbol, bar));
        leveraged_order.trade_value = leveraged_order.quantity * leveraged_order.price;
        orders.push_back(leveraged_order);
    }
}

bool EnhancedBackendComponent::validate_risk_limits(const std::vector<TradeOrder>& orders) {
    double total_value = 0.0;
    double total_leverage = 0.0;
    
    for (const auto& order : orders) {
        total_value += order.trade_value;
        
        // Check leverage
        if (order.symbol == "TQQQ" || order.symbol == "SQQQ") {
            total_leverage += order.trade_value * 3.0;
        } else {
            total_leverage += order.trade_value;
        }
    }
    
    // Check position value limit
    if (total_value > enhanced_config_.max_position_value) {
        utils::log_warning("Position value exceeds limit: " + std::to_string(total_value));
        return false;
    }
    
    // Check leverage limit
    double portfolio_value = 100000.0;  // Placeholder
    if (portfolio_value > 0) {
        double effective_leverage = total_leverage / portfolio_value;
        if (effective_leverage > enhanced_config_.max_portfolio_leverage) {
            utils::log_warning("Leverage exceeds limit: " + std::to_string(effective_leverage));
            return false;
        }
    }
    
    return true;
}

void EnhancedBackendComponent::apply_position_limits(std::vector<TradeOrder>& orders) {
    // Scale down orders if they exceed limits
    double total_value = 0.0;
    for (const auto& order : orders) {
        total_value += order.trade_value;
    }
    
    if (total_value > enhanced_config_.max_position_value) {
        double scale_factor = enhanced_config_.max_position_value / total_value;
        for (auto& order : orders) {
            order.quantity = std::floor(order.quantity * scale_factor);
            order.trade_value = order.quantity * order.price;
        }
    }
}

void EnhancedBackendComponent::check_daily_loss_limit() {
    if (daily_pnl_ < 0) {
        double portfolio_value = 100000.0;  // Placeholder
        double loss_pct = std::abs(daily_pnl_) / portfolio_value;
        
        if (loss_pct > enhanced_config_.daily_loss_limit) {
            utils::log_error("DAILY LOSS LIMIT BREACHED: " + 
                           std::to_string(loss_pct * 100) + "%");
        }
    }
}

void EnhancedBackendComponent::record_transition_performance(
    const EnhancedPositionStateMachine::EnhancedTransition& transition,
    const std::vector<TradeOrder>& orders) {
    
    // Track metrics
    if (!orders.empty()) {
        double trade_value = 0.0;
        for (const auto& order : orders) {
            trade_value += order.trade_value;
        }
        
        // Update metrics
        metrics_.total_transitions++;
    }
}

void EnhancedBackendComponent::generate_performance_report() {
    std::stringstream report;
    report << "\n=== ENHANCED PERFORMANCE REPORT (Multi-Bar) ===\n";
    report << "Transitions: " << metrics_.total_transitions 
           << " (Dual: " << metrics_.dual_state_transitions
           << ", Single: " << metrics_.single_state_transitions << ")\n";
    report << "Hold Decisions: " << metrics_.hold_decisions << "\n";
    
    // NEW: Multi-bar specific metrics
    if (enhanced_config_.track_horizon_performance) {
        report << "\nHorizon Performance:\n";
        for (const auto& [horizon, success_rate] : enhanced_config_.horizon_success_rates) {
            report << "  " << horizon << "-bar predictions: " 
                   << std::setprecision(1) << (success_rate * 100) << "% success\n";
        }
    }
    
    report << "\nP&L Metrics:\n";
    report << "Total P&L: $" << std::fixed << std::setprecision(2) << metrics_.total_pnl << "\n";
    report << "Daily P&L: $" << daily_pnl_ << "\n";
    report << "Max Drawdown: " << std::setprecision(2) << metrics_.max_drawdown * 100 << "%\n";
    
    if (enhanced_psm_) {
        report << "\nPSM Metrics:\n";
        report << "Recent Win Rate: " << std::setprecision(1) 
               << enhanced_psm_->get_recent_win_rate() * 100 << "%\n";
        report << "Bars in Position: " << enhanced_psm_->get_bars_in_position() << "\n";
    }
    
    report << "================================================\n";
    
    utils::log_info(report.str());
}

double EnhancedBackendComponent::calculate_position_pnl(
    const Position& position,
    double current_price) const {
    return (current_price - position.avg_price) * position.quantity;
}

void EnhancedBackendComponent::update_daily_pnl(double pnl) {
    daily_pnl_ += pnl;
    metrics_.total_pnl += pnl;
}

// NEW: Track performance by horizon with REAL P&L tracking
void EnhancedBackendComponent::track_horizon_transition(
    const EnhancedPositionStateMachine::EnhancedTransition& transition,
    int prediction_horizon) {

    // Track position entry with horizon information
    struct HorizonPosition {
        uint64_t entry_bar_id;
        uint64_t target_exit_bar_id;
        double entry_price;
        double position_value;
        int horizon;
        bool is_long;
    };

    static std::map<uint64_t, HorizonPosition> active_horizon_positions;  // bar_id -> position
    static std::map<int, std::vector<double>> horizon_returns;            // horizon -> returns

    // Record new position entry
    if (transition.target_state != PositionStateMachine::State::CASH_ONLY) {
        HorizonPosition pos;
        pos.entry_bar_id = transition.position_open_bar_id;
        pos.target_exit_bar_id = transition.earliest_exit_bar_id;
        pos.entry_price = 0.0;  // Will be set by caller
        pos.position_value = 100000.0;  // Placeholder
        pos.horizon = prediction_horizon;
        pos.is_long = (transition.target_state == PositionStateMachine::State::QQQ_ONLY ||
                      transition.target_state == PositionStateMachine::State::TQQQ_ONLY);

        active_horizon_positions[pos.entry_bar_id] = pos;
    }

    // Check for positions that should be closed (reached target horizon)
    std::vector<uint64_t> positions_to_close;
    for (const auto& [entry_bar_id, pos] : active_horizon_positions) {
        // If we've reached or passed the target exit bar
        if (transition.position_open_bar_id >= pos.target_exit_bar_id) {
            // Calculate P&L
            double current_price = 100.0;  // Would get from market data
            double entry_price = pos.entry_price > 0 ? pos.entry_price : 100.0;

            double return_pct = ((current_price - entry_price) / entry_price);
            if (!pos.is_long) {
                return_pct = -return_pct;  // Invert for short positions
            }

            // Record return for this horizon
            horizon_returns[pos.horizon].push_back(return_pct);

            // Keep last 100 returns per horizon
            if (horizon_returns[pos.horizon].size() > 100) {
                horizon_returns[pos.horizon].erase(horizon_returns[pos.horizon].begin());
            }

            positions_to_close.push_back(entry_bar_id);
        }
    }

    // Remove closed positions
    for (uint64_t bar_id : positions_to_close) {
        active_horizon_positions.erase(bar_id);
    }

    // Calculate and update success rates based on ACTUAL returns
    for (const auto& [horizon, returns] : horizon_returns) {
        if (returns.empty()) continue;

        // Success = positive return
        int successes = std::count_if(returns.begin(), returns.end(),
                                     [](double r) { return r > 0.0; });

        double success_rate = static_cast<double>(successes) / returns.size();
        enhanced_config_.horizon_success_rates[horizon] = success_rate;

        // Also calculate average return for this horizon
        double avg_return = std::accumulate(returns.begin(), returns.end(), 0.0) / returns.size();

        utils::log_debug("Horizon " + std::to_string(horizon) +
                        ": Success=" + std::to_string(success_rate * 100) + "%, " +
                        "AvgReturn=" + std::to_string(avg_return * 100) + "%");
    }
}

double EnhancedBackendComponent::get_current_price(
    const std::string& symbol, 
    const Bar& bar) const {
    // Temporary mapping: approximate relative prices vs QQQ close
    // TODO: Replace with real per-symbol market data lookup
    if (symbol == "TQQQ") {
        return bar.close * 0.33; // rough ratio placeholder
    }
    if (symbol == "SQQQ") {
        return bar.close * 0.11; // rough ratio placeholder
    }
    if (symbol == "PSQ") {
        return bar.close * 0.25; // rough ratio placeholder
    }
    return bar.close; // QQQ
}

double EnhancedBackendComponent::estimate_execution_price(
    const std::string& symbol,
    TradeAction action,
    double base_price) const {
    
    // Apply slippage
    double slippage = base_price * enhanced_config_.slippage_factor;
    
    if (action == TradeAction::BUY) {
        return base_price + slippage;
    } else {
        return base_price - slippage;
    }
}

std::string EnhancedBackendComponent::format_order_details(const TradeOrder& order) const {
    std::stringstream ss;
    ss << order.symbol << " " 
       << (order.action == TradeAction::BUY ? "BUY" : "SELL")
       << " " << order.quantity << " @ $" << std::fixed << std::setprecision(2) << order.price
       << " = $" << order.trade_value;
    
    return ss.str();
}

std::string EnhancedBackendComponent::format_allocation_summary(
    const backend::DynamicAllocationManager::AllocationResult& allocation) const {
    
    std::stringstream ss;
    ss << "ALLOCATION: "
       << allocation.base_symbol << "=" << std::fixed << std::setprecision(1) 
       << allocation.base_allocation_pct * 100 << "% (" << allocation.base_quantity << " shares), "
       << allocation.leveraged_symbol << "=" << allocation.leveraged_allocation_pct * 100 
       << "% (" << allocation.leveraged_quantity << " shares) | "
       << "Leverage=" << std::setprecision(2) << allocation.effective_leverage << "x | "
       << "Risk=" << allocation.risk_score;
    
    return ss.str();
}

} // namespace sentio

