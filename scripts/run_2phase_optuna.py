#!/usr/bin/env python3
"""
2-Phase Optuna Optimization for Live Trading Launch

Phase 1: Optimize primary parameters (50 trials)
  - buy_threshold, sell_threshold, ewrls_lambda, bb_amplification_factor

Phase 2: Optimize secondary parameters using Phase 1 best params (50 trials)
  - horizon_weights (h1, h5, h10), bb_period, bb_std_dev, bb_proximity, regularization

Saves best params to config/best_params.json for live trading.

Author: Claude Code
Date: 2025-10-09
"""

import os
import sys
import json
import time
import subprocess
import argparse
from pathlib import Path
from typing import Dict, Optional
from datetime import datetime

import optuna
import pandas as pd
import numpy as np


class TwoPhaseOptuna:
    """2-Phase Optuna optimization for pre-market launch."""

    def __init__(self,
                 data_file: str,
                 build_dir: str,
                 output_dir: str,
                 n_trials_phase1: int = 50,
                 n_trials_phase2: int = 50,
                 n_jobs: int = 4):
        self.data_file = data_file
        self.build_dir = build_dir
        self.output_dir = output_dir
        self.sentio_cli = os.path.join(build_dir, "sentio_cli")
        self.n_trials_phase1 = n_trials_phase1
        self.n_trials_phase2 = n_trials_phase2
        self.n_jobs = n_jobs

        # Create output directory
        Path(output_dir).mkdir(parents=True, exist_ok=True)

        # Load data
        full_df = pd.read_csv(data_file)
        total_bars = len(full_df)
        total_blocks = total_bars // 391

        # Limit to most recent 100 blocks (~6 months) for optimization speed
        # Recent data is more relevant and EOD validation is computationally expensive
        max_blocks = 100
        if total_blocks > max_blocks:
            start_idx = total_bars - (max_blocks * 391)
            self.df = full_df.iloc[start_idx:].reset_index(drop=True)
            print(f"[2PhaseOptuna] Full dataset: {total_bars} bars ({total_blocks} blocks)")
            print(f"[2PhaseOptuna] Using recent {len(self.df)} bars ({max_blocks} blocks) for optimization")
        else:
            self.df = full_df
            print(f"[2PhaseOptuna] Loaded {total_bars} bars ({total_blocks} blocks)")

        self.total_bars = len(self.df)
        self.bars_per_block = 391
        self.total_blocks = self.total_bars // self.bars_per_block

        print(f"[2PhaseOptuna] Phase 1 trials: {self.n_trials_phase1}")
        print(f"[2PhaseOptuna] Phase 2 trials: {self.n_trials_phase2}")
        print(f"[2PhaseOptuna] Parallel jobs: {self.n_jobs}")
        print()

    def run_backtest_with_eod_validation(self, params: Dict, warmup_blocks: int = 10) -> Dict:
        """Run backtest with strict EOD enforcement between blocks."""

        # Constants
        BARS_PER_DAY = 391  # 9:30 AM - 4:00 PM inclusive
        BARS_PER_BLOCK = 391  # Ensure blocks align with trading days

        # Parse data to identify trading days
        if 'timestamp_dt' not in self.df.columns:
            # Check which timestamp column exists
            if 'timestamp' in self.df.columns:
                self.df['timestamp_dt'] = pd.to_datetime(self.df['timestamp'], unit='ms')
            elif 'ts_nyt_epoch' in self.df.columns:
                self.df['timestamp_dt'] = pd.to_datetime(self.df['ts_nyt_epoch'], unit='s')
            elif 'ts_utc' in self.df.columns:
                self.df['timestamp_dt'] = pd.to_datetime(self.df['ts_utc'])
            else:
                return {'mrd': -999.0, 'error': 'No timestamp column found'}

        if 'trading_date' not in self.df.columns:
            self.df['trading_date'] = self.df['timestamp_dt'].dt.date

        # Group by trading days
        daily_groups = self.df.groupby('trading_date')
        trading_days = sorted(daily_groups.groups.keys())

        # Skip warmup days
        warmup_days = warmup_blocks
        test_days = trading_days[warmup_days:]

        if len(test_days) == 0:
            print(f"ERROR: Insufficient data - have {len(trading_days)} days, need >{warmup_blocks}")
            return {'mrd': -999.0, 'error': 'Insufficient data after warmup'}

        print(f"  Processing {len(test_days)} test days (warmup={warmup_days} days)", end="... ")

        # Track daily returns for MRD calculation
        daily_returns = []
        cumulative_trades = []
        errors = {'signal_gen': 0, 'trade_exec': 0, 'no_trades': 0, 'eod_check': 0}

        for day_idx, trading_date in enumerate(test_days):
            day_data = daily_groups.get_group(trading_date)
            day_bars = len(day_data)

            # Create temporary files for this day's backtest (include SPY in filename for symbol detection)
            day_signals_file = f"{self.output_dir}/day_{day_idx}_SPY_signals.jsonl"
            day_trades_file = f"{self.output_dir}/day_{day_idx}_SPY_trades.jsonl"
            day_data_file = f"{self.output_dir}/day_{day_idx}_SPY_data.csv"

            # Include warmup data + current day
            warmup_start_idx = max(0, day_data.index[0] - warmup_blocks * BARS_PER_DAY)
            day_with_warmup = self.df.iloc[warmup_start_idx:day_data.index[-1] + 1]
            day_with_warmup.to_csv(day_data_file, index=False)

            # Generate signals for the day
            cmd_generate = [
                self.sentio_cli, "generate-signals",
                "--data", day_data_file,
                "--output", day_signals_file,
                "--warmup", str(warmup_blocks * BARS_PER_DAY),
                "--buy-threshold", str(params['buy_threshold']),
                "--sell-threshold", str(params['sell_threshold']),
                "--lambda", str(params['ewrls_lambda']),
                "--bb-amp", str(params['bb_amplification_factor'])
            ]

            # Add phase 2 parameters if present
            if 'h1_weight' in params:
                cmd_generate.extend(["--h1-weight", str(params['h1_weight'])])
            if 'h5_weight' in params:
                cmd_generate.extend(["--h5-weight", str(params['h5_weight'])])
            if 'h10_weight' in params:
                cmd_generate.extend(["--h10-weight", str(params['h10_weight'])])
            if 'bb_period' in params:
                cmd_generate.extend(["--bb-period", str(params['bb_period'])])
            if 'bb_std_dev' in params:
                cmd_generate.extend(["--bb-std-dev", str(params['bb_std_dev'])])
            if 'bb_proximity' in params:
                cmd_generate.extend(["--bb-proximity", str(params['bb_proximity'])])
            if 'regularization' in params:
                cmd_generate.extend(["--regularization", str(params['regularization'])])

            try:
                result = subprocess.run(cmd_generate, capture_output=True, text=True, timeout=60)
                if result.returncode != 0:
                    errors['signal_gen'] += 1
                    continue  # Skip failed days
            except subprocess.TimeoutExpired:
                errors['signal_gen'] += 1
                continue

            # Execute trades with EOD enforcement
            cmd_execute = [
                self.sentio_cli, "execute-trades",
                "--signals", day_signals_file,
                "--data", day_data_file,
                "--output", day_trades_file,
                "--warmup", str(warmup_blocks * BARS_PER_DAY)
            ]

            try:
                result = subprocess.run(cmd_execute, capture_output=True, text=True, timeout=60)
                if result.returncode != 0:
                    errors['trade_exec'] += 1
                    continue
            except subprocess.TimeoutExpired:
                errors['trade_exec'] += 1
                continue

            # Validate EOD closure - parse trades and check final position
            try:
                with open(day_trades_file, 'r') as f:
                    trades = [json.loads(line) for line in f if line.strip()]
            except:
                errors['no_trades'] += 1
                trades = []

            if trades:
                last_trade = trades[-1]
                final_bar_index = last_trade.get('bar_index', 0)

                # Verify last trade is near EOD (within last 3 bars of day)
                expected_last_bar = warmup_blocks * BARS_PER_DAY + day_bars - 1
                if final_bar_index < expected_last_bar - 3:
                    print(f"WARNING: Day {trading_date} - Last trade at bar {final_bar_index}, "
                          f"expected near {expected_last_bar}")

                # Check position is flat (cash only)
                final_positions = last_trade.get('positions', {})
                has_open_position = False
                if final_positions:
                    for pos in (final_positions.values() if isinstance(final_positions, dict) else final_positions):
                        if isinstance(pos, dict) and pos.get('quantity', 0) != 0:
                            has_open_position = True
                            break

                if has_open_position:
                    print(f"ERROR: Day {trading_date} - Positions not closed at EOD!")
                    print(f"  Remaining positions: {final_positions}")
                    # Force return 0 for this day
                    daily_returns.append(0.0)
                else:
                    # Calculate day's return
                    starting_equity = 100000.0  # Reset each day
                    ending_equity = last_trade.get('portfolio_value', starting_equity)
                    day_return = (ending_equity - starting_equity) / starting_equity
                    daily_returns.append(day_return)

                    # Store trades for analysis
                    cumulative_trades.extend(trades)
            else:
                daily_returns.append(0.0)  # No trades = 0 return

            # Clean up temporary files
            for temp_file in [day_signals_file, day_trades_file, day_data_file]:
                if os.path.exists(temp_file):
                    try:
                        os.remove(temp_file)
                    except:
                        pass

        # Calculate MRD (Mean Return per Day)
        if daily_returns:
            mrd = np.mean(daily_returns) * 100  # Convert to percentage
            print(f"✓ ({len(daily_returns)} days, {len(cumulative_trades)} trades)")

            # Sanity check
            if abs(mrd) > 5.0:  # Flag if > 5% daily return
                print(f"WARNING: Unrealistic MRD detected: {mrd:.2f}%")
                print(f"  Daily returns: {[f'{r*100:.2f}%' for r in daily_returns[:5]]}")

            return {
                'mrd': mrd,
                'daily_returns': daily_returns,
                'num_days': len(daily_returns),
                'total_trades': len(cumulative_trades)
            }
        else:
            print(f"✗ All days failed!")
            print(f"  Signal gen errors: {errors['signal_gen']}")
            print(f"  Trade exec errors: {errors['trade_exec']}")
            print(f"  Parse errors: {errors['no_trades']}")
            print(f"  EOD errors: {errors['eod_check']}")
            return {'mrd': -999.0, 'error': 'No valid trading days'}

    def phase1_optimize(self) -> Dict:
        """
        Phase 1: Optimize primary parameters on full dataset.

        Returns best parameters and MRD.
        """
        print("=" * 80)
        print("PHASE 1: PRIMARY PARAMETER OPTIMIZATION")
        print("=" * 80)
        print(f"Target: Find best buy/sell thresholds, lambda, BB amplification")
        print(f"Trials: {self.n_trials_phase1}")
        print(f"Data: {self.total_blocks} blocks")
        print()

        def objective(trial):
            params = {
                'buy_threshold': trial.suggest_float('buy_threshold', 0.50, 0.65, step=0.01),
                'sell_threshold': trial.suggest_float('sell_threshold', 0.35, 0.50, step=0.01),
                'ewrls_lambda': trial.suggest_float('ewrls_lambda', 0.985, 0.999, step=0.001),
                'bb_amplification_factor': trial.suggest_float('bb_amplification_factor', 0.00, 0.20, step=0.01)
            }

            # Ensure asymmetric thresholds
            if params['buy_threshold'] <= params['sell_threshold']:
                return -999.0

            result = self.run_backtest_with_eod_validation(params, warmup_blocks=10)

            mrd = result.get('mrd', result.get('mrb', 0.0))
            mrb = result.get('mrb', 0.0)
            print(f"  Trial {trial.number:3d}: MRD={mrd:+7.4f}% (MRB={mrb:+7.4f}%) | "
                  f"buy={params['buy_threshold']:.2f} sell={params['sell_threshold']:.2f} "
                  f"λ={params['ewrls_lambda']:.3f} BB={params['bb_amplification_factor']:.2f}")

            return mrd  # Optimize for MRD (daily returns)

        start_time = time.time()
        study = optuna.create_study(
            direction='maximize',
            sampler=optuna.samplers.TPESampler(seed=42)
        )
        study.optimize(objective, n_trials=self.n_trials_phase1, n_jobs=self.n_jobs, show_progress_bar=True)
        elapsed = time.time() - start_time

        best_params = study.best_params
        best_mrd = study.best_value

        print()
        print(f"✓ Phase 1 Complete in {elapsed:.1f}s ({elapsed/60:.1f} min)")
        print(f"✓ Best MRD: {best_mrd:.4f}%")
        print(f"✓ Best params:")
        for key, value in best_params.items():
            print(f"    {key:25s} = {value}")
        print()

        return best_params, best_mrd

    def phase2_optimize(self, phase1_params: Dict) -> Dict:
        """
        Phase 2: Optimize secondary parameters using Phase 1 best params.

        Returns best parameters and MRD.
        """
        print("=" * 80)
        print("PHASE 2: SECONDARY PARAMETER OPTIMIZATION")
        print("=" * 80)
        print(f"Target: Fine-tune horizon weights, BB params, regularization")
        print(f"Trials: {self.n_trials_phase2}")
        print(f"Phase 1 params (FIXED):")
        for key, value in phase1_params.items():
            print(f"  {key:25s} = {value}")
        print()

        def objective(trial):
            # Sample 2 weights, compute 3rd to ensure sum = 1.0
            h1_weight = trial.suggest_float('h1_weight', 0.1, 0.6, step=0.05)
            h5_weight = trial.suggest_float('h5_weight', 0.2, 0.7, step=0.05)
            h10_weight = 1.0 - h1_weight - h5_weight

            # Reject if h10 out of range
            if h10_weight < 0.05 or h10_weight > 0.6:
                return -999.0

            params = {
                # Phase 1 params FIXED
                'buy_threshold': phase1_params['buy_threshold'],
                'sell_threshold': phase1_params['sell_threshold'],
                'ewrls_lambda': phase1_params['ewrls_lambda'],
                'bb_amplification_factor': phase1_params['bb_amplification_factor'],

                # Phase 2 params OPTIMIZED
                'h1_weight': h1_weight,
                'h5_weight': h5_weight,
                'h10_weight': h10_weight,
                'bb_period': trial.suggest_int('bb_period', 5, 40, step=5),
                'bb_std_dev': trial.suggest_float('bb_std_dev', 1.0, 3.0, step=0.25),
                'bb_proximity': trial.suggest_float('bb_proximity', 0.10, 0.50, step=0.05),
                'regularization': trial.suggest_float('regularization', 0.0, 0.10, step=0.005)
            }

            result = self.run_backtest_with_eod_validation(params, warmup_blocks=10)

            mrd = result.get('mrd', result.get('mrb', 0.0))
            mrb = result.get('mrb', 0.0)
            print(f"  Trial {trial.number:3d}: MRD={mrd:+7.4f}% (MRB={mrb:+7.4f}%) | "
                  f"h=({h1_weight:.2f},{h5_weight:.2f},{h10_weight:.2f}) "
                  f"BB({params['bb_period']},{params['bb_std_dev']:.1f}) "
                  f"prox={params['bb_proximity']:.2f} reg={params['regularization']:.3f}")

            return mrd  # Optimize for MRD (daily returns)

        start_time = time.time()
        study = optuna.create_study(
            direction='maximize',
            sampler=optuna.samplers.TPESampler(seed=42)
        )
        study.optimize(objective, n_trials=self.n_trials_phase2, n_jobs=self.n_jobs, show_progress_bar=True)
        elapsed = time.time() - start_time

        best_params = study.best_params.copy()
        best_mrd = study.best_value

        # Add Phase 1 params to final result
        best_params.update(phase1_params)

        print()
        print(f"✓ Phase 2 Complete in {elapsed:.1f}s ({elapsed/60:.1f} min)")
        print(f"✓ Best MRD: {best_mrd:.4f}%")
        print(f"✓ Best params (Phase 1 + Phase 2):")
        for key, value in best_params.items():
            print(f"    {key:25s} = {value}")
        print()

        return best_params, best_mrd

    def save_best_params(self, params: Dict, mrd: float, output_file: str):
        """Save best parameters to JSON file for live trading."""
        output = {
            "last_updated": datetime.now().strftime("%Y-%m-%dT%H:%M:%SZ"),
            "optimization_source": "2phase_optuna_premarket",
            "optimization_date": datetime.now().strftime("%Y-%m-%d"),
            "data_used": os.path.basename(self.data_file),
            "n_trials_phase1": self.n_trials_phase1,
            "n_trials_phase2": self.n_trials_phase2,
            "best_mrd": round(mrd, 4),
            "parameters": {
                "buy_threshold": params['buy_threshold'],
                "sell_threshold": params['sell_threshold'],
                "ewrls_lambda": params['ewrls_lambda'],
                "bb_amplification_factor": params['bb_amplification_factor'],
                "h1_weight": params.get('h1_weight', 0.3),
                "h5_weight": params.get('h5_weight', 0.5),
                "h10_weight": params.get('h10_weight', 0.2),
                "bb_period": int(params.get('bb_period', 20)),
                "bb_std_dev": params.get('bb_std_dev', 2.0),
                "bb_proximity": params.get('bb_proximity', 0.30),
                "regularization": params.get('regularization', 0.01)
            },
            "note": f"Optimized for live trading session on {datetime.now().strftime('%Y-%m-%d')}"
        }

        with open(output_file, 'w') as f:
            json.dump(output, f, indent=2)

        print(f"✓ Saved best parameters to: {output_file}")

    def run(self, output_file: str) -> Dict:
        """Run 2-phase optimization and save results."""
        total_start = time.time()

        # Phase 1: Primary parameters
        phase1_params, phase1_mrd = self.phase1_optimize()

        # Phase 2: Secondary parameters
        final_params, final_mrd = self.phase2_optimize(phase1_params)

        # Save to output file
        self.save_best_params(final_params, final_mrd, output_file)

        total_elapsed = time.time() - total_start

        print("=" * 80)
        print("2-PHASE OPTIMIZATION COMPLETE")
        print("=" * 80)
        print(f"Total time: {total_elapsed/60:.1f} minutes")
        print(f"Phase 1 MRD: {phase1_mrd:.4f}%")
        print(f"Phase 2 MRD: {final_mrd:.4f}%")
        print(f"Improvement: {(final_mrd - phase1_mrd):.4f}%")
        print(f"Parameters saved to: {output_file}")
        print("=" * 80)

        return final_params


