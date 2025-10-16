# Optuna Daily Optimization Requirements - Maximize Daily Returns

**Document Version**: 1.0  
**Date**: 2025-01-27  
**Status**: REQUIREMENTS DEFINITION  
**Priority**: CRITICAL - Production Blocker  

---

## Executive Summary

The current Optuna optimization system has a **critical flaw** that prevents it from finding parameters that maximize daily returns in live trading. The system optimizes for block-based performance without enforcing End-of-Day (EOD) closing constraints, leading to:

1. **Overnight positions** in optimization that don't exist in live trading
2. **Distorted MRB calculations** due to overnight price gaps
3. **Suboptimal parameters** that fail to maximize daily returns
4. **Mismatch between optimization and live trading behavior**

This document defines requirements for a **Daily Return Maximization System** that optimizes parameters twice daily (pre-market and midday) to achieve maximum daily returns in live trading.

---

## Business Requirements

### BR-1: Daily Return Maximization
- **Requirement**: System must optimize parameters to maximize **daily returns** (not block returns)
- **Success Criteria**: Live trading daily returns match or exceed optimization predictions
- **Priority**: CRITICAL

### BR-2: Twice-Daily Optimization
- **Requirement**: Run optimization twice per trading day:
  1. **Pre-market optimization** (8:00-9:30 AM ET)
  2. **Midday optimization** (2:30-3:00 PM ET)
- **Success Criteria**: Parameters adapt to intraday market regime changes
- **Priority**: HIGH

### BR-3: EOD Constraint Enforcement
- **Requirement**: All optimization scenarios must enforce 3:58 PM ET position closing
- **Success Criteria**: Zero overnight positions in all optimization results
- **Priority**: CRITICAL

### BR-4: Live Trading Alignment
- **Requirement**: Optimization behavior must exactly match live trading behavior
- **Success Criteria**: Optimization MRB within 10% of live trading performance
- **Priority**: CRITICAL

---

## Functional Requirements

### FR-1: Daily-Based Optimization Engine

**FR-1.1: Daily Data Segmentation**
- Split historical data into complete trading days (9:30 AM - 4:00 PM ET)
- Each day must contain exactly 390 bars (6.5 hours × 60 minutes)
- Reject incomplete days or days with data gaps

**FR-1.2: Daily Return Calculation**
- Calculate return for each complete trading day
- Enforce 100% cash position at 3:58 PM ET
- Account for all trading costs and slippage

**FR-1.3: Multi-Day Optimization**
- Optimize on rolling windows of 3-5 trading days
- Use recent days for training, current day for validation
- Adapt to current market regime

### FR-2: Pre-Market Optimization System

**FR-2.1: Morning Data Collection**
- Collect last 5 trading days of market data
- Include overnight news and pre-market indicators
- Validate data quality and completeness

**FR-2.2: Pre-Market Parameter Optimization**
- Run 100-200 Optuna trials in 30-45 minutes
- Optimize for expected daily return
- Validate parameters against recent market conditions

**FR-2.3: Parameter Deployment**
- Deploy optimized parameters before 9:30 AM ET
- Validate parameter ranges and constraints
- Fallback to previous day's parameters if optimization fails

### FR-3: Midday Optimization System

**FR-3.1: Intraday Performance Monitoring**
- Monitor strategy performance through 2:30 PM ET
- Collect morning trading data and market conditions
- Identify regime changes or performance degradation

**FR-3.2: Midday Parameter Re-optimization**
- Run 50-100 quick trials in 15-20 minutes
- Optimize for afternoon performance
- Account for morning's realized performance

**FR-3.3: Seamless Parameter Transition**
- Liquidate positions before parameter change
- Restart trading with new parameters
- Maintain strategy continuity

### FR-4: EOD Constraint Enforcement

**FR-4.1: Position Closing Validation**
- Enforce 3:58 PM ET position closing in all scenarios
- Validate zero positions at end of each trading day
- Reject optimization results with overnight positions

**FR-4.2: Gap Risk Elimination**
- Eliminate overnight price gap exposure
- Ensure all returns are from intraday trading only
- Account for market close timing constraints

### FR-5: Performance Validation System

**FR-5.1: Optimization Validation**
- Validate optimization results against historical data
- Ensure MRB calculations are realistic and achievable
- Test parameter stability across different market conditions

