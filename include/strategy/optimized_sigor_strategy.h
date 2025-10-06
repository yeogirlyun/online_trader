// File: include/strategy/optimized_sigor_strategy.h
#ifndef OPTIMIZED_SIGOR_STRATEGY_H
#define OPTIMIZED_SIGOR_STRATEGY_H

#include "strategy/strategy_component.h"
#include "strategy/signal_output.h"
#include "common/types.h"
#include <vector>
#include <deque>
#include <memory>

namespace sentio {

// Optimized configuration for SGO strategy to achieve positive MRB
struct OptimizedSigorConfig {
    // Fusion parameters
    double k = 2.0;  // INCREASED from 1.5 for sharper signals
    
    // Optimized detector weights - reduce noisy signals
    double w_boll = 1.2;    // INCREASED: Bollinger Bands (strong signal)
    double w_rsi  = 0.8;    // DECREASED: RSI (noisy in trending markets)
    double w_mom  = 1.0;    // KEEP: Momentum (balanced)
    double w_vwap = 1.1;    // INCREASED: VWAP (good mean reversion)
    double w_orb  = 0.3;    // DECREASED: ORB (too frequent)
    double w_ofi  = 0.4;    // DECREASED: OFI (noisy proxy)
    double w_vol  = 0.6;    // INCREASED: Volume (good confirmation)
    // WRT removed
    
    // Optimized window parameters for smoother signals
    int win_boll = 25;              // INCREASED from 20
    int win_rsi  = 21;              // INCREASED from 14
    int win_mom  = 15;              // INCREASED from 10
    int win_vwap = 25;              // INCREASED from 20
    int orb_opening_bars = 45;      // INCREASED from 30
    int volume_window = 25;          // INCREASED from 20
    
    // Signal quality filters - KEY FOR REDUCING OVER-TRADING
    double min_signal_strength = 0.15;     // Minimum |p - 0.5|
    double min_detector_agreement = 0.6;   // Minimum detector consensus
    bool enable_signal_filtering = true;   // Enable quality filtering
    
    // Market regime adaptation
    bool enable_regime_adaptation = true;
    int regime_detection_window = 50;
    
    // Transaction cost awareness
    double transaction_cost = 0.001;       // 0.1% per trade
    double min_profit_target = 0.003;      // 0.3% minimum profit
    bool enable_cost_filtering = true;     // Enable cost-aware filtering
    
    // ========== DETECTOR CALIBRATION PARAMETERS ==========
    // Bollinger Bands calibration
    double bollinger_band_width = 2.0;      // Range: 1.5 to 2.5
    double bollinger_steepness = 6.0;       // Range: 3.0 to 10.0
    double bollinger_threshold = 0.3;       // Range: 0.2 to 0.4
    double volume_threshold = 1.5;          // For volume confirmation
    double volume_boost = 1.2;              // Range: 1.1 to 1.3
    
    // RSI calibration
    double rsi_oversold = 30.0;             // Range: 20 to 35
    double rsi_overbought = 70.0;           // Range: 65 to 80
    double rsi_sharpness = 5.0;             // Range: 2.0 to 8.0
    double rsi_momentum_bias = 0.05;        // Range: 0.0 to 0.1
    double high_vol_threshold = 0.015;      // For threshold adaptation
    
    // Momentum calibration
    double momentum_threshold = 0.005;      // Range: 0.001 to 0.01
    double momentum_sensitivity = 100.0;    // Range: 50 to 200
    double acceleration_weight = 10.0;      // Range: 5 to 20
    double acceleration_boost = 0.2;        // Range: 0.1 to 0.3
    
    // VWAP calibration
    double vwap_inner_band = 1.0;           // Range: 0.5 to 1.5
    double vwap_outer_band = 2.5;           // Range: 2.0 to 3.0
    double vwap_sharpness = 3.0;            // Range: 1.0 to 5.0
    double vwap_volume_boost = 0.05;        // Range: 0.0 to 0.1
    
    // ORB calibration
    double orb_breakout_1 = 1.2;            // Range: 1.0 to 1.5
    double orb_breakout_2 = 2.0;            // Range: 1.5 to 2.5
    double orb_sharpness = 2.0;
    int orb_relevance_window = 180;         // Range: 120 to 240
    double orb_decay_rate = 0.01;
    
    // OFI calibration
    int ofi_window = 10;                    // Range: 5 to 20
    double ofi_threshold = 0.03;            // Range: 0.01 to 0.05
    double ofi_sensitivity = 50.0;          // Range: 20 to 100
    
    // Volume calibration
    double volume_surge_1 = 2.0;            // Range: 1.5 to 2.5
    double volume_surge_2 = 3.5;            // Range: 2.5 to 4.0
    double volume_fade_factor = 10.0;       // Range: 5 to 20
    double volume_price_sensitivity = 30.0; // Range: 10 to 50
    
    // Williams/RSI/TSI detector removed
    
    // Config loading
    static OptimizedSigorConfig from_file(const std::string& path);
    static OptimizedSigorConfig defaults();
};

// Optimized SGO (Sigor) strategy with improved signal quality
class OptimizedSigorStrategy : public StrategyComponent {
public:
    explicit OptimizedSigorStrategy(const OptimizedSigorConfig& config = {});
    
