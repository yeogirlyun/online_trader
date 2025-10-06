#pragma once

#include <vector>
#include <string>
#include <memory>
#include "common/types.h"
#include "strategy/signal_output.h"
#include "strategy/strategy_component.h"

namespace sentio {

/**
 * Unified Strategy Interface
 * 
 * This interface ensures all strategies implement consistent behavior:
 * - Proper initialization with configuration
 * - Unified data processing (no file I/O in strategies)
 * - Consistent error handling
 * - Testable design through dependency injection
 */
class IStrategy {
public:
    virtual ~IStrategy() = default;

    /**
     * Initialize the strategy with configuration
     * @param config Strategy-specific configuration
     * @return true if initialization successful, false otherwise
     * @throws std::runtime_error for critical initialization failures
     */
    virtual bool initialize(const StrategyComponent::StrategyConfig& config) = 0;

    /**
     * Process market data and generate trading signals
     * 
     * This method should be pure - no file I/O, no knowledge of blocks/ranges.
     * The CLI layer handles data loading and range selection.
     * 
     * @param market_data Vector of market bars to process
     * @return Vector of generated trading signals
     * @throws std::runtime_error for processing failures
     */
    virtual std::vector<SignalOutput> process_data(const std::vector<Bar>& market_data) = 0;

    /**
     * Get strategy name for identification
     */
    virtual std::string get_strategy_name() const = 0;

    /**
     * Get strategy version
     */
    virtual std::string get_strategy_version() const = 0;

    /**
     * Check if strategy requires warmup period
     */
    virtual bool requires_warmup() const = 0;

    /**
     * Get required warmup bars count
     */
    virtual int get_warmup_bars() const = 0;

    /**
     * Validate strategy is ready for processing
     * Called before process_data to ensure strategy is in valid state
     */
    virtual bool validate() const = 0;

    /**
     * Get strategy-specific metadata for signal output
     */
    virtual std::map<std::string, std::string> get_metadata() const = 0;
    
    /**
     * Reset strategy internal state
     * 
     * Called between independent processing runs (e.g., walk-forward windows)
     * to ensure clean state without contamination from previous data.
     * 
     * Should clear:
     * - Indicator history buffers
     * - Calculation state (moving averages, etc.)
     * - Signal tracking/throttling state
     * - Any accumulated statistics
     */
    virtual void reset() = 0;
};

/**
 * Strategy type enumeration
 */
enum class StrategyType {
    OPTIMIZED_SGO,      // SGO with production-grade filtering (default SGO)
    PPO,                // Renamed from LEVERAGED_PPO
    XGBOOST,            // Intraday XGBoost with 25 features
    CATBOOST,           // Short name: CTB
    TFT,
    WILLIAMS_RSI_TSI,   // Williams/RSI crossover with TSI gating
    WILLIAMS_RSI_BB,    // Williams/RSI crossover with Bollinger Bands
    WILLIAMS_RSI,       // Williams/RSI simple strategy (no TSI gating)
    DETERMINISTIC_TEST, // Deterministic test strategy with known outcomes
    CHEAT               // Cheat strategy that looks ahead for validation
};

/**
 * Strategy Factory Functions
 * Simple free functions for creating strategy instances
 */

/**
 * Create strategy instance by name
 * @param strategy_name String name of strategy (e.g., "sgo", "xgb", "ctb")
 * @return Unique pointer to strategy instance
 * @throws std::invalid_argument for unknown strategy names
 */
std::unique_ptr<IStrategy> create_strategy(const std::string& strategy_name);

/**
 * Create strategy instance by type enum
 * @param type Strategy type enum
 * @return Unique pointer to strategy instance
 */
std::unique_ptr<IStrategy> create_strategy(StrategyType type);

/**
 * Get list of available strategy names
 */
std::vector<std::string> get_available_strategies();

/**
 * Check if strategy name is valid
 */
bool is_valid_strategy(const std::string& strategy_name);

/**
 * Convert string to strategy type
 */
StrategyType string_to_strategy_type(const std::string& strategy_name);

/**
 * Convert strategy type to string
 */
std::string strategy_type_to_string(StrategyType type);

} // namespace sentio

