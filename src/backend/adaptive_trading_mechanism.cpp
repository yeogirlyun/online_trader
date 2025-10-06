#include "backend/adaptive_trading_mechanism.h"
#include "common/utils.h"
#include <numeric>
#include <filesystem>

namespace sentio {

// ===================================================================
// MARKET REGIME DETECTOR IMPLEMENTATION
// ===================================================================

MarketState MarketRegimeDetector::analyze_market_state(const Bar& current_bar, 
                                                      const std::vector<Bar>& recent_history,
                                                      const SignalOutput& signal) {
    MarketState state;
    
    // Update price history
    price_history_.push_back(current_bar.close);
    if (price_history_.size() > LOOKBACK_PERIOD) {
        price_history_.erase(price_history_.begin());
    }
    
    // Update volume history
    volume_history_.push_back(current_bar.volume);
    if (volume_history_.size() > LOOKBACK_PERIOD) {
        volume_history_.erase(volume_history_.begin());
    }
    
    // Calculate market metrics
    state.current_price = current_bar.close;
    state.volatility = calculate_volatility();
    state.trend_strength = calculate_trend_strength();
    state.volume_ratio = calculate_volume_ratio();
    state.regime = classify_market_regime(state.volatility, state.trend_strength);
    
    // Signal statistics
    state.avg_signal_strength = std::abs(signal.probability - 0.5) * 2.0;
    
    utils::log_debug("Market Analysis: Price=" + std::to_string(state.current_price) + 
                    ", Vol=" + std::to_string(state.volatility) + 
                    ", Trend=" + std::to_string(state.trend_strength) + 
                    ", Regime=" + std::to_string(static_cast<int>(state.regime)));
    
    return state;
}

double MarketRegimeDetector::calculate_volatility() {
    if (price_history_.size() < 2) return 0.1; // Default volatility
    
    std::vector<double> returns;
    for (size_t i = 1; i < price_history_.size(); ++i) {
        double ret = std::log(price_history_[i] / price_history_[i-1]);
        returns.push_back(ret);
    }
    
    // Calculate standard deviation of returns
    double mean = std::accumulate(returns.begin(), returns.end(), 0.0) / returns.size();
    double sq_sum = 0.0;
    for (double ret : returns) {
        sq_sum += (ret - mean) * (ret - mean);
    }
    
    return std::sqrt(sq_sum / returns.size()) * std::sqrt(252); // Annualized
}

double MarketRegimeDetector::calculate_trend_strength() {
    if (price_history_.size() < 10) return 0.0;
    
    // Linear regression slope over recent prices
    double n = static_cast<double>(price_history_.size());
    double sum_x = n * (n - 1) / 2;
    double sum_y = std::accumulate(price_history_.begin(), price_history_.end(), 0.0);
    double sum_xy = 0.0;
    double sum_x2 = n * (n - 1) * (2 * n - 1) / 6;
    
    for (size_t i = 0; i < price_history_.size(); ++i) {
        sum_xy += static_cast<double>(i) * price_history_[i];
    }
    
    double slope = (n * sum_xy - sum_x * sum_y) / (n * sum_x2 - sum_x * sum_x);
    
    // Normalize slope to [-1, 1] range
    double price_range = *std::max_element(price_history_.begin(), price_history_.end()) -
                        *std::min_element(price_history_.begin(), price_history_.end());
    
    if (price_range > 0) {
        return std::clamp(slope / price_range * 100, -1.0, 1.0);
    }
    
    return 0.0;
}

double MarketRegimeDetector::calculate_volume_ratio() {
    if (volume_history_.empty()) return 1.0;
    
    double current_volume = volume_history_.back();
    double avg_volume = std::accumulate(volume_history_.begin(), volume_history_.end(), 0.0) / volume_history_.size();
    
    return (avg_volume > 0) ? current_volume / avg_volume : 1.0;
}

MarketRegime MarketRegimeDetector::classify_market_regime(double volatility, double trend_strength) {
    bool high_vol = volatility > 0.25; // 25% annualized volatility threshold
    
    if (trend_strength > 0.3) {
        return high_vol ? MarketRegime::BULL_HIGH_VOL : MarketRegime::BULL_LOW_VOL;
    } else if (trend_strength < -0.3) {
        return high_vol ? MarketRegime::BEAR_HIGH_VOL : MarketRegime::BEAR_LOW_VOL;
    } else {
        return high_vol ? MarketRegime::SIDEWAYS_HIGH_VOL : MarketRegime::SIDEWAYS_LOW_VOL;
    }
}

// ===================================================================
// PERFORMANCE EVALUATOR IMPLEMENTATION
// ===================================================================

void PerformanceEvaluator::add_trade_outcome(const TradeOutcome& outcome) {
    trade_history_.push_back(outcome);
    
    // Maintain rolling window
    if (trade_history_.size() > MAX_HISTORY) {
        trade_history_.erase(trade_history_.begin());
    }
    
    utils::log_debug("Trade outcome added: PnL=" + std::to_string(outcome.actual_pnl) + 
                    ", Profitable=" + (outcome.was_profitable ? "YES" : "NO"));
}

void PerformanceEvaluator::add_portfolio_value(double value) {
    portfolio_values_.push_back(value);
    
    if (portfolio_values_.size() > MAX_HISTORY) {
        portfolio_values_.erase(portfolio_values_.begin());
    }
}

PerformanceMetrics PerformanceEvaluator::calculate_performance_metrics() {
    PerformanceMetrics metrics;
    
    if (trade_history_.empty()) {
        return metrics;
    }
    
    // Get recent trades for analysis
    size_t start_idx = trade_history_.size() > PERFORMANCE_WINDOW ? 
                      trade_history_.size() - PERFORMANCE_WINDOW : 0;
    
    std::vector<TradeOutcome> recent_trades(
        trade_history_.begin() + start_idx, trade_history_.end());
    
    // Calculate basic metrics
    metrics.total_trades = static_cast<int>(recent_trades.size());
    metrics.winning_trades = 0;
    metrics.losing_trades = 0;
    metrics.gross_profit = 0.0;
    metrics.gross_loss = 0.0;
    
    for (const auto& trade : recent_trades) {
        if (trade.was_profitable) {
            metrics.winning_trades++;
            metrics.gross_profit += trade.actual_pnl;
        } else {
            metrics.losing_trades++;
            metrics.gross_loss += std::abs(trade.actual_pnl);
        }
        
        metrics.returns.push_back(trade.pnl_percentage);
    }
    
    // Calculate derived metrics
    metrics.win_rate = metrics.total_trades > 0 ? 
                      static_cast<double>(metrics.winning_trades) / metrics.total_trades : 0.0;
    
    metrics.profit_factor = metrics.gross_loss > 0 ? 
                           metrics.gross_profit / metrics.gross_loss : 1.0;
    
    metrics.sharpe_ratio = calculate_sharpe_ratio(metrics.returns);
    metrics.max_drawdown = calculate_max_drawdown();
    metrics.capital_efficiency = calculate_capital_efficiency();
    
    return metrics;
}

double PerformanceEvaluator::calculate_reward_signal(const PerformanceMetrics& metrics) {
    // Multi-objective reward function
    double profit_component = metrics.gross_profit - metrics.gross_loss;
    double risk_component = metrics.sharpe_ratio * 0.5;
    double drawdown_penalty = metrics.max_drawdown * -2.0;
    double overtrading_penalty = std::max(0.0, metrics.trade_frequency - 10.0) * -0.1;
    
    double total_reward = profit_component + risk_component + drawdown_penalty + overtrading_penalty;
    
    utils::log_debug("Reward calculation: Profit=" + std::to_string(profit_component) + 
                    ", Risk=" + std::to_string(risk_component) + 
                    ", Drawdown=" + std::to_string(drawdown_penalty) + 
                    ", Total=" + std::to_string(total_reward));
    
    return total_reward;
}

double PerformanceEvaluator::calculate_sharpe_ratio(const std::vector<double>& returns) {
    if (returns.size() < 2) return 0.0;
    
    double mean_return = std::accumulate(returns.begin(), returns.end(), 0.0) / returns.size();
    
    double variance = 0.0;
    for (double ret : returns) {
        variance += (ret - mean_return) * (ret - mean_return);
    }
    variance /= returns.size();
    
    double std_dev = std::sqrt(variance);
    return std_dev > 0 ? mean_return / std_dev : 0.0;
}

double PerformanceEvaluator::calculate_max_drawdown() {
    if (portfolio_values_.size() < 2) return 0.0;
    
    double peak = portfolio_values_[0];
    double max_dd = 0.0;
    
    for (double value : portfolio_values_) {
        if (value > peak) {
            peak = value;
        }
        
        double drawdown = (peak - value) / peak;
        max_dd = std::max(max_dd, drawdown);
    }
    
    return max_dd;
}

double PerformanceEvaluator::calculate_capital_efficiency() {
    if (portfolio_values_.size() < 2) return 0.0;
    
    double initial_value = portfolio_values_.front();
    double final_value = portfolio_values_.back();
    
    return initial_value > 0 ? (final_value - initial_value) / initial_value : 0.0;
}

// ===================================================================
// Q-LEARNING THRESHOLD OPTIMIZER IMPLEMENTATION
// ===================================================================

QLearningThresholdOptimizer::QLearningThresholdOptimizer() 
    : rng_(std::chrono::steady_clock::now().time_since_epoch().count()) {
    utils::log_info("Q-Learning Threshold Optimizer initialized with learning_rate=" + 
                   std::to_string(learning_rate_) + ", exploration_rate=" + std::to_string(exploration_rate_));
}

ThresholdAction QLearningThresholdOptimizer::select_action(const MarketState& state, 
                                                          const ThresholdPair& current_thresholds,
                                                          const PerformanceMetrics& performance) {
    int state_hash = discretize_state(state, current_thresholds, performance);
    
    // Epsilon-greedy action selection
    std::uniform_real_distribution<double> dis(0.0, 1.0);
    
    if (dis(rng_) < exploration_rate_) {
        // Explore: random action
        std::uniform_int_distribution<int> action_dis(0, static_cast<int>(ThresholdAction::COUNT) - 1);
        ThresholdAction action = static_cast<ThresholdAction>(action_dis(rng_));
        utils::log_debug("Q-Learning: EXPLORE action=" + std::to_string(static_cast<int>(action)));
        return action;
    } else {
        // Exploit: best known action
        ThresholdAction action = get_best_action(state_hash);
        utils::log_debug("Q-Learning: EXPLOIT action=" + std::to_string(static_cast<int>(action)));
        return action;
    }
}

void QLearningThresholdOptimizer::update_q_value(const MarketState& prev_state,
                                                 const ThresholdPair& prev_thresholds,
                                                 const PerformanceMetrics& prev_performance,
                                                 ThresholdAction action,
                                                 double reward,
                                                 const MarketState& new_state,
                                                 const ThresholdPair& new_thresholds,
                                                 const PerformanceMetrics& new_performance) {
    
    int prev_state_hash = discretize_state(prev_state, prev_thresholds, prev_performance);
    int new_state_hash = discretize_state(new_state, new_thresholds, new_performance);
    
    StateActionPair sa_pair{prev_state_hash, static_cast<int>(action)};
    
    // Get current Q-value
    double current_q = get_q_value(sa_pair);
    
    // Get maximum Q-value for next state
    double max_next_q = get_max_q_value(new_state_hash);
    
    // Q-learning update
    double target = reward + discount_factor_ * max_next_q;
    double new_q = current_q + learning_rate_ * (target - current_q);
    
    q_table_[sa_pair] = new_q;
    state_visit_count_[prev_state_hash]++;
    
    // Decay exploration rate
    exploration_rate_ = std::max(min_exploration_, exploration_rate_ * exploration_decay_);
    
    utils::log_debug("Q-Learning update: State=" + std::to_string(prev_state_hash) + 
                    ", Action=" + std::to_string(static_cast<int>(action)) + 
                    ", Reward=" + std::to_string(reward) + 
                    ", Q_old=" + std::to_string(current_q) + 
                    ", Q_new=" + std::to_string(new_q));
}

ThresholdPair QLearningThresholdOptimizer::apply_action(const ThresholdPair& current_thresholds, ThresholdAction action) {
    ThresholdPair new_thresholds = current_thresholds;
    
    switch (action) {
        case ThresholdAction::INCREASE_BUY_SMALL:
            new_thresholds.buy_threshold += 0.01;
            break;
        case ThresholdAction::INCREASE_BUY_MEDIUM:
            new_thresholds.buy_threshold += 0.03;
            break;
        case ThresholdAction::DECREASE_BUY_SMALL:
            new_thresholds.buy_threshold -= 0.01;
            break;
        case ThresholdAction::DECREASE_BUY_MEDIUM:
            new_thresholds.buy_threshold -= 0.03;
            break;
        case ThresholdAction::INCREASE_SELL_SMALL:
            new_thresholds.sell_threshold += 0.01;
            break;
        case ThresholdAction::INCREASE_SELL_MEDIUM:
            new_thresholds.sell_threshold += 0.03;
            break;
        case ThresholdAction::DECREASE_SELL_SMALL:
            new_thresholds.sell_threshold -= 0.01;
            break;
        case ThresholdAction::DECREASE_SELL_MEDIUM:
            new_thresholds.sell_threshold -= 0.03;
            break;
        case ThresholdAction::MAINTAIN_THRESHOLDS:
        default:
            // No change
            break;
    }
    
    // Ensure thresholds remain valid
    new_thresholds.buy_threshold = std::clamp(new_thresholds.buy_threshold, 0.51, 0.90);
    new_thresholds.sell_threshold = std::clamp(new_thresholds.sell_threshold, 0.10, 0.49);
    
    // Ensure minimum gap
    if (new_thresholds.buy_threshold - new_thresholds.sell_threshold < 0.05) {
        new_thresholds.buy_threshold = new_thresholds.sell_threshold + 0.05;
        new_thresholds.buy_threshold = std::min(new_thresholds.buy_threshold, 0.90);
    }
    
    return new_thresholds;
}

double QLearningThresholdOptimizer::get_learning_progress() const {
    return 1.0 - exploration_rate_;
}

int QLearningThresholdOptimizer::discretize_state(const MarketState& state, 
                                                 const ThresholdPair& thresholds,
                                                 const PerformanceMetrics& performance) {
    // Create a hash of the discretized state
    int buy_bin = static_cast<int>((thresholds.buy_threshold - 0.5) / 0.4 * THRESHOLD_BINS);
    int sell_bin = static_cast<int>((thresholds.sell_threshold - 0.1) / 0.4 * THRESHOLD_BINS);
    int vol_bin = static_cast<int>(std::min(state.volatility / 0.5, 1.0) * 5);
    int trend_bin = static_cast<int>((state.trend_strength + 1.0) / 2.0 * 5);
    int perf_bin = static_cast<int>(std::clamp(performance.win_rate, 0.0, 1.0) * PERFORMANCE_BINS);
    
    // Combine bins into a single hash
    return buy_bin * 10000 + sell_bin * 1000 + vol_bin * 100 + trend_bin * 10 + perf_bin;
}

double QLearningThresholdOptimizer::get_q_value(const StateActionPair& sa_pair) {
    auto it = q_table_.find(sa_pair);
    return (it != q_table_.end()) ? it->second : 0.0; // Optimistic initialization
}

double QLearningThresholdOptimizer::get_max_q_value(int state_hash) {
    double max_q = 0.0;
    
    for (int action = 0; action < static_cast<int>(ThresholdAction::COUNT); ++action) {
        StateActionPair sa_pair{state_hash, action};
        max_q = std::max(max_q, get_q_value(sa_pair));
    }
    
    return max_q;
}

ThresholdAction QLearningThresholdOptimizer::get_best_action(int state_hash) {
    ThresholdAction best_action = ThresholdAction::MAINTAIN_THRESHOLDS;
    double best_q = get_q_value({state_hash, static_cast<int>(best_action)});
    
    for (int action = 0; action < static_cast<int>(ThresholdAction::COUNT); ++action) {
        StateActionPair sa_pair{state_hash, action};
        double q_val = get_q_value(sa_pair);
        
        if (q_val > best_q) {
            best_q = q_val;
            best_action = static_cast<ThresholdAction>(action);
        }
    }
    
    return best_action;
}

// ===================================================================
// MULTI-ARMED BANDIT OPTIMIZER IMPLEMENTATION
// ===================================================================

MultiArmedBanditOptimizer::MultiArmedBanditOptimizer() 
    : rng_(std::chrono::steady_clock::now().time_since_epoch().count()) {
    initialize_arms();
    utils::log_info("Multi-Armed Bandit Optimizer initialized with " + std::to_string(arms_.size()) + " arms");
}

ThresholdPair MultiArmedBanditOptimizer::select_thresholds() {
    if (arms_.empty()) {
        return ThresholdPair(); // Default thresholds
    }
    
    // UCB1 algorithm
    update_confidence_bounds();
    
    auto best_arm = std::max_element(arms_.begin(), arms_.end(),
        [](const BanditArm& a, const BanditArm& b) {
            return (a.estimated_reward + a.confidence_bound) < 
                   (b.estimated_reward + b.confidence_bound);
        });
    
    utils::log_debug("Bandit selected: Buy=" + std::to_string(best_arm->thresholds.buy_threshold) + 
                    ", Sell=" + std::to_string(best_arm->thresholds.sell_threshold) + 
                    ", UCB=" + std::to_string(best_arm->estimated_reward + best_arm->confidence_bound));
    
    return best_arm->thresholds;
}

void MultiArmedBanditOptimizer::update_reward(const ThresholdPair& thresholds, double reward) {
    // Find the arm that was pulled
    auto arm_it = std::find_if(arms_.begin(), arms_.end(),
        [&thresholds](const BanditArm& arm) {
            return std::abs(arm.thresholds.buy_threshold - thresholds.buy_threshold) < 0.005 &&
                   std::abs(arm.thresholds.sell_threshold - thresholds.sell_threshold) < 0.005;
        });
    
    if (arm_it != arms_.end()) {
        // Update arm's estimated reward using incremental mean
        arm_it->pull_count++;
        total_pulls_++;
        
        double old_estimate = arm_it->estimated_reward;
        arm_it->estimated_reward = old_estimate + (reward - old_estimate) / arm_it->pull_count;
        
        utils::log_debug("Bandit reward update: Buy=" + std::to_string(thresholds.buy_threshold) + 
                        ", Sell=" + std::to_string(thresholds.sell_threshold) + 
                        ", Reward=" + std::to_string(reward) + 
                        ", New_Est=" + std::to_string(arm_it->estimated_reward));
    }
}

void MultiArmedBanditOptimizer::initialize_arms() {
    // Create a grid of threshold combinations
    for (double buy = 0.55; buy <= 0.85; buy += 0.05) {
        for (double sell = 0.15; sell <= 0.45; sell += 0.05) {
            if (buy > sell + 0.05) { // Ensure minimum gap
                arms_.emplace_back(ThresholdPair(buy, sell));
            }
        }
    }
}

void MultiArmedBanditOptimizer::update_confidence_bounds() {
    for (auto& arm : arms_) {
        if (arm.pull_count == 0) {
            arm.confidence_bound = std::numeric_limits<double>::max();
        } else {
            arm.confidence_bound = std::sqrt(2.0 * std::log(total_pulls_) / arm.pull_count);
        }
    }
}

// ===================================================================
// ADAPTIVE THRESHOLD MANAGER IMPLEMENTATION
// ===================================================================

AdaptiveThresholdManager::AdaptiveThresholdManager(const AdaptiveConfig& config) 
    : config_(config), current_thresholds_(0.55, 0.45) {
    
    // Initialize components
    q_learner_ = std::make_unique<QLearningThresholdOptimizer>();
    bandit_optimizer_ = std::make_unique<MultiArmedBanditOptimizer>();
    regime_detector_ = std::make_unique<MarketRegimeDetector>();
    performance_evaluator_ = std::make_unique<PerformanceEvaluator>();
    
    // Initialize regime-specific thresholds
    initialize_regime_thresholds();
    
    utils::log_info("AdaptiveThresholdManager initialized: Algorithm=" + 
                   std::to_string(static_cast<int>(config_.algorithm)) + 
                   ", LearningRate=" + std::to_string(config_.learning_rate) + 
                   ", ConservativeMode=" + (config_.conservative_mode ? "YES" : "NO"));
}

ThresholdPair AdaptiveThresholdManager::get_current_thresholds(const SignalOutput& signal, const Bar& bar) {
    // Update market state
    current_market_state_ = regime_detector_->analyze_market_state(bar, recent_bars_, signal);
    recent_bars_.push_back(bar);
    if (recent_bars_.size() > 100) {
        recent_bars_.erase(recent_bars_.begin());
    }
    
    // Check circuit breaker
    if (circuit_breaker_active_) {
        utils::log_warning("Circuit breaker active - using conservative thresholds");
        return get_conservative_thresholds();
    }
    
    // Update performance and potentially adjust thresholds
    update_performance_and_learn();
    
    // Get regime-adapted thresholds if enabled
    if (config_.enable_regime_adaptation) {
        return get_regime_adapted_thresholds();
    }
    
    return current_thresholds_;
}

void AdaptiveThresholdManager::process_trade_outcome(const std::string& symbol, TradeAction action, 
                                                    double quantity, double price, double trade_value, double fees,
                                                    double actual_pnl, double pnl_percentage, bool was_profitable) {
    TradeOutcome outcome;
    outcome.symbol = symbol;
    outcome.action = action;
    outcome.quantity = quantity;
    outcome.price = price;
    outcome.trade_value = trade_value;
    outcome.fees = fees;
    outcome.actual_pnl = actual_pnl;
    outcome.pnl_percentage = pnl_percentage;
    outcome.was_profitable = was_profitable;
    outcome.outcome_timestamp = std::chrono::system_clock::now();
    
    performance_evaluator_->add_trade_outcome(outcome);
    
    // Update learning algorithms with reward feedback
    if (learning_enabled_) {
        current_performance_ = performance_evaluator_->calculate_performance_metrics();
        double reward = performance_evaluator_->calculate_reward_signal(current_performance_);
        
        // Update based on algorithm type
        switch (config_.algorithm) {
            case LearningAlgorithm::Q_LEARNING:
                // Q-learning update will happen in next call to update_performance_and_learn()
                break;
                
            case LearningAlgorithm::MULTI_ARMED_BANDIT:
                bandit_optimizer_->update_reward(current_thresholds_, reward);
                break;
                
            case LearningAlgorithm::ENSEMBLE:
                // Update both algorithms
                bandit_optimizer_->update_reward(current_thresholds_, reward);
                break;
        }
    }
    
    // Check for circuit breaker conditions
    check_circuit_breaker();
}

void AdaptiveThresholdManager::update_portfolio_value(double value) {
    performance_evaluator_->add_portfolio_value(value);
}

double AdaptiveThresholdManager::get_learning_progress() const {
    return q_learner_->get_learning_progress();
}

std::string AdaptiveThresholdManager::generate_performance_report() const {
    std::ostringstream report;
    
    report << "=== ADAPTIVE TRADING PERFORMANCE REPORT ===\n";
    report << "Current Thresholds: Buy=" << std::fixed << std::setprecision(3) << current_thresholds_.buy_threshold 
           << ", Sell=" << current_thresholds_.sell_threshold << "\n";
    report << "Market Regime: " << static_cast<int>(current_market_state_.regime) << "\n";
    report << "Total Trades: " << current_performance_.total_trades << "\n";
    report << "Win Rate: " << std::fixed << std::setprecision(1) << (current_performance_.win_rate * 100) << "%\n";
    report << "Profit Factor: " << std::fixed << std::setprecision(2) << current_performance_.profit_factor << "\n";
    report << "Sharpe Ratio: " << std::fixed << std::setprecision(2) << current_performance_.sharpe_ratio << "\n";
    report << "Max Drawdown: " << std::fixed << std::setprecision(1) << (current_performance_.max_drawdown * 100) << "%\n";
    report << "Learning Progress: " << std::fixed << std::setprecision(1) << (get_learning_progress() * 100) << "%\n";
    report << "Circuit Breaker: " << (circuit_breaker_active_ ? "ACTIVE" : "INACTIVE") << "\n";
    
    return report.str();
}

void AdaptiveThresholdManager::initialize_regime_thresholds() {
    // Conservative thresholds for volatile markets
    regime_thresholds_[MarketRegime::BULL_HIGH_VOL] = ThresholdPair(0.65, 0.35);
    regime_thresholds_[MarketRegime::BEAR_HIGH_VOL] = ThresholdPair(0.70, 0.30);
    regime_thresholds_[MarketRegime::SIDEWAYS_HIGH_VOL] = ThresholdPair(0.68, 0.32);
    
    // More aggressive thresholds for stable markets
    regime_thresholds_[MarketRegime::BULL_LOW_VOL] = ThresholdPair(0.58, 0.42);
    regime_thresholds_[MarketRegime::BEAR_LOW_VOL] = ThresholdPair(0.62, 0.38);
    regime_thresholds_[MarketRegime::SIDEWAYS_LOW_VOL] = ThresholdPair(0.60, 0.40);
}

void AdaptiveThresholdManager::update_performance_and_learn() {
    if (!learning_enabled_ || circuit_breaker_active_) {
        return;
    }
    
    // Update performance metrics
    PerformanceMetrics new_performance = performance_evaluator_->calculate_performance_metrics();
    
    // Only learn if we have enough data
    if (new_performance.total_trades < config_.performance_window / 2) {
        return;
    }
    
    // Q-Learning update
    if (config_.algorithm == LearningAlgorithm::Q_LEARNING || 
        config_.algorithm == LearningAlgorithm::ENSEMBLE) {
        
        double reward = performance_evaluator_->calculate_reward_signal(new_performance);
        
        // Select and apply action
        ThresholdAction action = q_learner_->select_action(
            current_market_state_, current_thresholds_, current_performance_);
        
        ThresholdPair new_thresholds = q_learner_->apply_action(current_thresholds_, action);
        
        // Update Q-values if we have previous state
        if (current_performance_.total_trades > 0) {
            q_learner_->update_q_value(
                current_market_state_, current_thresholds_, current_performance_,
                action, reward,
                current_market_state_, new_thresholds, new_performance);
        }
        
        current_thresholds_ = new_thresholds;
    }
    
    // Multi-Armed Bandit update
    if (config_.algorithm == LearningAlgorithm::MULTI_ARMED_BANDIT || 
        config_.algorithm == LearningAlgorithm::ENSEMBLE) {
        
        current_thresholds_ = bandit_optimizer_->select_thresholds();
    }
    
    current_performance_ = new_performance;
}

ThresholdPair AdaptiveThresholdManager::get_regime_adapted_thresholds() {
    auto regime_it = regime_thresholds_.find(current_market_state_.regime);
    if (regime_it != regime_thresholds_.end()) {
        // Blend learned thresholds with regime-specific ones
        ThresholdPair regime_thresholds = regime_it->second;
        double blend_factor = config_.conservative_mode ? 0.7 : 0.3;
        
        return ThresholdPair(
            current_thresholds_.buy_threshold * (1.0 - blend_factor) + 
            regime_thresholds.buy_threshold * blend_factor,
            current_thresholds_.sell_threshold * (1.0 - blend_factor) + 
            regime_thresholds.sell_threshold * blend_factor
        );
    }
    
    return current_thresholds_;
}

ThresholdPair AdaptiveThresholdManager::get_conservative_thresholds() {
    // Return very conservative thresholds during circuit breaker
    return ThresholdPair(0.75, 0.25);
}

void AdaptiveThresholdManager::check_circuit_breaker() {
    // Only activate circuit breaker if we have sufficient trading history
    if (current_performance_.total_trades < 10) {
        return; // Not enough data to make circuit breaker decisions
    }
    
    if (current_performance_.max_drawdown > config_.max_drawdown_limit ||
        current_performance_.win_rate < 0.3 ||
        (current_performance_.total_trades > 20 && current_performance_.profit_factor < 0.8)) {
        
        circuit_breaker_active_ = true;
        learning_enabled_ = false;
        
        utils::log_error("CIRCUIT BREAKER ACTIVATED: Drawdown=" + std::to_string(current_performance_.max_drawdown) + 
                        ", WinRate=" + std::to_string(current_performance_.win_rate) + 
                        ", ProfitFactor=" + std::to_string(current_performance_.profit_factor));
    }
}

} // namespace sentio
