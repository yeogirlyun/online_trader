#pragma once

#include <stdexcept>
#include <string>
#include <vector>
#include <cmath>
#include "common/types.h"
#include "backend/backend_component.h"

namespace sentio::validation {

/**
 * @brief Performance validation system to detect unrealistic trading results
 * 
 * This class provides critical safety checks to prevent the reporting of
 * impossible or artificial trading performance metrics. It serves as a
 * safeguard against bugs in trade execution or calculation logic.
 */
class PerformanceValidator {
public:
    
    /**
     * @brief Validates that MRB (Mean Return per Block) is within realistic bounds
     * 
     * A 5% return per block would translate to astronomical annual returns
     * (5% × 20 blocks/month × 12 months = 1200% annually), which indicates
     * artificial or buggy trade generation.
     * 
     * @param mrb_percentage The MRB value as a percentage (e.g., 1.5 for 1.5%)
     * @throws std::runtime_error if MRB is suspiciously high or low
     */
    static void validate_mrb(double mrb_percentage) {
        const double SUSPICIOUS_HIGH_MRB = 5.0;  // 5% per block is extremely suspicious
        const double SUSPICIOUS_LOW_MRB = -5.0;  // -5% per block indicates major losses
        
        if (mrb_percentage > SUSPICIOUS_HIGH_MRB) {
            throw std::runtime_error(
                "VALIDATION ERROR: MRB is suspiciously high at " +
                std::to_string(mrb_percentage) + "%. " +
                "This would translate to " + std::to_string(mrb_percentage * 240) + "% annually. " +
                "Results are likely invalid due to artificial trade generation."
            );
        }
        
        if (mrb_percentage < SUSPICIOUS_LOW_MRB) {
            throw std::runtime_error(
                "VALIDATION ERROR: MRB shows extreme losses at " +
                std::to_string(mrb_percentage) + "%. " +
                "This indicates potential bugs in trade execution or calculation logic."
            );
        }
    }
    
    /**
     * @brief Validates total return percentage against realistic bounds
     * 
     * @param total_return_pct Total return as percentage (e.g., 150.0 for 150%)
     * @param time_period_blocks Number of blocks the return was achieved over
     */
    static void validate_total_return(double total_return_pct, int time_period_blocks) {
        // Calculate annualized return assuming 20 blocks per month
        double blocks_per_year = 20 * 12; // 240 blocks per year
        double annualized_return = total_return_pct * (blocks_per_year / time_period_blocks);
        
        const double SUSPICIOUS_ANNUAL_RETURN = 1000.0; // 1000% annually is highly suspicious
        
        if (annualized_return > SUSPICIOUS_ANNUAL_RETURN) {
            throw std::runtime_error(
                "VALIDATION ERROR: Annualized return of " +
                std::to_string(annualized_return) + "% is unrealistic. " +
                "Total return: " + std::to_string(total_return_pct) + "% over " +
                std::to_string(time_period_blocks) + " blocks."
            );
        }
    }
    
    /**
     * @brief Checks a sample of trades for signs of artificial, hardcoded values
     * 
     * This method detects the specific pattern from the bug report:
     * - Fixed prices of exactly $100.00
     * - Fixed quantities of exactly 100 shares
     * - Artificial equity progressions
     * 
     * @param trades Vector of trade records to validate
     * @throws std::runtime_error if artificial patterns are detected
     */
    static void validate_trade_realism(const std::vector<sentio::BackendComponent::TradeOrder>& trades) {
        if (trades.empty()) return;
        
        int artificial_count = 0;
        int fixed_price_count = 0;
        int fixed_quantity_count = 0;
        const size_t sample_size = std::min(trades.size(), static_cast<size_t>(50)); // Check larger sample
        
        for (size_t i = 0; i < sample_size; ++i) {
            const auto& trade = trades[i];
            
            // Check for exact hardcoded values from the bug report
            bool is_fixed_price = (std::abs(trade.price - 100.0) < 0.01); // Exactly $100.00
            bool is_fixed_quantity = (std::abs(trade.quantity - 100.0) < 0.01); // Exactly 100 shares
            
            if (is_fixed_price) fixed_price_count++;
            if (is_fixed_quantity) fixed_quantity_count++;
            if (is_fixed_price && is_fixed_quantity) artificial_count++;
        }
        
        // If more than 80% of trades have fixed prices, it's artificial
        if (fixed_price_count > (sample_size * 0.8)) {
            throw std::runtime_error(
                "VALIDATION ERROR: " + std::to_string(fixed_price_count) + " out of " +
                std::to_string(sample_size) + " trades have fixed price of $100.00. " +
                "This indicates artificial trade generation instead of real market data."
            );
        }
        
        // If more than 80% of trades have fixed quantities, it's artificial
        if (fixed_quantity_count > (sample_size * 0.8)) {
            throw std::runtime_error(
                "VALIDATION ERROR: " + std::to_string(fixed_quantity_count) + " out of " +
                std::to_string(sample_size) + " trades have fixed quantity of 100 shares. " +
                "This indicates artificial position sizing instead of realistic portfolio management."
            );
        }
        
        // If more than 50% of trades match the exact artificial pattern, it's definitely fake
        if (artificial_count > (sample_size / 2)) {
            throw std::runtime_error(
                "VALIDATION ERROR: " + std::to_string(artificial_count) + " out of " +
                std::to_string(sample_size) + " trades appear completely artificial " +
                "(fixed price $100.00 AND fixed quantity 100). " +
                "This matches the known bug pattern in process_signals_to_trades()."
            );
        }
    }
    
