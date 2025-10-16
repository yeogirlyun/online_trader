#pragma once

#include "strategy/online_ensemble_strategy.h"
#include "strategy/signal_output.h"
#include "data/multi_symbol_data_manager.h"
#include "common/types.h"
#include <memory>
#include <map>
#include <vector>
#include <string>

namespace sentio {

/**
 * @brief Manages 6 independent OnlineEnsemble strategy instances
 *
 * Each symbol gets its own:
 * - OnlineEnsembleStrategy instance
 * - EWRLS predictor
 * - Feature engine
 * - Learning state
 *
 * This ensures no cross-contamination between symbols and allows
 * pure independent signal generation.
 *
 * Usage:
 *   MultiSymbolOESManager oes_mgr(config, data_mgr);
 *   auto signals = oes_mgr.generate_all_signals();
 *   oes_mgr.update_all(realized_pnls);
 */
class MultiSymbolOESManager {
public:
    struct Config {
        std::vector<std::string> symbols;              // Active symbols
        OnlineEnsembleStrategy::OnlineEnsembleConfig base_config;  // Base config for all OES
        bool independent_learning = true;              // Each symbol learns independently
        bool share_features = false;                   // Share feature engine (NOT RECOMMENDED)

        // Symbol-specific overrides (optional)
        std::map<std::string, OnlineEnsembleStrategy::OnlineEnsembleConfig> symbol_configs;
    };

    /**
     * @brief Construct manager with data source
     *
     * @param config Configuration
     * @param data_mgr Data manager for multi-symbol data
     */
    explicit MultiSymbolOESManager(
        const Config& config,
        std::shared_ptr<data::MultiSymbolDataManager> data_mgr
    );

    ~MultiSymbolOESManager() = default;

    // === Signal Generation ===

    /**
     * @brief Generate signals for all symbols
     *
     * Returns map of symbol → signal. Only symbols with valid data
     * will have signals.
     *
     * @return Map of symbol → SignalOutput
     */
    std::map<std::string, SignalOutput> generate_all_signals();

    /**
     * @brief Generate signal for specific symbol
     *
     * @param symbol Symbol ticker
     * @return SignalOutput for symbol (or empty if data invalid)
     */
    SignalOutput generate_signal(const std::string& symbol);

    // === Learning Updates ===

    /**
     * @brief Update all OES instances with realized P&L
     *
     * @param realized_pnls Map of symbol → realized P&L
     */
    void update_all(const std::map<std::string, double>& realized_pnls);

    /**
     * @brief Update specific symbol with realized P&L
     *
     * @param symbol Symbol ticker
     * @param realized_pnl Realized P&L from last trade
     */
    void update(const std::string& symbol, double realized_pnl);

    /**
     * @brief Process new bar for all symbols
     *
     * Called each bar to update feature engines and check learning state.
     */
    void on_bar();

    // === Warmup ===

    /**
     * @brief Warmup all OES instances from historical data
     *
     * @param symbol_bars Map of symbol → historical bars
     * @return true if warmup successful
     */
    bool warmup_all(const std::map<std::string, std::vector<Bar>>& symbol_bars);

    /**
     * @brief Warmup specific symbol
     *
     * @param symbol Symbol ticker
     * @param bars Historical bars
     * @return true if warmup successful
     */
    bool warmup(const std::string& symbol, const std::vector<Bar>& bars);

    // === Configuration ===

    /**
     * @brief Update configuration for all symbols
     *
     * @param new_config New base configuration
     */
    void update_config(const OnlineEnsembleStrategy::OnlineEnsembleConfig& new_config);

    /**
     * @brief Update configuration for specific symbol
     *
     * @param symbol Symbol ticker
     * @param new_config New configuration
     */
    void update_config(const std::string& symbol,
                      const OnlineEnsembleStrategy::OnlineEnsembleConfig& new_config);

    // === Diagnostics ===

    /**
     * @brief Get performance metrics for all symbols
     *
     * @return Map of symbol → performance metrics
     */
    std::map<std::string, OnlineEnsembleStrategy::PerformanceMetrics>
    get_all_performance_metrics() const;

    /**
     * @brief Get performance metrics for specific symbol
     *
     * @param symbol Symbol ticker
     * @return Performance metrics
     */
    OnlineEnsembleStrategy::PerformanceMetrics
    get_performance_metrics(const std::string& symbol) const;

    /**
     * @brief Check if all OES instances are ready
     *
     * @return true if all have sufficient warmup samples
     */
    bool all_ready() const;

    /**
     * @brief Get ready status for each symbol
     *
     * @return Map of symbol → ready status
     */
    std::map<std::string, bool> get_ready_status() const;

    /**
     * @brief Get learning state for all symbols
     *
     * @return Map of symbol → learning state
     */
    std::map<std::string, OnlineEnsembleStrategy::LearningState>
    get_all_learning_states() const;

    /**
     * @brief Get OES instance for symbol (for direct access)
     *
     * @param symbol Symbol ticker
     * @return Pointer to OES instance (nullptr if not found)
     */
    OnlineEnsembleStrategy* get_oes_instance(const std::string& symbol);

    /**
     * @brief Get const OES instance for symbol
     *
     * @param symbol Symbol ticker
     * @return Const pointer to OES instance (nullptr if not found)
     */
    const OnlineEnsembleStrategy* get_oes_instance(const std::string& symbol) const;

private:
    /**
     * @brief Get latest bar for symbol from data manager
     *
     * @param symbol Symbol ticker
     * @param bar Output bar
     * @return true if valid bar available
     */
    bool get_latest_bar(const std::string& symbol, Bar& bar);

    Config config_;
    std::shared_ptr<data::MultiSymbolDataManager> data_mgr_;

    // Map of symbol → OES instance
    std::map<std::string, std::unique_ptr<OnlineEnsembleStrategy>> oes_instances_;

    // Statistics
    int total_signals_generated_{0};
    int total_updates_{0};
};

} // namespace sentio
