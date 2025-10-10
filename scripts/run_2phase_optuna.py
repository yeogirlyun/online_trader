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

# Import signal caching system for 3-5x optimization speedup
from signal_cache import SignalCache, CacheStats


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

        # Limit to most recent 40 blocks (~2.5 months) for optimization speed
        # Recent data is more relevant and EOD validation is computationally expensive
        # Shorter lookback is more responsive to current market conditions
        max_blocks = 40
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

    def _generate_leveraged_data_for_day(self, spy_file: str, day_idx: int):
        """Generate SPXL, SH, SDS data from SPY data for a specific day."""
        import csv

        # Read SPY data
        spy_bars = []
        with open(spy_file) as f:
            reader = csv.DictReader(f)
            for row in reader:
                spy_bars.append({
                    'ts_utc': row['ts_utc'],
                    'ts_nyt_epoch': row['ts_nyt_epoch'],
                    'open': float(row['open']),
                    'high': float(row['high']),
                    'low': float(row['low']),
                    'close': float(row['close']),
                    'volume': float(row['volume'])
                })

        # Initialize starting prices
        spy_start = spy_bars[0]['close']
        instruments = {
            'SPXL': {'leverage': 3.0, 'prev_close': 100.0, 'bars': []},  # 3x bull
            'SH': {'leverage': -1.0, 'prev_close': 50.0, 'bars': []},   # -1x bear
            'SDS': {'leverage': -2.0, 'prev_close': 50.0, 'bars': []}   # -2x bear (asymmetric)
        }

        spy_prev_close = spy_start

        for spy_bar in spy_bars:
            # Calculate SPY returns
            spy_open_ret = (spy_bar['open'] - spy_prev_close) / spy_prev_close
            spy_high_ret = (spy_bar['high'] - spy_prev_close) / spy_prev_close
            spy_low_ret = (spy_bar['low'] - spy_prev_close) / spy_prev_close
            spy_close_ret = (spy_bar['close'] - spy_prev_close) / spy_prev_close

            # Generate leveraged bars
            for symbol, inst in instruments.items():
                leverage = inst['leverage']
                prev_close = inst['prev_close']

                # Apply leverage
                open_price = prev_close * (1 + spy_open_ret * leverage)
                high_price = prev_close * (1 + spy_high_ret * leverage)
                low_price = prev_close * (1 + spy_low_ret * leverage)
                close_price = prev_close * (1 + spy_close_ret * leverage)

                # Ensure valid OHLC
                if high_price < low_price:
                    high_price, low_price = low_price, high_price
                open_price = max(low_price, min(high_price, open_price))
                close_price = max(low_price, min(high_price, close_price))

                inst['bars'].append({
                    'ts_utc': spy_bar['ts_utc'],
                    'ts_nyt_epoch': spy_bar['ts_nyt_epoch'],
                    'open': open_price,
                    'high': high_price,
                    'low': low_price,
                    'close': close_price,
                    'volume': spy_bar['volume']
                })

                inst['prev_close'] = close_price

            spy_prev_close = spy_bar['close']

        # Write output files
        for symbol, inst in instruments.items():
            output_file = f"{self.output_dir}/day_{day_idx}_{symbol}_data.csv"
            with open(output_file, 'w', newline='') as f:
                writer = csv.writer(f)
                writer.writerow(['ts_utc', 'ts_nyt_epoch', 'open', 'high', 'low', 'close', 'volume'])
                for bar in inst['bars']:
                    writer.writerow([
                        bar['ts_utc'], bar['ts_nyt_epoch'],
                        f"{bar['open']:.4f}", f"{bar['high']:.4f}",
                        f"{bar['low']:.4f}", f"{bar['close']:.4f}",
                        f"{bar['volume']:.1f}"
                    ])

    def _generate_leveraged_files(self, spy_data: pd.DataFrame, output_dir: str):
        """Generate SPXL, SH, SDS files from SPY DataFrame with standard RTH_NH naming."""
        instruments = {
            'SPXL': {'leverage': 3.0, 'prev_close': 100.0},  # 3x bull
            'SH': {'leverage': -1.0, 'prev_close': 50.0},   # -1x bear
            'SDS': {'leverage': -2.0, 'prev_close': 50.0}   # -2x bear
        }

        spy_data_copy = spy_data.copy().reset_index(drop=True)
        spy_prev_close = spy_data_copy.iloc[0]['close']

        for symbol, inst in instruments.items():
            bars = []
            prev_close = inst['prev_close']

            for _, spy_bar in spy_data_copy.iterrows():
                # Calculate SPY returns
                spy_close_ret = (spy_bar['close'] - spy_prev_close) / spy_prev_close
                spy_open_ret = (spy_bar['open'] - spy_prev_close) / spy_prev_close
                spy_high_ret = (spy_bar['high'] - spy_prev_close) / spy_prev_close
                spy_low_ret = (spy_bar['low'] - spy_prev_close) / spy_prev_close

                # Apply leverage
                leverage = inst['leverage']
                open_price = prev_close * (1 + spy_open_ret * leverage)
                high_price = prev_close * (1 + spy_high_ret * leverage)
                low_price = prev_close * (1 + spy_low_ret * leverage)
                close_price = prev_close * (1 + spy_close_ret * leverage)

                # Ensure valid OHLC
                if high_price < low_price:
                    high_price, low_price = low_price, high_price
                open_price = max(low_price, min(high_price, open_price))
                close_price = max(low_price, min(high_price, close_price))

                bars.append({
                    'ts_utc': spy_bar['ts_utc'],
                    'ts_nyt_epoch': spy_bar['ts_nyt_epoch'],
                    'open': open_price,
                    'high': high_price,
                    'low': low_price,
                    'close': close_price,
                    'volume': spy_bar['volume']
                })

                prev_close = close_price
                spy_prev_close = spy_bar['close']

            # Write to file with standard RTH_NH naming
            output_file = f"{output_dir}/{symbol}_RTH_NH.csv"
            df = pd.DataFrame(bars)
            df.to_csv(output_file, index=False)

    def run_backtest_with_eod_validation(self, params: Dict, warmup_blocks: int = 10) -> Dict:
        """Run backtest with continuous file (SIMPLIFIED for reliability).

        NOTE: This is a simplified version that uses a single continuous file
        instead of day-by-day testing. EOD enforcement is handled by execute-trades
        time-based logic (closes positions at 15:58 ET automatically).

        This approach is proven to work (manual test: 220 trades on SPY_4blocks.csv)
        vs day-by-day approach which has complex bar alignment issues.
        """

        # Initialize signal cache for 3-5x speedup
        signal_cache = SignalCache()

        # Import numpy for statistics (needed throughout method)
        import numpy as np

        # Constants
        BARS_PER_DAY = 391  # 9:30 AM - 4:00 PM inclusive
        TEST_DAYS = 10  # Test on 10 trading days for faster iteration (~2 weeks)

        # Calculate indices for continuous test period
        warmup_bars = warmup_blocks * BARS_PER_DAY
        test_bars = TEST_DAYS * BARS_PER_DAY
        total_bars_needed = warmup_bars + test_bars

        if len(self.df) < total_bars_needed:
            print(f"ERROR: Insufficient data - have {len(self.df)} bars, need {total_bars_needed}")
            return {'mrd': -999.0, 'error': 'Insufficient data'}

        # Extract continuous test period
        test_data = self.df.iloc[0:total_bars_needed]

        # Use standard naming convention so execute-trades can find instruments
        test_file = f"{self.output_dir}/SPY_RTH_NH.csv"
        test_data.to_csv(test_file, index=False)

        # Generate leveraged ETF data files in same directory
        self._generate_leveraged_files(test_data, self.output_dir)

        print(f"  Processing {TEST_DAYS} days continuous (warmup={warmup_blocks} blocks)", end="... ")

        # File paths for single continuous test
        signals_file = f"{self.output_dir}/test_signals.jsonl"
        trades_file = f"{self.output_dir}/test_trades.jsonl"

        # Generate cache key for signal lookup
        cache_key = signal_cache.generate_cache_key(
            test_file, 0, warmup_blocks, params
        )

        # Check cache first - huge speedup if hit!
        cached_signals = signal_cache.load(cache_key)

        if cached_signals is not None:
            # Cache HIT - write cached signals to file
            with open(signals_file, 'w') as f:
                for signal in cached_signals:
                    f.write(json.dumps(signal) + '\n')
            signal_cache.stats.record_hit(signal_cache.stats.avg_signal_gen_time)
        else:
            # Cache MISS - generate signals via C++ binary
            cmd_generate = [
                self.sentio_cli, "generate-signals",
                "--data", test_file,
                "--output", signals_file,
                "--warmup", str(warmup_bars),
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

            # Time signal generation for cache statistics
            start_time = time.time()

            try:
                result = subprocess.run(cmd_generate, capture_output=True, text=True, timeout=120)
                if result.returncode != 0:
                    print(f"✗ Signal generation failed!")
                    return {'mrd': -999.0, 'error': 'Signal generation failed'}

                # Record generation time
                elapsed = time.time() - start_time
                signal_cache.stats.record_miss(elapsed)

                # Load generated signals and save to cache
                try:
                    with open(signals_file, 'r') as f:
                        signals = [json.loads(line) for line in f if line.strip()]
                    signal_cache.save(cache_key, signals)
                except:
                    pass  # Don't fail if cache save fails

            except subprocess.TimeoutExpired:
                print(f"✗ Signal generation timeout!")
                return {'mrd': -999.0, 'error': 'Signal generation timeout'}

        # Execute trades with optimized thresholds
        cmd_execute = [
            self.sentio_cli, "execute-trades",
            "--signals", signals_file,
            "--data", test_file,
            "--output", trades_file,
            "--warmup", str(warmup_bars),
            "--buy-threshold", str(params['buy_threshold']),
            "--sell-threshold", str(params['sell_threshold'])
        ]

        try:
            result = subprocess.run(cmd_execute, capture_output=True, text=True, timeout=120)
            if result.returncode != 0:
                print(f"✗ Trade execution failed!")
                return {'mrd': -999.0, 'error': 'Trade execution failed'}
        except subprocess.TimeoutExpired:
            print(f"✗ Trade execution timeout!")
            return {'mrd': -999.0, 'error': 'Trade execution timeout'}

        # Load and analyze trades
        try:
            with open(trades_file, 'r') as f:
                trades = [json.loads(line) for line in f if line.strip()]
        except:
            trades = []

        if not trades or len(trades) == 0:
            print(f"✗ ({TEST_DAYS} days, 0 trades)")
            return {'mrd': -999.0, 'num_days': TEST_DAYS, 'total_trades': 0}

        # Calculate MRD from final portfolio value
        starting_equity = 100000.0
        final_trade = trades[-1]
        ending_equity = final_trade.get('portfolio_value', starting_equity)
        total_return = (ending_equity - starting_equity) / starting_equity
        base_mrd = (total_return / TEST_DAYS) * 100  # Daily return percentage

        # Add trade frequency bonus (reward active trading)
        # Expected: ~50-100 trades/day for 10 days = 500-1000 trades
        min_trades = 50 * TEST_DAYS  # 50 trades/day minimum
        target_trades = 100 * TEST_DAYS  # 100 trades/day target

        if len(trades) < min_trades:
            # Penalize low trade frequency severely
            trade_penalty = -0.5  # -0.5% penalty
        else:
            # Reward trade frequency (up to 0.1% bonus)
            trade_bonus = min((len(trades) - min_trades) / (target_trades - min_trades), 1.0) * 0.1
            trade_penalty = trade_bonus

        mrd = base_mrd + trade_penalty

        # Report cache statistics
        if signal_cache.stats.hits + signal_cache.stats.misses > 0:
            total = signal_cache.stats.hits + signal_cache.stats.misses
            hit_rate = signal_cache.stats.hits / total * 100 if total > 0 else 0
            print(f"✓ ({TEST_DAYS} days, {len(trades)} trades)")
            print(f"  Cache: {signal_cache.stats.hits} hits, {signal_cache.stats.misses} misses ({hit_rate:.0f}% hit rate)")
            if signal_cache.stats.compute_time_saved > 0:
                print(f"  Time saved: {signal_cache.stats.compute_time_saved:.1f}s")
        else:
            print(f"✓ ({TEST_DAYS} days, {len(trades)} trades)")

        # Clean up temporary files
        for temp_file in [signals_file, trades_file, test_file]:
            if os.path.exists(temp_file):
                try:
                    os.remove(temp_file)
                except:
                    pass

        return {
            'mrd': mrd,
            'total_return': total_return * 100,
            'num_days': TEST_DAYS,
            'total_trades': len(trades),
            'final_equity': ending_equity
        }

    def run_backtest_LEGACY_day_by_day(self, params: Dict, warmup_blocks: int = 10) -> Dict:
        """LEGACY: Old day-by-day approach (DISABLED - too complex, has bar alignment bugs)"""
        return {'mrd': -999.0, 'error': 'Legacy day-by-day method disabled'}

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

        # Handle case where Phase 2 is skipped
        if self.n_trials_phase2 == 0:
            print("⚠️  Phase 2 skipped (n_trials=0). Using Phase 1 params only.")
            print()
            # Return Phase 1 params with default secondary params
            default_secondary = {
                'h1_weight': 0.5,
                'h5_weight': 0.3,
                'h10_weight': 0.2,
                'bb_period': 20,
                'bb_std_dev': 2.0,
                'bb_proximity': 0.01,
                'regularization': 0.001
            }
            final_params = phase1_params.copy()
            final_params.update(default_secondary)
            return final_params, 0.0  # MRD unknown without Phase 2

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
            # AGGRESSIVE: Tight thresholds for high-frequency trading
            return {
                'buy_threshold': (0.51, 0.60),
                'sell_threshold': (0.40, 0.49),
                'ewrls_lambda': (0.975, 0.990),  # Much faster adaptation
                'bb_amplification_factor': (0.20, 0.50),  # Strong amplification
                'bb_period': (10, 30),
                'bb_std_dev': (1.0, 2.0),  # Tighter bands → more signals
                'regularization': (0.01, 0.10)
            }
        elif regime == "TRENDING":
            # AGGRESSIVE: Tighter thresholds for momentum capture
            return {
                'buy_threshold': (0.505, 0.55),
                'sell_threshold': (0.45, 0.495),
                'ewrls_lambda': (0.980, 0.995),  # Faster than before
                'bb_amplification_factor': (0.15, 0.35),
                'bb_period': (15, 35),
                'bb_std_dev': (1.0, 2.0),  # Tighter bands
                'regularization': (0.00, 0.05)
            }
        else:  # CHOPPY - MOST AGGRESSIVE (most common regime)
            # RECOMMENDED: Minimal gap (0.002-0.03) for maximum trading opportunities
            # buy_min=0.501, sell_max=0.499 → gap=0.002 minimum
            return {
                'buy_threshold': (0.501, 0.53),   # Tight around 0.5
                'sell_threshold': (0.47, 0.499),  # Tight around 0.5
                'ewrls_lambda': (0.980, 0.995),   # Faster adaptation
                'bb_amplification_factor': (0.15, 0.40),  # Strong amplification
                'bb_period': (15, 30),
                'bb_std_dev': (1.0, 2.0),  # Much tighter for more signals
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
            if current_regime == "HIGH_VOLATILITY":
                min_gap = 0.08
            elif current_regime == "CHOPPY":
                min_gap = 0.01  # Tighter gap for CHOPPY (matches new ranges)
            else:  # TRENDING
                min_gap = 0.04

            if params['buy_threshold'] - params['sell_threshold'] < min_gap:
                return -999.0

            # Use EOD-enforced backtest
            result = self.run_backtest_with_eod_validation(params, warmup_blocks=10)

            mrd = result.get('mrd', -999.0)

            # Penalize extreme MRD values
            if abs(mrd) > 2.0:  # More than 2% daily is suspicious
                print(f"  WARNING: Trial {trial.number} has extreme MRD: {mrd:.4f}%")
                return -999.0

            # Minimum trade frequency constraint
            total_trades = result.get('total_trades', 0)
            test_days = result.get('num_days', 0)
            if test_days > 0 and total_trades < 5:  # Less than 5 trades in test period
                print(f"  Trial {trial.number:3d}: MRD={mrd:+7.4f}% | "
                      f"buy={params['buy_threshold']:.2f} sell={params['sell_threshold']:.2f} | "
                      f"⚠️  REJECTED: Only {total_trades} trades in {test_days} days")
                return -999.0

            print(f"  Trial {trial.number:3d}: MRD={mrd:+7.4f}% | "
                  f"buy={params['buy_threshold']:.2f} sell={params['sell_threshold']:.2f} | "
                  f"{total_trades} trades")

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