    // Core strategy interface
    virtual SignalOutput generate_signal(const Bar& bar, int bar_index) override;
    virtual void update_indicators(const Bar& bar) override;
    virtual bool is_warmed_up() const override;
    
    // Configuration
    void update_config(const OptimizedSigorConfig& config);
    const OptimizedSigorConfig& get_config() const { return config_; }
    
    // Reset strategy state (for walk-forward validation)
    void reset();
    
    // Market regime detection
    enum class MarketRegime {
        TRENDING_UP,
        TRENDING_DOWN,
        RANGING,
        VOLATILE,
        UNKNOWN
    };
    
    MarketRegime detect_market_regime() const;
    
    // Performance tracking for adaptive optimization
    void track_signal_performance(const SignalOutput& signal, double actual_return);
    double get_recent_accuracy() const;
    
protected:
    // Individual detector probabilities (original)
    double prob_bollinger_(const Bar& bar);
    double prob_rsi_14_();
    double prob_momentum_(int window, double threshold);
    double prob_vwap_reversion_(int window);
    double prob_orb_daily_(int opening_bars);
    double prob_ofi_proxy_(const Bar& bar);
    double prob_volume_surge_scaled_(int window);
    
    // Calibrated detector probabilities (enhanced)
    double prob_bollinger_calibrated(const Bar& bar);
    double prob_rsi_calibrated();
    double prob_momentum_calibrated();
    double prob_vwap_calibrated(const Bar& bar);
    double prob_orb_calibrated();
    double prob_ofi_calibrated(const Bar& bar);
    double prob_volume_calibrated();
    // WRT removed
    
    // Enhanced signal aggregation with quality filtering (8 detectors)
    double aggregate_probability_enhanced(
        double p1, double p2, double p3, double p4,
        double p5, double p6, double p7
    );
    
    // Signal quality assessment (simplified - no confidence)
    bool passes_transaction_cost_filter(double probability, const Bar& bar);
    
    // Regime-specific parameter adaptation
    void adapt_weights_for_regime(MarketRegime regime);
    void restore_default_weights();
    
    // Helper functions
    double calculate_sma(int window) const;
    double calculate_ema(double alpha) const;
    double calculate_std_dev(int window) const;
    double calculate_rsi() const;
    double calculate_momentum(int window) const;
    double calculate_vwap() const;
    double calculate_volume_ratio() const;
    std::string get_regime_name(MarketRegime regime) const;
    std::string join_strings(const std::vector<std::string>& strings, const std::string& delimiter) const;
    
    // Enhanced helper functions for calibrated detectors
    double calculate_mean(const std::deque<double>& data, int window) const;
    double calculate_std(const std::deque<double>& data, int window, double mean) const;
    double calculate_rsi(int window) const;
    double calculate_vwap(int window) const;
    double calculate_vwap_std(int window) const;
    double calculate_momentum(int lookback, int window) const;
    void update_volatility();
    void update_rsi_slope();
    
    // WRT helpers removed
    
    // Signal history tracking for filtering
    void update_signal_history(const SignalOutput& signal);
    double calculate_signal_variance() const;
    double calculate_signal_momentum() const;
    
private:
    OptimizedSigorConfig config_;
    OptimizedSigorConfig original_config_;  // For restoration after regime adaptation
    
    // Price and volume history
    std::deque<double> price_history_;
    std::deque<double> volume_history_;
    std::deque<double> high_history_;
    std::deque<double> low_history_;
    
    // Technical indicator states
    double rsi_avg_gain_ = 0.0;
    double rsi_avg_loss_ = 0.0;
    double vwap_cumulative_ = 0.0;
    double volume_cumulative_ = 0.0;
    double opening_range_high_ = 0.0;
    double opening_range_low_ = 0.0;
    int bars_since_open_ = 0;
    
    // Enhanced detector states
    double rsi_slope_ = 0.0;            // RSI momentum
    double current_volatility_ = 0.01;  // Current market volatility
    double average_volume_ = 1000000.0; // Average volume for calculations
    std::deque<double> ofi_history_;    // OFI persistence tracking
    std::deque<double> vwap_history_;   // VWAP history for calculations
    
    // WRT state removed
    
    // Signal quality tracking
    std::deque<SignalOutput> signal_history_;
    
    // Performance tracking
    struct PerformanceRecord {
        double predicted_direction;  // 1.0 for up, -1.0 for down
        double actual_return;
    };
    std::deque<PerformanceRecord> performance_history_;
    
    // Market regime
    mutable MarketRegime current_regime_ = MarketRegime::UNKNOWN;
    mutable int regime_bars_count_ = 0;
    
    // Constants
    static constexpr int MAX_HISTORY_SIZE = 100;
    static constexpr double EPSILON = 1e-10;
};

// Factory function for creating optimized SGO strategy
std::unique_ptr<OptimizedSigorStrategy> create_optimized_sgo_strategy(
    const OptimizedSigorConfig& config = {}
);

} // namespace sentio

#endif // OPTIMIZED_SIGOR_STRATEGY_H