**FR-5.2: Live Trading Validation**
- Compare live trading performance to optimization predictions
- Monitor parameter effectiveness in real-time
- Adjust optimization methodology based on live results

---

## Technical Requirements

### TR-1: Optuna Optimization Engine

**TR-1.1: Daily Return Objective Function**
```python
def daily_return_objective(trial):
    """Optimize for maximum daily return with EOD constraints."""
    params = sample_parameters(trial)
    
    daily_returns = []
    for day in recent_trading_days:
        day_result = simulate_daily_trading(params, day)
        
        # CRITICAL: Validate EOD closing
        if not day_result.ends_with_cash:
            return -999.0  # Invalid trial
        
        daily_returns.append(day_result.return_pct)
    
    return np.mean(daily_returns)  # Average daily return
```

**TR-1.2: Parameter Space Definition**
- `buy_threshold`: [0.50, 0.70] - Probability threshold for long positions
- `sell_threshold`: [0.30, 0.50] - Probability threshold for short positions
- `ewrls_lambda`: [0.990, 0.999] - Learning rate adaptation
- `bb_amplification_factor`: [0.00, 0.20] - Bollinger Band amplification
- `horizon_weights`: [0.1, 0.8] each - Multi-horizon ensemble weights

**TR-1.3: Constraint Validation**
- Ensure `buy_threshold > sell_threshold`
- Validate horizon weights sum to 1.0
- Enforce parameter bounds and stability

### TR-2: Daily Simulation Engine

**TR-2.1: Complete Day Simulation**
```python
class DailyTradingSimulator:
    def simulate_day(self, params, day_bars):
        """Simulate complete trading day with EOD enforcement."""
        
        # Initialize strategy with parameters
        strategy = OnlineEnsembleStrategy(params)
        portfolio = Portfolio(initial_capital=100000)
        
        # Process each bar
        for bar in day_bars:
            # Generate signal
            signal = strategy.generate_signal(bar)
            
            # Execute trade if within trading hours
            if self.is_trading_hours(bar.timestamp):
                self.execute_trade(signal, bar, portfolio)
            
            # CRITICAL: Enforce EOD closing
            if self.is_eod_time(bar.timestamp):
                self.force_close_all_positions(portfolio)
        
        # Validate end-of-day state
        assert portfolio.total_positions == 0, "EOD constraint violated"
        
        return DailyResult(
            return_pct=(portfolio.equity - 100000) / 100000,
            ends_with_cash=True,
            trades=portfolio.trade_history
        )
```

**TR-2.2: EOD Constraint Enforcement**
- Force close all positions at 3:58 PM ET
- Validate zero positions at end of each day
- Account for market close timing and liquidity

### TR-3: Twice-Daily Optimization Orchestration

**TR-3.1: Pre-Market Optimization**
```python
class PreMarketOptimizer:
    def optimize_morning_parameters(self):
        """Run pre-market optimization for daily return maximization."""
        
        # Collect recent data
        recent_data = self.collect_recent_trading_days(days=5)
        
        # Create optimization study
        study = optuna.create_study(
            direction='maximize',
            sampler=optuna.samplers.TPESampler(seed=42)
        )
        
        # Run optimization
        study.optimize(
            lambda trial: self.daily_return_objective(trial, recent_data),
            n_trials=150,
            timeout=2700  # 45 minutes
        )
        
        # Validate and deploy parameters
        best_params = study.best_params
        if self.validate_parameters(best_params):
            self.deploy_parameters(best_params)
        
        return best_params
```

**TR-3.2: Midday Optimization**
```python
class MiddayOptimizer:
    def optimize_afternoon_parameters(self):
        """Run midday optimization based on morning performance."""
        
        # Collect morning performance data
        morning_data = self.collect_morning_performance()
        
        # Adjust optimization based on morning results
        if morning_data.performance < 0:
            # Poor morning performance - more conservative parameters
            self.adjust_parameter_bounds(conservative=True)
        else:
            # Good morning performance - maintain or slightly aggressive
            self.adjust_parameter_bounds(conservative=False)
        
        # Run quick optimization
        study = optuna.create_study(
            direction='maximize',
            sampler=optuna.samplers.TPESampler(seed=42)
        )
        
        study.optimize(
            lambda trial: self.afternoon_objective(trial, morning_data),
            n_trials=75,
            timeout=1200  # 20 minutes
        )
        
        return study.best_params
```