class MarketRegimeDetector:
    """Detect market regime for adaptive parameter ranges"""

    def __init__(self, lookback_periods: int = 20):
        self.lookback_periods = lookback_periods

    def detect_regime(self, data: pd.DataFrame) -> str:
        """Detect current market regime based on recent data"""

        # Calculate recent volatility (20-bar rolling std of returns)
        data_copy = data.copy()
        data_copy['returns'] = data_copy['close'].pct_change()
        recent_vol = data_copy['returns'].tail(self.lookback_periods).std()

        # Calculate trend strength (linear regression slope)
        recent_prices = data_copy['close'].tail(self.lookback_periods).values
        x = np.arange(len(recent_prices))
        slope, _ = np.polyfit(x, recent_prices, 1)
        normalized_slope = slope / np.mean(recent_prices)

        # Classify regime
        if recent_vol > 0.02:
            return "HIGH_VOLATILITY"
        elif abs(normalized_slope) > 0.001:
            return "TRENDING"
        else:
            return "CHOPPY"

    def get_adaptive_ranges(self, regime: str) -> Dict:
        """Get parameter ranges based on market regime"""

        if regime == "HIGH_VOLATILITY":
            # Require 0.08 gap: buy_min=0.53, sell_max=0.45 → gap=0.08
            return {
                'buy_threshold': (0.53, 0.70),
                'sell_threshold': (0.30, 0.45),
                'ewrls_lambda': (0.980, 0.995),  # Faster adaptation
                'bb_amplification_factor': (0.05, 0.30),
                'bb_period': (10, 30),  # Shorter periods
                'bb_std_dev': (1.5, 3.0),
                'regularization': (0.01, 0.10)
            }
        elif regime == "TRENDING":
            # Require 0.04 gap: buy_min=0.52, sell_max=0.48 → gap=0.04
            return {
                'buy_threshold': (0.52, 0.62),
                'sell_threshold': (0.38, 0.48),
                'ewrls_lambda': (0.990, 0.999),  # Slower adaptation
                'bb_amplification_factor': (0.00, 0.15),
                'bb_period': (20, 40),
                'bb_std_dev': (2.0, 2.5),
                'regularization': (0.00, 0.05)
            }
        else:  # CHOPPY
            # Require 0.04 gap: buy_min=0.52, sell_max=0.48 → gap=0.04
            return {
                'buy_threshold': (0.52, 0.60),
                'sell_threshold': (0.40, 0.48),
                'ewrls_lambda': (0.985, 0.997),
                'bb_amplification_factor': (0.10, 0.25),
                'bb_period': (15, 35),
                'bb_std_dev': (1.75, 2.5),
                'regularization': (0.005, 0.08)
            }