    /**
     * @brief Validates that equity progression is realistic
     * 
     * Checks for patterns like fixed $100 gains per trade that indicate
     * artificial trade generation.
     * 
     * @param equity_values Vector of equity values over time
     */
    static void validate_equity_progression(const std::vector<double>& equity_values) {
        if (equity_values.size() < 10) return; // Need sufficient data
        
        // Check for suspiciously regular patterns
        std::vector<double> deltas;
        for (size_t i = 1; i < equity_values.size(); ++i) {
            deltas.push_back(equity_values[i] - equity_values[i-1]);
        }
        
        // Count how many deltas are exactly $100 or $-50 (the artificial pattern)
        int fixed_gain_count = 0;
        int fixed_loss_count = 0;
        
        for (double delta : deltas) {
            if (std::abs(delta - 100.0) < 0.01) fixed_gain_count++;
            if (std::abs(delta + 50.0) < 0.01) fixed_loss_count++;
        }
        
        double total_deltas = deltas.size();
        if ((fixed_gain_count + fixed_loss_count) > (total_deltas * 0.7)) {
            throw std::runtime_error(
                "VALIDATION ERROR: Equity progression shows artificial pattern. " +
                std::to_string(fixed_gain_count) + " trades with exactly +$100 gain, " +
                std::to_string(fixed_loss_count) + " trades with exactly -$50 loss. " +
                "This matches the artificial trade generation bug pattern."
            );
        }
    }
    
    /**
     * @brief Comprehensive validation of all performance metrics
     * 
     * This is the main validation method that should be called after
     * calculating performance metrics to ensure they are realistic.
     * 
     * @param mrb_pct Mean Return per Block as percentage
     * @param total_return_pct Total return as percentage  
     * @param blocks Number of blocks in the test period
     * @param trades Vector of trade records
     * @param equity_values Vector of equity progression
     */
    static void validate_all_metrics(
        double mrb_pct,
        double total_return_pct,
        int blocks,
        const std::vector<sentio::BackendComponent::TradeOrder>& trades,
        const std::vector<double>& equity_values) {
        
        try {
            validate_mrb(mrb_pct);
            validate_total_return(total_return_pct, blocks);
            validate_trade_realism(trades);
            validate_equity_progression(equity_values);
            
            // If we get here, all validations passed
            std::cout << "✅ Performance validation passed - metrics appear realistic" << std::endl;
            
        } catch (const std::runtime_error& e) {
            // Re-throw with additional context
            throw std::runtime_error(
                "CRITICAL PERFORMANCE VALIDATION FAILURE: " + std::string(e.what()) + 
                "\n\nThis indicates a fundamental bug in trade execution or calculation logic. " +
                "DO NOT USE THESE RESULTS FOR INVESTMENT DECISIONS. " +
                "The trading system must be fixed before proceeding."
            );
        }
    }
    
    /**
     * @brief Get realistic performance bounds for different strategy types
     * 
     * @param strategy_name Name of the strategy (e.g., "sgo", "xgb", "ppo")
     * @return Pair of (min_mrb, max_mrb) expected for this strategy type
     */
    static std::pair<double, double> get_realistic_mrb_bounds(const std::string& strategy_name) {
        if (strategy_name == "sgo") {
            return {-1.0, 2.0}; // SGO: -1% to +2% per block (conservative momentum)
        } else if (strategy_name == "xgb" || strategy_name == "xgboost") {
            return {-2.0, 3.0}; // XGBoost: -2% to +3% per block (ML can be more volatile)
        } else if (strategy_name == "ppo" || strategy_name == "leveraged_ppo") {
            return {-3.0, 2.0}; // PPO: -3% to +2% per block (RL learning phase)
        } else {
            return {-2.0, 2.0}; // Default conservative bounds
        }
    }
};

} // namespace sentio::validation
