#pragma once

#include "backend/position_state_machine.h"
#include "strategy/signal_output.h"
#include "common/types.h"
#include <map>
#include <vector>
#include <memory>

namespace sentio {

/**
 * Enhanced PSM that handles multiple prediction horizons simultaneously
 * Manages overlapping positions from different time horizons
 */
class EnsemblePositionStateMachine : public PositionStateMachine {
public:
    struct EnsembleSignal {
        std::vector<SignalOutput> horizon_signals;  // Signals from each horizon
        std::vector<double> horizon_weights;        // Weight for each horizon
        std::vector<int> horizon_bars;              // Horizon length (1, 5, 10)
        
        // Aggregated results
        double weighted_probability;
        double signal_agreement;  // How much horizons agree (0-1)
        sentio::SignalType consensus_signal;  // Use global SignalType from signal_output.h
        double confidence;
        
        EnsembleSignal()
            : weighted_probability(0.5),
              signal_agreement(0.0),
              consensus_signal(sentio::SignalType::NEUTRAL),
              confidence(0.0) {}
    };
    
    struct HorizonPosition {
        std::string symbol;
        int horizon_bars;
        uint64_t entry_bar_id;
        uint64_t exit_bar_id;     // Expected exit
        double entry_price;
        double predicted_return;
        double position_weight;    // Fraction of capital for this horizon
        sentio::SignalType signal_type;  // Use global SignalType from signal_output.h
        bool is_active;
        
        HorizonPosition()
            : horizon_bars(0),
              entry_bar_id(0),
              exit_bar_id(0),
              entry_price(0.0),
              predicted_return(0.0),
              position_weight(0.0),
              signal_type(sentio::SignalType::NEUTRAL),
              is_active(true) {}
    };
    
    struct EnsembleTransition : public StateTransition {
        // Enhanced transition with multi-horizon awareness
        std::vector<HorizonPosition> horizon_positions;
        double total_position_size;  // Sum across all horizons
        std::map<int, double> horizon_allocations;  // horizon -> % allocation
        bool has_consensus;  // Do horizons agree?
        int dominant_horizon;    // Which horizon has strongest signal
        
        EnsembleTransition()
            : total_position_size(0.0),
              has_consensus(false),
              dominant_horizon(0) {}
    };

    EnsemblePositionStateMachine();
    
    // Main ensemble interface
    EnsembleTransition get_ensemble_transition(
        const PortfolioState& current_portfolio,
        const EnsembleSignal& ensemble_signal,
        const MarketState& market_conditions,
        uint64_t current_bar_id
    );
    
    // Aggregate multiple horizon signals into consensus
    EnsembleSignal aggregate_signals(
        const std::map<int, SignalOutput>& horizon_signals,
        const std::map<int, double>& horizon_weights
    );
    
    // Position management for multiple horizons
    void update_horizon_positions(uint64_t current_bar_id, double current_price);
    std::vector<HorizonPosition> get_active_positions() const;
    std::vector<HorizonPosition> get_closeable_positions(uint64_t current_bar_id) const;
    
    // Dynamic allocation based on signal agreement
    std::map<int, double> calculate_horizon_allocations(const EnsembleSignal& signal);
    
    // Risk management with overlapping positions
    double calculate_ensemble_risk(const std::vector<HorizonPosition>& positions) const;
    double get_maximum_position_size() const;
    
private:
    // Track positions by horizon
    std::map<int, std::vector<HorizonPosition>> positions_by_horizon_;
    
    // Performance tracking by horizon
    std::map<int, double> horizon_accuracy_;
    std::map<int, double> horizon_pnl_;
    std::map<int, int> horizon_trade_count_;
    
    // Ensemble configuration
    static constexpr double BASE_ALLOCATION = 0.3;  // Base allocation per horizon
    static constexpr double CONSENSUS_BONUS = 0.4;  // Extra allocation for agreement
    static constexpr double MIN_AGREEMENT = 0.6;    // Minimum agreement to trade
    
    // Helper methods
    sentio::SignalType determine_consensus(const std::vector<SignalOutput>& signals,
                                          const std::vector<double>& weights) const;
    double calculate_agreement(const std::vector<SignalOutput>& signals) const;
    bool should_override_hold(const EnsembleSignal& signal, uint64_t current_bar_id) const;
    void update_horizon_performance(int horizon, double pnl);
};

} // namespace sentio