class AdaptiveTwoPhaseOptuna(TwoPhaseOptuna):
    """Enhanced optimizer with adaptive parameter ranges"""

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.regime_detector = MarketRegimeDetector()

    def phase1_optimize(self) -> Dict:
        """Phase 1 with adaptive ranges based on market regime"""

        # Detect current market regime
        current_regime = self.regime_detector.detect_regime(self.df)
        adaptive_ranges = self.regime_detector.get_adaptive_ranges(current_regime)

        print("=" * 80)
        print("PHASE 1: ADAPTIVE PRIMARY PARAMETER OPTIMIZATION")
        print("=" * 80)
        print(f"Detected Market Regime: {current_regime}")
        print(f"Adaptive Ranges:")
        for param, range_val in adaptive_ranges.items():
            if param in ['buy_threshold', 'sell_threshold', 'ewrls_lambda', 'bb_amplification_factor']:
                print(f"  {param:25s}: {range_val}")
        print()

        def objective(trial):
            # Use adaptive ranges
            params = {
                'buy_threshold': trial.suggest_float(
                    'buy_threshold',
                    adaptive_ranges['buy_threshold'][0],
                    adaptive_ranges['buy_threshold'][1],
                    step=0.01
                ),
                'sell_threshold': trial.suggest_float(
                    'sell_threshold',
                    adaptive_ranges['sell_threshold'][0],
                    adaptive_ranges['sell_threshold'][1],
                    step=0.01
                ),
                'ewrls_lambda': trial.suggest_float(
                    'ewrls_lambda',
                    adaptive_ranges['ewrls_lambda'][0],
                    adaptive_ranges['ewrls_lambda'][1],
                    step=0.001
                ),
                'bb_amplification_factor': trial.suggest_float(
                    'bb_amplification_factor',
                    adaptive_ranges['bb_amplification_factor'][0],
                    adaptive_ranges['bb_amplification_factor'][1],
                    step=0.01
                )
            }

            # Ensure asymmetric thresholds with regime-specific gap
            min_gap = 0.08 if current_regime == "HIGH_VOLATILITY" else 0.04
            if params['buy_threshold'] - params['sell_threshold'] < min_gap:
                return -999.0

            # Use EOD-enforced backtest
            result = self.run_backtest_with_eod_validation(params, warmup_blocks=10)

            mrd = result.get('mrd', -999.0)

            # Penalize extreme MRD values
            if abs(mrd) > 2.0:  # More than 2% daily is suspicious
                print(f"  WARNING: Trial {trial.number} has extreme MRD: {mrd:.4f}%")
                return -999.0

            print(f"  Trial {trial.number:3d}: MRD={mrd:+7.4f}% | "
                  f"buy={params['buy_threshold']:.2f} sell={params['sell_threshold']:.2f}")

            return mrd

        # Run optimization
        start_time = time.time()
        study = optuna.create_study(
            direction='maximize',
            sampler=optuna.samplers.TPESampler(seed=42),
            pruner=optuna.pruners.MedianPruner()  # Add pruning for efficiency
        )
        study.optimize(
            objective,
            n_trials=self.n_trials_phase1,
            n_jobs=self.n_jobs,
            show_progress_bar=True
        )
        elapsed = time.time() - start_time

        best_params = study.best_params
        best_mrd = study.best_value

        print()
        print(f"✓ Phase 1 Complete in {elapsed:.1f}s ({elapsed/60:.1f} min)")
        print(f"✓ Best MRD: {best_mrd:.4f}%")
        print(f"✓ Best params:")
        for key, value in best_params.items():
            print(f"    {key:25s} = {value}")
        print()

        return best_params, best_mrd