### TR-4: Integration with Live Trading System

**TR-4.1: Parameter Deployment**
- Seamlessly deploy optimized parameters to live trading
- Validate parameter compatibility with current strategy state
- Maintain strategy continuity during parameter transitions

**TR-4.2: Performance Monitoring**
- Monitor live trading performance against optimization predictions
- Detect parameter degradation or market regime changes
- Trigger re-optimization if performance deviates significantly

---

## System Architecture Requirements

### AR-1: Modular Optimization Framework

**AR-1.1: Core Components**
```
optuna_daily_optimization/
├── daily_simulator.py          # Daily trading simulation engine
├── pre_market_optimizer.py     # Morning optimization system
├── midday_optimizer.py         # Afternoon optimization system
├── eod_enforcer.py            # EOD constraint enforcement
├── parameter_validator.py     # Parameter validation and deployment
└── performance_monitor.py     # Live performance monitoring
```

**AR-1.2: Integration Points**
- Integrate with existing `scripts/launch_trading.sh`
- Extend `OnlineEnsembleStrategy` for parameter optimization
- Enhance `scripts/run_2phase_optuna.py` with daily constraints

### AR-2: Data Management System

**AR-2.1: Daily Data Collection**
- Automatically collect and validate daily market data
- Maintain rolling window of recent trading days
- Handle data gaps and quality issues

**AR-2.2: Performance Data Storage**
- Store optimization results and parameter history
- Track live trading performance vs optimization predictions
- Maintain audit trail for regulatory compliance

### AR-3: Real-Time Monitoring System

**AR-3.1: Performance Tracking**
- Real-time monitoring of strategy performance
- Comparison with optimization predictions
- Alert system for performance degradation

**AR-3.2: Parameter Effectiveness Monitoring**
- Track parameter effectiveness over time
- Identify when parameters need re-optimization
- Maintain parameter performance history

---

## Performance Requirements

### PR-1: Optimization Speed
- **Pre-market optimization**: Complete within 45 minutes
- **Midday optimization**: Complete within 20 minutes
- **Parameter deployment**: Complete within 2 minutes

### PR-2: Accuracy Requirements
- **Optimization accuracy**: MRB prediction within 10% of live performance
- **Parameter stability**: Parameters remain effective for at least 2-3 hours
- **Daily return correlation**: R² > 0.8 between optimization and live results

### PR-3: Reliability Requirements
- **System uptime**: 99.9% availability during trading hours
- **Parameter deployment success**: >99% successful deployments
- **EOD constraint enforcement**: 100% compliance (zero overnight positions)

---

## Implementation Requirements

### IR-1: Code Changes Required

**IR-1.1: New Files to Create**
```
scripts/
├── daily_optuna_optimizer.py      # Main daily optimization engine
├── pre_market_optimization.py     # Morning optimization system
├── midday_optimization.py         # Afternoon optimization system
└── eod_constraint_validator.py    # EOD enforcement validation

src/optimization/
├── daily_simulator.cpp            # C++ daily simulation engine
├── parameter_optimizer.cpp        # Parameter optimization logic
└── eod_enforcer.cpp              # EOD constraint enforcement

include/optimization/
├── daily_simulator.h              # Daily simulation interface
├── parameter_optimizer.h          # Parameter optimization interface
└── eod_enforcer.h                # EOD enforcement interface
```

**IR-1.2: Existing Files to Modify**
- `scripts/launch_trading.sh` - Integrate twice-daily optimization
- `scripts/run_2phase_optuna.py` - Add EOD constraint validation
- `src/strategy/online_ensemble_strategy.cpp` - Enhance parameter handling
- `src/cli/execute_trades_command.cpp` - Strengthen EOD enforcement

### IR-2: OES Strategy Enhancements

**IR-2.1: Parameter Optimization Integration**
```cpp
class OnlineEnsembleStrategy {
public:
    // Enhanced parameter handling for optimization
    struct OptimizationConfig {
        double buy_threshold;
        double sell_threshold;
        double ewrls_lambda;
        double bb_amplification_factor;
        std::vector<double> horizon_weights;
    };
    
    // Load optimized parameters
    void load_optimized_parameters(const OptimizationConfig& config);
    
    // Validate parameter effectiveness
    bool validate_parameters() const;
    
    // Get current parameter performance
    PerformanceMetrics get_current_performance() const;
};
```

