#pragma once

// =============================================================================
// Module: strategy/sigor_strategy.h
// Purpose: SigorStrategy — a rule-based ensemble (Signal-OR) strategy that
//          combines multiple technical detectors into a single probability.
//
// Detailed algorithm (Signal-OR aggregator):
// - Compute several detector probabilities independently per bar:
//   1) Bollinger Z-Score: z = (Close - SMA20) / StdDev20; prob = 0.5 + 0.5*tanh(z/2).
//   2) RSI(14): map RSI [0..100] to prob around 0.5 (50 neutral).
//   3) Momentum (10 bars): prob = 0.5 + 0.5*tanh(return*scale), scale≈50.
//   4) VWAP reversion (rolling window): typical price vs rolling VWAP.
//   5) Opening Range Breakout (daily): breakout above/below day’s opening range (first N bars).
//   6) OFI Proxy: order-flow imbalance proxy using bar geometry and volume.
//   7) Volume Surge: volume vs rolling average scales the momentum signal.
// - OR aggregation: choose the detector with maximum strength |p - 0.5| as the
//   final probability for the bar. This captures the strongest signal.
// - Confidence: blend detector agreement (majority on the same side of 0.5)
//   with maximum strength, then clamp to [0,1].
//
// Design notes:
// - Stateless outside of rolling windows; no I/O. All state is internal buffers.
// - Uses only OHLCV bars, no external feature libs; computations are simplified
//   proxies of the detectors referenced in sentio_cpp.
// - Warmup is inherited from StrategyComponent; emits signals after warmup.
// =============================================================================

#include <vector>
#include <string>
#include <cstdint>
#include "common/types.h"
#include "strategy_component.h"
#include "sigor_config.h"

namespace sentio {

class SigorStrategy : public StrategyComponent {
public:
    // CRITICAL FIX: Constructor now takes the symbol explicitly to prevent "UNKNOWN" symbol issues
    explicit SigorStrategy(const StrategyConfig& config, const std::string& symbol);
    // Optional: set configuration (weights, windows, k)
    void set_config(const SigorConfig& cfg) { cfg_ = cfg; }

protected:
    SignalOutput generate_signal(const Bar& bar, int bar_index) override;
    void update_indicators(const Bar& bar) override;
    bool is_warmed_up() const override;

private:
    SigorConfig cfg_;
    std::string symbol_; // CRITICAL FIX: Store the correct symbol to prevent "UNKNOWN" issues
    // ---- Rolling series ----
    std::vector<double> closes_;
    std::vector<double> highs_;
    std::vector<double> lows_;
    std::vector<double> volumes_;
    std::vector<int64_t> timestamps_;
    std::vector<double> gains_;
    std::vector<double> losses_;

    // ---- Detector probabilities ----
    double prob_bollinger_(const Bar& bar) const;
    double prob_rsi_14_() const;
    double prob_momentum_(int window, double scale) const;
    double prob_vwap_reversion_(int window) const;
    double prob_orb_daily_(int opening_window_bars) const;
    double prob_ofi_proxy_(const Bar& bar) const;
    double prob_volume_surge_scaled_(int window) const;

    // ---- Aggregation & helpers ----
    double aggregate_probability(double p1, double p2, double p3,
                                 double p4, double p5, double p6, double p7) const;
    double calculate_confidence(double p1, double p2, double p3,
                                double p4, double p5, double p6, double p7) const;

    // Stats helpers
    double compute_sma(const std::vector<double>& v, int window) const;
    double compute_stddev(const std::vector<double>& v, int window, double mean) const;
    double compute_rsi(int window) const;
    double clamp01(double v) const { return v < 0.0 ? 0.0 : (v > 1.0 ? 1.0 : v); }
};

} // namespace sentio


