#!/usr/bin/env python3
"""
Analyze SPY 5-Year Data Characteristics
========================================

This script analyzes SPY market characteristics to inform strategy recalibration:
- Volatility profile (intraday, daily, regime-based)
- Return distribution
- Bollinger Band statistics
- Optimal holding periods
- Profit target / stop loss analysis
"""

import pandas as pd
import numpy as np
import json
from datetime import datetime

def analyze_spy_data(filepath):
    """Comprehensive analysis of SPY 5-year data"""

    print("=" * 70)
    print("SPY 5-YEAR DATA ANALYSIS FOR STRATEGY RECALIBRATION")
    print("=" * 70)
    print()

    # Load data
    print("Loading SPY 5-year data...")
    df = pd.read_csv(filepath)
    print(f"✅ Loaded {len(df):,} bars ({len(df) / 480:.1f} blocks)")
    print(f"   Date range: {df['ts_utc'].iloc[0]} to {df['ts_utc'].iloc[-1]}")
    print()

    # Calculate returns
    df['returns'] = df['close'].pct_change()
    df['log_returns'] = np.log(df['close'] / df['close'].shift(1))

    # Intrabar volatility
    df['intrabar_range'] = (df['high'] - df['low']) / df['close']

    # === 1. VOLATILITY ANALYSIS ===
    print("=" * 70)
    print("1. VOLATILITY ANALYSIS")
    print("=" * 70)

    # Overall volatility
    overall_vol = df['returns'].std()
    print(f"Overall 1-bar volatility: {overall_vol:.6f} ({overall_vol*100:.4f}%)")
    print(f"Annualized volatility: {overall_vol * np.sqrt(252*390):.4f} ({overall_vol * np.sqrt(252*390)*100:.2f}%)")
    print()

    # Intrabar range
    avg_intrabar_range = df['intrabar_range'].mean()
    print(f"Average intrabar range: {avg_intrabar_range:.6f} ({avg_intrabar_range*100:.4f}%)")
    print(f"95th percentile intrabar range: {df['intrabar_range'].quantile(0.95):.6f} ({df['intrabar_range'].quantile(0.95)*100:.4f}%)")
    print()

    # Rolling volatility (20-bar window, matching Bollinger Bands)
    df['rolling_vol_20'] = df['returns'].rolling(20).std()
    print(f"Rolling 20-bar volatility:")
    print(f"  Mean: {df['rolling_vol_20'].mean():.6f} ({df['rolling_vol_20'].mean()*100:.4f}%)")
    print(f"  Median: {df['rolling_vol_20'].median():.6f} ({df['rolling_vol_20'].median()*100:.4f}%)")
    print(f"  25th percentile: {df['rolling_vol_20'].quantile(0.25):.6f}")
    print(f"  75th percentile: {df['rolling_vol_20'].quantile(0.75):.6f}")
    print()

    # === 2. RETURN DISTRIBUTION ===
    print("=" * 70)
    print("2. RETURN DISTRIBUTION")
    print("=" * 70)

    mean_return = df['returns'].mean()
    print(f"Mean 1-bar return: {mean_return:.8f} ({mean_return*100:.6f}%)")
    print(f"Median 1-bar return: {df['returns'].median():.8f}")
    print(f"Skewness: {df['returns'].skew():.4f}")
    print(f"Kurtosis: {df['returns'].kurtosis():.4f}")
    print()

    # Percentiles
    print("Return percentiles:")
    for p in [1, 5, 10, 25, 50, 75, 90, 95, 99]:
        val = df['returns'].quantile(p/100)
        print(f"  {p:2d}th: {val:+.6f} ({val*100:+.4f}%)")
    print()

    # === 3. BOLLINGER BAND ANALYSIS ===
    print("=" * 70)
    print("3. BOLLINGER BAND ANALYSIS")
    print("=" * 70)

    # Test different BB parameters
    for period in [10, 15, 20, 30]:
        for std_mult in [1.5, 2.0, 2.5, 3.0]:
            df[f'bb_middle_{period}'] = df['close'].rolling(period).mean()
            df[f'bb_std_{period}'] = df['close'].rolling(period).std()
            df[f'bb_upper_{period}_{std_mult}'] = df[f'bb_middle_{period}'] + std_mult * df[f'bb_std_{period}']
            df[f'bb_lower_{period}_{std_mult}'] = df[f'bb_middle_{period}'] - std_mult * df[f'bb_std_{period}']

            # Calculate breakout frequency
            upper_breakouts = (df['close'] > df[f'bb_upper_{period}_{std_mult}']).sum()
            lower_breakouts = (df['close'] < df[f'bb_lower_{period}_{std_mult}']).sum()
            total_breakouts = upper_breakouts + lower_breakouts
            breakout_pct = (total_breakouts / len(df)) * 100

            if period == 20 and std_mult == 2.0:
                print(f"Current parameters (period={period}, std={std_mult}):")
                print(f"  Upper breakouts: {upper_breakouts:,} ({upper_breakouts/len(df)*100:.2f}%)")
                print(f"  Lower breakouts: {lower_breakouts:,} ({lower_breakouts/len(df)*100:.2f}%)")
                print(f"  Total breakouts: {total_breakouts:,} ({breakout_pct:.2f}%)")
                print()

    # Recommended BB parameters
    print("Bollinger Band parameter recommendations:")
    print("  Current (QQQ): period=20, std=2.0 → 4.92% breakout rate")
    print("  For SPY: Consider period=20, std=2.0 (similar volatility) or period=20, std=2.5 (fewer signals)")
    print()

    # === 4. HOLDING PERIOD ANALYSIS ===
    print("=" * 70)
    print("4. HOLDING PERIOD ANALYSIS")
    print("=" * 70)

    # Calculate forward returns for different holding periods
    for horizon in [1, 3, 5, 10, 20]:
        df[f'forward_return_{horizon}'] = df['close'].pct_change(horizon).shift(-horizon)

        mean_fwd = df[f'forward_return_{horizon}'].mean()
        std_fwd = df[f'forward_return_{horizon}'].std()
        sharpe = mean_fwd / std_fwd if std_fwd > 0 else 0

        print(f"{horizon}-bar forward returns:")
        print(f"  Mean: {mean_fwd:.6f} ({mean_fwd*100:.4f}%)")
        print(f"  Std: {std_fwd:.6f} ({std_fwd*100:.4f}%)")
        print(f"  Sharpe: {sharpe:.4f}")
        print(f"  95th percentile: {df[f'forward_return_{horizon}'].quantile(0.95):.4f} ({df[f'forward_return_{horizon}'].quantile(0.95)*100:.2f}%)")
        print(f"  5th percentile: {df[f'forward_return_{horizon}'].quantile(0.05):.4f} ({df[f'forward_return_{horizon}'].quantile(0.05)*100:.2f}%)")
        print()

    # === 5. PROFIT TARGET / STOP LOSS ANALYSIS ===
    print("=" * 70)
    print("5. PROFIT TARGET / STOP LOSS RECOMMENDATIONS")
    print("=" * 70)

    # Current QQQ parameters
    print("Current QQQ parameters:")
    print("  Long profit target: +2.0%")
    print("  Short profit target: -1.5%")
    print("  Stop loss: -1.0% (typical)")
    print()

    # Analyze what percentage of trades would hit these targets
    # Use 10-bar forward returns as proxy for trade outcomes
    forward_10 = df['forward_return_10'].dropna()

    long_hit_2pct = (forward_10 >= 0.020).sum() / len(forward_10) * 100
    long_hit_15pct = (forward_10 >= 0.015).sum() / len(forward_10) * 100
    long_hit_1pct = (forward_10 >= 0.010).sum() / len(forward_10) * 100

    short_hit_15pct = (forward_10 <= -0.015).sum() / len(forward_10) * 100
    short_hit_1pct = (forward_10 <= -0.010).sum() / len(forward_10) * 100

    print("Probability of hitting targets within 10 bars:")
    print(f"  Long +2.0%: {long_hit_2pct:.2f}%")
    print(f"  Long +1.5%: {long_hit_15pct:.2f}%")
    print(f"  Long +1.0%: {long_hit_1pct:.2f}%")
    print(f"  Short -1.5%: {short_hit_15pct:.2f}%")
    print(f"  Short -1.0%: {short_hit_1pct:.2f}%")
    print()

    # Recommendations
    print("SPY Recommendations:")
    avg_10bar_move = df['forward_return_10'].abs().mean()
    print(f"  Average 10-bar absolute move: {avg_10bar_move:.4f} ({avg_10bar_move*100:.2f}%)")

    recommended_long_target = df['forward_return_10'].quantile(0.75)
    recommended_short_target = df['forward_return_10'].quantile(0.25)

    print(f"  Recommended long profit target: {recommended_long_target:.4f} ({recommended_long_target*100:.2f}%)")
    print(f"  Recommended short profit target: {abs(recommended_short_target):.4f} ({abs(recommended_short_target)*100:.2f}%)")
    print(f"  Recommended stop loss: {df['forward_return_10'].quantile(0.10):.4f} ({abs(df['forward_return_10'].quantile(0.10))*100:.2f}%)")
    print()

    # === 6. SIGNAL AMPLIFICATION RECOMMENDATIONS ===
    print("=" * 70)
    print("6. SIGNAL AMPLIFICATION FACTOR RECOMMENDATIONS")
    print("=" * 70)

    # Based on volatility ratio QQQ/SPY
    print("Current QQQ amplification factors:")
    print("  Bollinger breakout: 2.0x - 3.0x")
    print("  High probability signals: 1.5x - 2.0x")
    print()

    # SPY is typically ~70-80% as volatile as QQQ
    spy_to_qqq_vol_ratio = 0.75  # Typical estimate

    print(f"SPY/QQQ volatility ratio: ~{spy_to_qqq_vol_ratio:.2f}")
    print("SPY Recommendations:")
    print(f"  Bollinger breakout: {2.0*spy_to_qqq_vol_ratio:.2f}x - {3.0*spy_to_qqq_vol_ratio:.2f}x")
    print(f"  High probability signals: {1.5*spy_to_qqq_vol_ratio:.2f}x - {2.0*spy_to_qqq_vol_ratio:.2f}x")
    print()

    # === SUMMARY ===
    print("=" * 70)
    print("SUMMARY: RECOMMENDED SPY RECALIBRATION")
    print("=" * 70)
    print()

    recommendations = {
        "bollinger_bands": {
            "period": 20,
            "std_multiplier": 2.0,
            "note": "Keep same as QQQ - similar characteristics"
        },
        "psm_thresholds": {
            "long_profit_target": f"{recommended_long_target*100:.2f}%",
            "short_profit_target": f"{abs(recommended_short_target)*100:.2f}%",
            "stop_loss": f"{abs(df['forward_return_10'].quantile(0.10))*100:.2f}%",
            "min_hold_bars": 3,
            "note": "Based on 10-bar forward return quantiles"
        },
        "signal_amplification": {
            "bollinger_breakout_mult": f"{2.5*spy_to_qqq_vol_ratio:.2f}",
            "high_prob_mult": f"{1.75*spy_to_qqq_vol_ratio:.2f}",
            "note": "Scaled by SPY/QQQ volatility ratio (~0.75)"
        },
        "feature_weights": {
            "note": "EWRLS learns automatically during warmup - no manual tuning needed"
        }
    }

    print(json.dumps(recommendations, indent=2))
    print()

    # Save recommendations
    output_file = "/Volumes/ExternalSSD/Dev/C++/online_trader/data/tmp/spy_calibration_recommendations.json"
    with open(output_file, 'w') as f:
        json.dump(recommendations, f, indent=2)

    print(f"✅ Recommendations saved to: {output_file}")
    print()

    return recommendations

if __name__ == "__main__":
    filepath = "/Volumes/ExternalSSD/Dev/C++/online_trader/data/equities/SPY_RTH_NH_5years.csv"
    analyze_spy_data(filepath)