**IR-2.2: Real-Time Parameter Adjustment**
- Support for mid-session parameter updates
- Seamless transition between parameter sets
- Maintain strategy state during parameter changes

### IR-3: Launch System Integration

**IR-3.1: Enhanced Launch Script**
```bash
# scripts/launch_trading.sh enhancements
function run_pre_market_optimization() {
    log_info "Running pre-market optimization..."
    python3 scripts/daily_optuna_optimizer.py \
        --mode pre-market \
        --data data/equities/SPY_recent_5days.csv \
        --output config/morning_params.json \
        --trials 150 \
        --timeout 2700
}

function run_midday_optimization() {
    log_info "Running midday optimization..."
    python3 scripts/daily_optuna_optimizer.py \
        --mode midday \
        --data data/equities/SPY_today_morning.csv \
        --output config/afternoon_params.json \
        --trials 75 \
        --timeout 1200
}
```

**IR-3.2: Automated Optimization Scheduling**
- Pre-market optimization: 8:00-9:30 AM ET
- Midday optimization: 2:30-3:00 PM ET
- Automatic parameter deployment and validation

---

## Testing Requirements

### TR-1: Unit Testing
- Test daily simulation engine with various market conditions
- Validate EOD constraint enforcement
- Test parameter optimization algorithms

### TR-2: Integration Testing
- Test twice-daily optimization workflow
- Validate parameter deployment and transition
- Test performance monitoring and alerting

### TR-3: Backtesting Validation
- Validate optimization results against historical data
- Test parameter effectiveness across different market regimes
- Ensure MRB calculations match live trading performance

### TR-4: Live Trading Validation
- Deploy to paper trading environment
- Monitor performance vs optimization predictions
- Validate EOD constraint enforcement in live environment

---

## Risk Management Requirements

### RM-1: Parameter Risk Management
- Validate parameter bounds and constraints
- Implement parameter stability checks
- Fallback to previous parameters if optimization fails

### RM-2: Performance Risk Management
- Monitor live performance vs optimization predictions
- Implement performance degradation alerts
- Automatic parameter re-optimization triggers

### RM-3: System Risk Management
- Implement system health monitoring
- Automatic fallback mechanisms
- Comprehensive error handling and logging

---

## Success Criteria

### SC-1: Daily Return Maximization
- **Target**: Achieve 0.5%+ daily returns consistently
- **Measurement**: Average daily return over 30 trading days
- **Validation**: Live trading performance matches optimization predictions

### SC-2: Optimization Effectiveness
- **Target**: MRB prediction accuracy within 10% of live performance
- **Measurement**: Correlation between optimization and live results
- **Validation**: R² > 0.8 between predicted and actual daily returns

### SC-3: System Reliability
- **Target**: 99.9% system uptime during trading hours
- **Measurement**: System availability and parameter deployment success
- **Validation**: Zero overnight positions, successful parameter deployments

---

## Implementation Timeline

### Phase 1: Core Development (Week 1)
- Implement daily simulation engine
- Create EOD constraint enforcement system
- Develop basic parameter optimization framework

### Phase 2: Integration (Week 2)
- Integrate with existing launch system
- Enhance OES strategy for parameter optimization
- Implement twice-daily optimization workflow

### Phase 3: Testing & Validation (Week 3)
- Comprehensive backtesting validation
- Paper trading deployment and monitoring
- Performance optimization and tuning

### Phase 4: Production Deployment (Week 4)
- Live trading deployment
- Performance monitoring and optimization
- Documentation and training

---

## Conclusion

This requirement document defines a comprehensive system for maximizing daily returns through twice-daily parameter optimization. The system addresses the critical flaw in current optimization by enforcing EOD constraints and focusing on daily return maximization rather than block-based performance.

The implementation will require significant enhancements to the existing codebase, including new optimization engines, enhanced strategy components, and integrated monitoring systems. However, the expected benefits in terms of daily return maximization and live trading performance alignment justify the investment.

**Next Steps**:
1. Review and approve requirements
2. Begin Phase 1 implementation
3. Establish testing and validation framework
4. Plan production deployment strategy