def main():
    parser = argparse.ArgumentParser(description="2-Phase Optuna Optimization for Live Trading")
    parser.add_argument('--data', required=True, help='Path to data CSV file')
    parser.add_argument('--build-dir', default='build', help='Path to build directory')
    parser.add_argument('--output', required=True, help='Path to output JSON file (e.g., config/best_params.json)')
    parser.add_argument('--n-trials-phase1', type=int, default=50, help='Phase 1 trials (default: 50)')
    parser.add_argument('--n-trials-phase2', type=int, default=50, help='Phase 2 trials (default: 50)')
    parser.add_argument('--n-jobs', type=int, default=4, help='Parallel jobs (default: 4)')

    args = parser.parse_args()

    # Determine project root
    script_dir = Path(__file__).parent
    project_root = script_dir.parent
    build_dir = project_root / args.build_dir
    output_dir = project_root / "data" / "tmp" / "optuna_premarket"

    print("=" * 80)
    print("2-PHASE OPTUNA OPTIMIZATION FOR LIVE TRADING")
    print("=" * 80)
    print(f"Data: {args.data}")
    print(f"Build: {build_dir}")
    print(f"Output: {args.output}")
    print("=" * 80)
    print()

    # Run optimization with adaptive regime-aware optimizer
    optimizer = AdaptiveTwoPhaseOptuna(
        data_file=args.data,
        build_dir=str(build_dir),
        output_dir=str(output_dir),
        n_trials_phase1=args.n_trials_phase1,
        n_trials_phase2=args.n_trials_phase2,
        n_jobs=args.n_jobs
    )

    optimizer.run(args.output)


if __name__ == '__main__':
    main()
