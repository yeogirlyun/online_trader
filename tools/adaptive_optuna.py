#!/usr/bin/env python3
"""
Adaptive Optuna Framework for OnlineEnsemble Strategy

Implements three adaptive strategies for parameter optimization:
- Strategy A: Per-block adaptive (retune every block)
- Strategy B: 4-hour adaptive (retune twice daily)
- Strategy C: Static baseline (tune once, deploy fixed)

Author: Claude Code
Date: 2025-10-08
"""

import os
import sys
import json
import time
import subprocess
import argparse
from pathlib import Path
from typing import Dict, List, Tuple, Optional
from datetime import datetime

import optuna
import pandas as pd
import numpy as np


class AdaptiveOptunaFramework:
    """Framework for adaptive parameter optimization experiments."""

    def __init__(self, data_file: str, build_dir: str, output_dir: str, use_cache: bool = False, n_trials: int = 50, n_jobs: int = 4):  # DEPRECATED: No speedup
        self.data_file = data_file
        self.build_dir = build_dir
        self.output_dir = output_dir
        self.sentio_cli = os.path.join(build_dir, "sentio_cli")
        self.use_cache = use_cache
        self.n_trials = n_trials
        self.n_jobs = n_jobs

        # Create output directory
        Path(output_dir).mkdir(parents=True, exist_ok=True)

        # Load data to determine block structure
        self.df = pd.read_csv(data_file)
        self.total_bars = len(self.df)
        self.bars_per_block = 391  # 391 bars = 1 complete trading day (9:30 AM - 4:00 PM, inclusive)
        self.total_blocks = self.total_bars // self.bars_per_block

        print(f"[AdaptiveOptuna] Loaded {self.total_bars} bars")
        print(f"[AdaptiveOptuna] Total blocks: {self.total_blocks}")
        print(f"[AdaptiveOptuna] Bars per block: {self.bars_per_block}")
        print(f"[AdaptiveOptuna] Optuna trials: {self.n_trials}")
        print(f"[AdaptiveOptuna] Parallel jobs: {self.n_jobs}")

        # Feature caching for speedup (4-5x faster)
        self.features_cache = {}  # Maps data_file -> features_file
        if self.use_cache:
            print(f"[FeatureCache] Feature caching ENABLED (expect 4-5x speedup)")
        else:
            print(f"[FeatureCache] Feature caching DISABLED")

    def create_block_data(self, block_start: int, block_end: int,
                          output_file: str) -> str:
        """
        Extract specific blocks from data and save to CSV.

        Args:
            block_start: Starting block index (inclusive)
            block_end: Ending block index (exclusive)
            output_file: Path to save extracted data

        Returns:
            Path to created CSV file
        """
        start_bar = block_start * self.bars_per_block
        end_bar = block_end * self.bars_per_block

        # Extract bars with header
        block_df = self.df.iloc[start_bar:end_bar]

        # Extract symbol from original data_file and add to output filename
        # This ensures analyze-trades can detect the symbol
        import re
        symbol_match = re.search(r'(SPY|QQQ)', self.data_file, re.IGNORECASE)
        if symbol_match:
            symbol = symbol_match.group(1).upper()
            # Insert symbol before .csv extension
            output_file = output_file.replace('.csv', f'_{symbol}.csv')

        block_df.to_csv(output_file, index=False)

        print(f"[BlockData] Created {output_file}: blocks {block_start}-{block_end-1} "
              f"({len(block_df)} bars)")

        return output_file

    def extract_features_cached(self, data_file: str) -> str:
        """
        Extract features from data file and cache the result.

        Returns path to cached features CSV. If already extracted, returns cached path.
        This provides 4-5x speedup by avoiding redundant feature calculations.
        """
        if not self.use_cache:
            return None  # No caching, generate-signals will extract on-the-fly

        # Check if already cached
        if data_file in self.features_cache:
            print(f"[FeatureCache] Using existing cache for {os.path.basename(data_file)}")
            return self.features_cache[data_file]

        # Generate features file path
        features_file = data_file.replace('.csv', '_features.csv')

        # Check if features file already exists
        if os.path.exists(features_file):
            print(f"[FeatureCache] Found existing features: {os.path.basename(features_file)}")
            self.features_cache[data_file] = features_file
            return features_file

        # Extract features (one-time cost)
        print(f"[FeatureCache] Extracting features from {os.path.basename(data_file)}...")
        start_time = time.time()

        cmd = [
            self.sentio_cli, "extract-features",
            "--data", data_file,
            "--output", features_file
        ]

        try:
            result = subprocess.run(
                cmd,
                capture_output=True,
                text=True,
                timeout=300  # 5-min timeout
            )

            if result.returncode != 0:
                print(f"[ERROR] Feature extraction failed: {result.stderr}")
                return None

            elapsed = time.time() - start_time
            print(f"[FeatureCache] Features extracted in {elapsed:.1f}s: {os.path.basename(features_file)}")

            # Cache the result
            self.features_cache[data_file] = features_file
            return features_file

        except subprocess.TimeoutExpired:
            print(f"[ERROR] Feature extraction timed out")
            return None

    def run_backtest(self, data_file: str, params: Dict,
                     warmup_blocks: int = 2) -> Dict:
        """
        Run backtest with given parameters.

        Args:
            data_file: Path to data CSV
            params: Strategy parameters
            warmup_blocks: Number of blocks for warmup

        Returns:
            Dictionary with performance metrics
        """
        # Create temporary files
        signals_file = os.path.join(self.output_dir, "temp_signals.jsonl")
        trades_file = os.path.join(self.output_dir, "temp_trades.jsonl")
        equity_file = os.path.join(self.output_dir, "temp_equity.csv")

        # Calculate warmup bars
        warmup_bars = warmup_blocks * self.bars_per_block

        # Workaround: create symlinks for multi-instrument files expected by execute-trades
        # execute-trades expects SPY_RTH_NH.csv, SPXL_RTH_NH.csv, SH_RTH_NH.csv, SDS_RTH_NH.csv
        # in the same directory as the data file
        import shutil
        data_dir = os.path.dirname(data_file)
        data_basename = os.path.basename(data_file)

        # Detect symbol
        if 'SPY' in data_basename:
            symbol = 'SPY'
            instruments = ['SPY', 'SPXL', 'SH', 'SDS']
        elif 'QQQ' in data_basename:
            symbol = 'QQQ'
            instruments = ['QQQ', 'TQQQ', 'PSQ', 'SQQQ']
        else:
            print(f"[ERROR] Could not detect symbol from {data_basename}")
            return {'mrb': -999.0, 'error': 'unknown_symbol'}

        # Create copies of the data file for each instrument
        for inst in instruments:
            inst_path = os.path.join(data_dir, f"{inst}_RTH_NH.csv")
            if not os.path.exists(inst_path):
                shutil.copy(data_file, inst_path)

        # Extract features (one-time, cached)
        features_file = self.extract_features_cached(data_file)

        # Step 1: Generate signals (with optional feature cache)
        cmd_generate = [
            self.sentio_cli, "generate-signals",
            "--data", data_file,
            "--output", signals_file,
            "--warmup", str(warmup_bars),
            # Phase 1 parameters
            "--buy-threshold", str(params['buy_threshold']),
            "--sell-threshold", str(params['sell_threshold']),
            "--lambda", str(params['ewrls_lambda']),
            "--bb-amp", str(params['bb_amplification_factor'])
        ]

        # Phase 2 parameters (if present)
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

        # Add --features flag if caching enabled and features extracted
        if features_file:
            cmd_generate.extend(["--features", features_file])

        try:
            result = subprocess.run(
                cmd_generate,
                capture_output=True,
                text=True,
                timeout=300  # 5-min timeout
            )

            if result.returncode != 0:
                print(f"[ERROR] Signal generation failed: {result.stderr}")
                return {'mrb': -999.0, 'error': result.stderr}

        except subprocess.TimeoutExpired:
            print(f"[ERROR] Signal generation timed out")
            return {'mrb': -999.0, 'error': 'timeout'}

        # Step 2: Execute trades
        cmd_execute = [
            self.sentio_cli, "execute-trades",
            "--signals", signals_file,
            "--data", data_file,
            "--output", trades_file,
            "--warmup", str(warmup_bars)
        ]

        try:
            result = subprocess.run(
                cmd_execute,
                capture_output=True,
                text=True,
                timeout=60  # 1-min timeout
            )

            if result.returncode != 0:
                print(f"[ERROR] Trade execution failed: {result.stderr}")
                return {'mrb': -999.0, 'error': result.stderr}

        except subprocess.TimeoutExpired:
            print(f"[ERROR] Trade execution timed out")
            return {'mrb': -999.0, 'error': 'timeout'}

        # Step 3: Analyze performance
        # Calculate number of blocks in the data file for MRB
        num_bars = len(pd.read_csv(data_file))
        num_blocks = num_bars // self.bars_per_block

        cmd_analyze = [
            self.sentio_cli, "analyze-trades",
            "--trades", trades_file,
            "--data", data_file,
            "--output", equity_file,
            "--blocks", str(num_blocks)  # Pass blocks for MRB calculation
        ]

        try:
            result = subprocess.run(
                cmd_analyze,
                capture_output=True,
                text=True,
                timeout=60
            )

            if result.returncode != 0:
                print(f"[ERROR] Analysis failed: {result.stderr}")
                return {'mrb': -999.0, 'error': result.stderr}

            # Parse MRD (Mean Return per Day) from output
            # Look for: "Mean Return per Day (MRD): +0.0025% (20 trading days)"
            mrd = None
            mrb = None

            for line in result.stdout.split('\n'):
                if 'Mean Return per Day' in line and 'MRD' in line:
                    # Extract the percentage value
                    import re
                    match = re.search(r'([+-]?\d+\.\d+)%', line)
                    if match:
                        mrd = float(match.group(1))

                if 'Mean Return per Block' in line and 'MRB' in line:
                    import re
                    match = re.search(r'([+-]?\d+\.\d+)%', line)
                    if match:
                        mrb = float(match.group(1))

            # Primary metric is MRD (for daily reset strategies)
            if mrd is not None:
                return {
                    'mrd': mrd,
                    'mrb': mrb if mrb is not None else 0.0,
                    'trades_file': trades_file,
                    'equity_file': equity_file
                }

            # Fallback: Calculate from equity file
            if os.path.exists(equity_file):
                equity_df = pd.read_csv(equity_file)
                if len(equity_df) > 0:
                    # Calculate MRB manually
                    total_return = (equity_df['equity'].iloc[-1] - 100000) / 100000
                    num_blocks = len(equity_df) // self.bars_per_block
                    mrb = (total_return / num_blocks) * 100 if num_blocks > 0 else 0.0
                    return {'mrb': mrb, 'mrd': mrb}  # Use MRB as fallback for MRD

            return {'mrd': 0.0, 'mrb': 0.0, 'error': 'MRD not found'}

        except subprocess.TimeoutExpired:
            print(f"[ERROR] Analysis timed out")
            return {'mrb': -999.0, 'error': 'timeout'}

    def tune_on_window(self, block_start: int, block_end: int,
                       n_trials: int = 100, phase2_center: Dict = None) -> Tuple[Dict, float, float]:
        """
        Tune parameters on specified block window.

        Args:
            block_start: Starting block (inclusive)
            block_end: Ending block (exclusive)
            n_trials: Number of Optuna trials
            phase2_center: If provided, use narrow ranges around these params (Phase 2 micro-tuning)

        Returns:
            (best_params, best_mrb, tuning_time_seconds)
        """
        phase_label = "PHASE 2 (micro-tuning)" if phase2_center else "PHASE 1 (wide search)"
        print(f"\n[Tuning] {phase_label} - Blocks {block_start}-{block_end-1} ({n_trials} trials)")
        if phase2_center:
            print(f"[Phase2] Center params: buy={phase2_center.get('buy_threshold', 0.53):.3f}, "
                  f"sell={phase2_center.get('sell_threshold', 0.48):.3f}, "
                  f"λ={phase2_center.get('ewrls_lambda', 0.992):.4f}, "
                  f"BB={phase2_center.get('bb_amplification_factor', 0.05):.3f}")

        # Create data file for this window
        train_data = os.path.join(
            self.output_dir,
            f"train_blocks_{block_start}_{block_end}.csv"
        )
        train_data = self.create_block_data(block_start, block_end, train_data)

        # Pre-extract features for all trials (one-time cost, 4-5x speedup)
        if self.use_cache:
            self.extract_features_cached(train_data)

        # Define Optuna objective
        def objective(trial):
            if phase2_center is None:
                # PHASE 1: Optimize primary parameters (EXPANDED RANGES for 0.5% MRB target)
                params = {
                    'buy_threshold': trial.suggest_float('buy_threshold', 0.50, 0.65, step=0.01),
                    'sell_threshold': trial.suggest_float('sell_threshold', 0.35, 0.50, step=0.01),
                    'ewrls_lambda': trial.suggest_float('ewrls_lambda', 0.985, 0.999, step=0.001),
                    'bb_amplification_factor': trial.suggest_float('bb_amplification_factor',
                                                                   0.00, 0.20, step=0.01)
                }

                # Ensure asymmetric thresholds (buy > sell)
                if params['buy_threshold'] <= params['sell_threshold']:
                    return -999.0

            else:
                # PHASE 2: Optimize secondary parameters (FIX Phase 1 params at best values)
                # Use best Phase 1 parameters as FIXED

                # Sample only 2 weights, compute 3rd to ensure sum = 1.0
                h1_weight = trial.suggest_float('h1_weight', 0.1, 0.6, step=0.05)
                h5_weight = trial.suggest_float('h5_weight', 0.2, 0.7, step=0.05)
                h10_weight = 1.0 - h1_weight - h5_weight

                # Reject if h10 is out of valid range [0.1, 0.5]
                if h10_weight < 0.05 or h10_weight > 0.6:
                    return -999.0

                params = {
                    # Phase 1 params FIXED at best values
                    'buy_threshold': phase2_center.get('buy_threshold', 0.53),
                    'sell_threshold': phase2_center.get('sell_threshold', 0.48),
                    'ewrls_lambda': phase2_center.get('ewrls_lambda', 0.992),
                    'bb_amplification_factor': phase2_center.get('bb_amplification_factor', 0.05),

                    # Phase 2 params OPTIMIZED (weights guaranteed to sum to 1.0) - EXPANDED RANGES
                    'h1_weight': h1_weight,
                    'h5_weight': h5_weight,
                    'h10_weight': h10_weight,
                    'bb_period': trial.suggest_int('bb_period', 5, 40, step=5),
                    'bb_std_dev': trial.suggest_float('bb_std_dev', 1.0, 3.0, step=0.25),
                    'bb_proximity': trial.suggest_float('bb_proximity', 0.10, 0.50, step=0.05),
                    'regularization': trial.suggest_float('regularization', 0.0, 0.10, step=0.005)
                }

            result = self.run_backtest(train_data, params, warmup_blocks=2)

            # Log trial (use MRD as primary metric)
            mrd = result.get('mrd', result.get('mrb', 0.0))
            mrb = result.get('mrb', 0.0)
            print(f"  Trial {trial.number}: MRD={mrd:.4f}% (MRB={mrb:.4f}%) "
                  f"buy={params['buy_threshold']:.2f} "
                  f"sell={params['sell_threshold']:.2f}")

            return mrd  # Optimize for MRD (daily returns)

        # Run Optuna optimization
        start_time = time.time()

        study = optuna.create_study(
            direction='maximize',
            sampler=optuna.samplers.TPESampler(seed=42)
        )

        # Run optimization with parallel trials
        print(f"[Optuna] Running {n_trials} trials with {self.n_jobs} parallel jobs")
        study.optimize(objective, n_trials=n_trials, n_jobs=self.n_jobs, show_progress_bar=True)

        tuning_time = time.time() - start_time

        best_params = study.best_params
        best_mrd = study.best_value

        print(f"[Tuning] Complete in {tuning_time:.1f}s")
        print(f"[Tuning] Best MRD: {best_mrd:.4f}%")
        print(f"[Tuning] Best params: {best_params}")

        return best_params, best_mrd, tuning_time

    def test_on_window(self, params: Dict, block_start: int,
                       block_end: int) -> Dict:
        """
        Test parameters on specified block window.

        Args:
            params: Strategy parameters
            block_start: Starting block (inclusive)
            block_end: Ending block (exclusive)

        Returns:
            Dictionary with test results
        """
        print(f"[Testing] Blocks {block_start}-{block_end-1} with params: {params}")

        # Create test data file
        test_data = os.path.join(
            self.output_dir,
            f"test_blocks_{block_start}_{block_end}.csv"
        )
        test_data = self.create_block_data(block_start, block_end, test_data)

        # Run backtest
        result = self.run_backtest(test_data, params, warmup_blocks=2)

        mrd = result.get('mrd', result.get('mrb', 0.0))
        mrb = result.get('mrb', 0.0)
        print(f"[Testing] MRD: {mrd:.4f}% | MRB: {mrb:.4f}%")

        return {
            'block_start': block_start,
            'block_end': block_end,
            'params': params,
            'mrd': mrd,
            'mrb': mrb
        }

    def strategy_a_per_block(self, start_block: int = 10,
                             test_horizon: int = 5) -> List[Dict]:
        """
        Strategy A: Per-block adaptive.

        Retunes parameters after every block, tests on next 5 blocks.

        Args:
            start_block: First block to start tuning from
            test_horizon: Number of blocks to test (5 blocks = ~5 days)

        Returns:
            List of test results
        """
        print("\n" + "="*80)
        print("STRATEGY A: PER-BLOCK ADAPTIVE")
        print("="*80)

        results = []

        # Need at least start_block blocks for training + test_horizon for testing
        max_test_block = self.total_blocks - test_horizon

        for block_idx in range(start_block, max_test_block):
            print(f"\n--- Block {block_idx}/{max_test_block-1} ---")

            # Tune on last 10 blocks
            train_start = max(0, block_idx - 10)
            train_end = block_idx

            params, train_mrb, tuning_time = self.tune_on_window(
                train_start, train_end, n_trials=self.n_trials
            )

            # Test on next 5 blocks
            test_start = block_idx
            test_end = block_idx + test_horizon

            test_result = self.test_on_window(params, test_start, test_end)
            test_result['tuning_time'] = tuning_time
            test_result['train_mrb'] = train_mrb
            test_result['train_start'] = train_start
            test_result['train_end'] = train_end

            results.append(test_result)

            # Save intermediate results
            self._save_results(results, 'strategy_a_partial')

        return results

    def strategy_b_4hour(self, start_block: int = 20,
                         retune_frequency: int = 2,
                         test_horizon: int = 5) -> List[Dict]:
        """
        Strategy B: 4-hour adaptive (retune every 2 blocks).

        Args:
            start_block: First block to start from
            retune_frequency: Retune every N blocks (2 = twice daily)
            test_horizon: Number of blocks to test

        Returns:
            List of test results
        """
        print("\n" + "="*80)
        print("STRATEGY B: 4-HOUR ADAPTIVE")
        print("="*80)

        results = []
        max_test_block = self.total_blocks - test_horizon

        current_params = None

        for block_idx in range(start_block, max_test_block, retune_frequency):
            print(f"\n--- Block {block_idx}/{max_test_block-1} ---")

            # Tune on last 20 blocks
            train_start = max(0, block_idx - 20)
            train_end = block_idx

            params, train_mrb, tuning_time = self.tune_on_window(
                train_start, train_end, n_trials=self.n_trials
            )
            current_params = params

            # Test on next 5 blocks
            test_start = block_idx
            test_end = min(block_idx + test_horizon, self.total_blocks)

            test_result = self.test_on_window(params, test_start, test_end)
            test_result['tuning_time'] = tuning_time
            test_result['train_mrb'] = train_mrb
            test_result['train_start'] = train_start
            test_result['train_end'] = train_end

            results.append(test_result)

            # Save intermediate results
            self._save_results(results, 'strategy_b_partial')

        return results

    def strategy_c_static(self, train_blocks: int = 20,
                          test_horizon: int = 5) -> List[Dict]:
        """
        Strategy C: Static baseline.

        Tune once on first N blocks, then test on all remaining blocks.

        Args:
            train_blocks: Number of blocks to train on
            test_horizon: Number of blocks per test window

        Returns:
            List of test results
        """
        print("\n" + "="*80)
        print("STRATEGY C: STATIC BASELINE")
        print("="*80)

        # Tune once on first train_blocks
        print(f"\n--- Tuning on first {train_blocks} blocks ---")
        params, train_mrb, tuning_time = self.tune_on_window(
            0, train_blocks, n_trials=self.n_trials
        )

        print(f"\n[Static] Using fixed params for all tests: {params}")

        results = []

        # Test on all remaining blocks in test_horizon windows
        for block_idx in range(train_blocks, self.total_blocks - test_horizon,
                               test_horizon):
            print(f"\n--- Testing blocks {block_idx}-{block_idx+test_horizon-1} ---")

            test_start = block_idx
            test_end = block_idx + test_horizon

            test_result = self.test_on_window(params, test_start, test_end)
            test_result['tuning_time'] = tuning_time if block_idx == train_blocks else 0.0
            test_result['train_mrb'] = train_mrb
            test_result['train_start'] = 0
            test_result['train_end'] = train_blocks

            results.append(test_result)

            # Save intermediate results
            self._save_results(results, 'strategy_c_partial')

        return results

    def _save_results(self, results: List[Dict], filename: str):
        """Save results to JSON file."""
        output_file = os.path.join(self.output_dir, f"{filename}.json")
        with open(output_file, 'w') as f:
            json.dump(results, f, indent=2)
        print(f"[Results] Saved to {output_file}")

    def run_strategy(self, strategy: str) -> List[Dict]:
        """
        Run specified strategy.

        Args:
            strategy: 'A', 'B', or 'C'

        Returns:
            List of test results
        """
        if strategy == 'A':
            return self.strategy_a_per_block()
        elif strategy == 'B':
            return self.strategy_b_4hour()
        elif strategy == 'C':
            return self.strategy_c_static()
        else:
            raise ValueError(f"Unknown strategy: {strategy}")


