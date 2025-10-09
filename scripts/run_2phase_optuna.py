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
        self.df = pd.read_csv(data_file)
        self.total_bars = len(self.df)
        self.bars_per_block = 391  # Complete trading day (9:30 AM - 4:00 PM inclusive)
        self.total_blocks = self.total_bars // self.bars_per_block

        print(f"[2PhaseOptuna] Loaded {self.total_bars} bars ({self.total_blocks} blocks)")
        print(f"[2PhaseOptuna] Phase 1 trials: {self.n_trials_phase1}")
        print(f"[2PhaseOptuna] Phase 2 trials: {self.n_trials_phase2}")
        print(f"[2PhaseOptuna] Parallel jobs: {self.n_jobs}")
        print()

    def run_backtest(self, params: Dict, warmup_blocks: int = 10) -> Dict:
        """Run backtest with given parameters."""
        signals_file = os.path.join(self.output_dir, "temp_signals.jsonl")
        trades_file = os.path.join(self.output_dir, "temp_trades.jsonl")
        equity_file = os.path.join(self.output_dir, "temp_equity.csv")

        warmup_bars = warmup_blocks * self.bars_per_block

        # Step 1: Generate signals
        cmd_generate = [
            self.sentio_cli, "generate-signals",
            "--data", self.data_file,
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

        try:
            result = subprocess.run(
                cmd_generate,
                capture_output=True,
                text=True,
                timeout=300
            )
            if result.returncode != 0:
                return {'mrb': -999.0, 'error': result.stderr}
        except subprocess.TimeoutExpired:
            return {'mrb': -999.0, 'error': 'timeout'}

        # Step 2: Execute trades
        cmd_execute = [
            self.sentio_cli, "execute-trades",
            "--signals", signals_file,
            "--data", self.data_file,
            "--output", trades_file,
            "--warmup", str(warmup_bars)
        ]

        try:
            result = subprocess.run(cmd_execute, capture_output=True, text=True, timeout=60)
            if result.returncode != 0:
                return {'mrb': -999.0, 'error': result.stderr}
        except subprocess.TimeoutExpired:
            return {'mrb': -999.0, 'error': 'timeout'}

        # Step 3: Analyze performance
        num_blocks = self.total_blocks - warmup_blocks
        cmd_analyze = [
            self.sentio_cli, "analyze-trades",
            "--trades", trades_file,
            "--data", self.data_file,
            "--output", equity_file,
            "--blocks", str(num_blocks)
        ]

        try:
            result = subprocess.run(cmd_analyze, capture_output=True, text=True, timeout=60)
            if result.returncode != 0:
                return {'mrb': -999.0, 'error': result.stderr}

            # Parse MRD (Mean Return per Day) from output - primary metric
            import re
            mrd = None
            mrb = None

            for line in result.stdout.split('\n'):
                if 'Mean Return per Day' in line and 'MRD' in line:
                    match = re.search(r'([+-]?\d+\.\d+)%', line)
                    if match:
                        mrd = float(match.group(1))

                if 'Mean Return per Block' in line and 'MRB' in line:
                    match = re.search(r'([+-]?\d+\.\d+)%', line)
                    if match:
                        mrb = float(match.group(1))

            # Primary metric is MRD (for daily reset strategies)
            if mrd is not None:
                return {
                    'mrd': mrd,
                    'mrb': mrb if mrb is not None else 0.0
                }

            # Fallback: calculate from equity file
            if os.path.exists(equity_file):
                equity_df = pd.read_csv(equity_file)
                if len(equity_df) > 0:
                    total_return = (equity_df['equity'].iloc[-1] - 100000) / 100000
                    mrb = (total_return / num_blocks) * 100 if num_blocks > 0 else 0.0
                    return {'mrd': mrb, 'mrb': mrb}  # Use MRB as fallback

            return {'mrd': 0.0, 'mrb': 0.0, 'error': 'MRD not found'}

        except subprocess.TimeoutExpired:
            return {'mrb': -999.0, 'error': 'timeout'}

    def phase1_optimize(self) -> Dict:
        """
        Phase 1: Optimize primary parameters on full dataset.

        Returns best parameters and MRB.
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

            result = self.run_backtest(params, warmup_blocks=10)

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
        best_mrb = study.best_value

        print()
        print(f"✓ Phase 1 Complete in {elapsed:.1f}s ({elapsed/60:.1f} min)")
        print(f"✓ Best MRB: {best_mrb:.4f}%")
        print(f"✓ Best params:")
        for key, value in best_params.items():
            print(f"    {key:25s} = {value}")
        print()

        return best_params, best_mrb

    def phase2_optimize(self, phase1_params: Dict) -> Dict:
        """
        Phase 2: Optimize secondary parameters using Phase 1 best params.

        Returns best parameters and MRB.
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

            result = self.run_backtest(params, warmup_blocks=10)

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

    def save_best_params(self, params: Dict, mrb: float, output_file: str):
        """Save best parameters to JSON file for live trading."""
        output = {
            "last_updated": datetime.now().strftime("%Y-%m-%dT%H:%M:%SZ"),
            "optimization_source": "2phase_optuna_premarket",
            "optimization_date": datetime.now().strftime("%Y-%m-%d"),
            "data_used": os.path.basename(self.data_file),
            "n_trials_phase1": self.n_trials_phase1,
            "n_trials_phase2": self.n_trials_phase2,
            "best_mrb": round(mrb, 4),
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
        phase1_params, phase1_mrb = self.phase1_optimize()

        # Phase 2: Secondary parameters
        final_params, final_mrb = self.phase2_optimize(phase1_params)

        # Save to output file
        self.save_best_params(final_params, final_mrb, output_file)

        total_elapsed = time.time() - total_start

        print("=" * 80)
        print("2-PHASE OPTIMIZATION COMPLETE")
        print("=" * 80)
        print(f"Total time: {total_elapsed/60:.1f} minutes")
        print(f"Phase 1 MRB: {phase1_mrb:.4f}%")
        print(f"Phase 2 MRB: {final_mrb:.4f}%")
        print(f"Improvement: {(final_mrb - phase1_mrb):.4f}%")
        print(f"Parameters saved to: {output_file}")
        print("=" * 80)

        return final_params


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

    # Run optimization
    optimizer = TwoPhaseOptuna(
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