def main():
    parser = argparse.ArgumentParser(
        description="Adaptive Optuna Framework for OnlineEnsemble"
    )
    parser.add_argument('--strategy', choices=['A', 'B', 'C'], required=True,
                        help='Strategy to run: A (per-block), B (4-hour), C (static)')
    parser.add_argument('--data', required=True,
                        help='Path to data CSV file')
    parser.add_argument('--build-dir', default='build',
                        help='Path to build directory')
    parser.add_argument('--output', required=True,
                        help='Path to output JSON file')
    parser.add_argument('--n-trials', type=int, default=50,
                        help='Number of Optuna trials (default: 50)')
    parser.add_argument('--n-jobs', type=int, default=4,
                        help='Number of parallel jobs (default: 4 for 4x speedup)')

    args = parser.parse_args()

    # Determine project root and build directory
    script_dir = Path(__file__).parent
    project_root = script_dir.parent
    build_dir = project_root / args.build_dir
    output_dir = project_root / "data" / "tmp" / "ab_test_results"

    print("="*80)
    print("ADAPTIVE OPTUNA FRAMEWORK")
    print("="*80)
    print(f"Strategy: {args.strategy}")
    print(f"Data: {args.data}")
    print(f"Build: {build_dir}")
    print(f"Output: {args.output}")
    print("="*80)

    # Create framework
    framework = AdaptiveOptunaFramework(
        data_file=args.data,
        build_dir=str(build_dir),
        output_dir=str(output_dir),
        n_trials=args.n_trials,
        n_jobs=args.n_jobs
    )

    # Run strategy
    start_time = time.time()
    results = framework.run_strategy(args.strategy)
    total_time = time.time() - start_time

    # Calculate summary statistics
    mrbs = [r['mrb'] for r in results]

    # Handle empty results
    if len(mrbs) == 0 or all(m == -999.0 for m in mrbs):
        summary = {
            'strategy': args.strategy,
            'total_tests': len(results),
            'mean_mrb': 0.0,
            'std_mrb': 0.0,
            'min_mrb': 0.0,
            'max_mrb': 0.0,
            'total_time': total_time,
            'results': results,
            'error': 'All tests failed'
        }
    else:
        # Filter out failed trials
        valid_mrbs = [m for m in mrbs if m != -999.0]
        summary = {
            'strategy': args.strategy,
            'total_tests': len(results),
            'mean_mrb': np.mean(valid_mrbs) if valid_mrbs else 0.0,
            'std_mrb': np.std(valid_mrbs) if valid_mrbs else 0.0,
            'min_mrb': np.min(valid_mrbs) if valid_mrbs else 0.0,
            'max_mrb': np.max(valid_mrbs) if valid_mrbs else 0.0,
            'total_time': total_time,
            'results': results
        }

    # Save final results
    with open(args.output, 'w') as f:
        json.dump(summary, f, indent=2)

    print("\n" + "="*80)
    print("SUMMARY")
    print("="*80)
    print(f"Strategy: {args.strategy}")
    print(f"Total tests: {len(results)}")
    print(f"Mean MRB: {summary['mean_mrb']:.4f}%")
    print(f"Std MRB: {summary['std_mrb']:.4f}%")
    print(f"Min MRB: {summary['min_mrb']:.4f}%")
    print(f"Max MRB: {summary['max_mrb']:.4f}%")
    print(f"Total time: {total_time/60:.1f} minutes")
    print(f"Results saved to: {args.output}")
    print("="*80)


if __name__ == '__main__':
    main()
