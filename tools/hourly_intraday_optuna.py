#!/usr/bin/env python3
"""
Hourly Intraday Optuna Optimization

Simplified optimization for hourly parameter tuning during trading day.
Uses 100-bar rolling window evaluation (MRD metric, not MRB).

Author: Claude Code
Date: 2025-10-10
"""

import os
import sys
import json
import subprocess
import argparse
import time
from typing import Dict, Tuple
import optuna

class HourlyIntraDayOptuna:
    def __init__(self, data_file: str, build_dir: str, n_trials: int = 20):
        self.data_file = data_file
        self.build_dir = build_dir
        self.n_trials = n_trials
        self.sentio_cli = os.path.join(build_dir, 'sentio_cli')

        # Baseline parameters
        self.baseline_buy = 0.55
        self.baseline_sell = 0.45
        self.baseline_lambda = 0.995

    def count_bars(self) -> int:
        """Count total bars in data file"""
        with open(self.data_file, 'r') as f:
            return sum(1 for line in f) - 1  # Subtract header

    def evaluate_params(self, buy_threshold: float, sell_threshold: float,
                       ewrls_lambda: float) -> float:
        """
        Evaluate parameters on last 100 bars.

        Returns MRD (Mean Return per Day) as percentage.
        """
        total_bars = self.count_bars()

        if total_bars < 100:
            print(f"⚠️  Insufficient data: {total_bars} bars (need at least 100)")
            return 0.0

        # Use all bars except last 100 as warmup
        warmup_bars = total_bars - 100

        # Generate signals
        signals_file = '/tmp/hourly_opt_signals.jsonl'
        cmd_signals = [
            self.sentio_cli, 'generate-signals',
            '--data', self.data_file,
            '--output', signals_file,
            '--warmup', str(warmup_bars),
            '--buy-threshold', str(buy_threshold),
            '--sell-threshold', str(sell_threshold),
            '--lambda', str(ewrls_lambda)
        ]

        try:
            subprocess.run(cmd_signals, capture_output=True, check=True, timeout=30)
        except (subprocess.CalledProcessError, subprocess.TimeoutExpired) as e:
            print(f"⚠️  Signal generation failed: {e}")
            return 0.0

        # Execute trades
        trades_file = '/tmp/hourly_opt_trades.jsonl'
        cmd_trades = [
            self.sentio_cli, 'execute-trades',
            '--signals', signals_file,
            '--data', self.data_file,
            '--output', trades_file,
            '--warmup', str(warmup_bars)
        ]

        try:
            subprocess.run(cmd_trades, capture_output=True, check=True, timeout=30)
        except (subprocess.CalledProcessError, subprocess.TimeoutExpired) as e:
            print(f"⚠️  Trade execution failed: {e}")
            return 0.0

        # Calculate MRD from final portfolio value
        try:
            with open(trades_file, 'r') as f:
                lines = [line.strip() for line in f if line.strip()]

            if not lines:
                return 0.0

            final_trade = json.loads(lines[-1])
            final_equity = final_trade.get('portfolio_value', 100000.0)
            initial_capital = 100000.0

            # Total return
            total_return = ((final_equity - initial_capital) / initial_capital) * 100

            # MRD = return per day (100 bars ≈ 0.256 days at 390 bars/day)
            num_days = 100 / 390.0
            mrd = total_return / num_days if num_days > 0 else 0.0

            return mrd

        except Exception as e:
            print(f"⚠️  Error calculating MRD: {e}")
            return 0.0

    def objective(self, trial: optuna.Trial) -> float:
        """Optuna objective function"""
        # Sample parameters
        buy_threshold = trial.suggest_float('buy_threshold', 0.50, 0.70, step=0.01)
        sell_threshold = trial.suggest_float('sell_threshold', 0.30, 0.50, step=0.01)
        ewrls_lambda = trial.suggest_float('ewrls_lambda', 0.990, 0.999, step=0.001)

        # Evaluate
        mrd = self.evaluate_params(buy_threshold, sell_threshold, ewrls_lambda)

        return mrd  # Optuna maximizes by default

    def optimize(self) -> Dict:
        """Run Optuna optimization"""
        print(f"[HourlyOptuna] Starting optimization...")
        print(f"  Data: {self.data_file}")
        print(f"  Total bars: {self.count_bars()}")
        print(f"  Test window: 100 bars")
        print(f"  Trials: {self.n_trials}")
        print("")

        # Evaluate baseline first
        print("Evaluating baseline parameters...")
        baseline_mrd = self.evaluate_params(
            self.baseline_buy,
            self.baseline_sell,
            self.baseline_lambda
        )
        print(f"  Baseline MRD: {baseline_mrd:+.4f}%")
        print("")

        # Run Optuna
        print(f"Running {self.n_trials} Optuna trials...")
        start_time = time.time()

        study = optuna.create_study(direction='maximize', sampler=optuna.samplers.TPESampler())
        study.optimize(self.objective, n_trials=self.n_trials, show_progress_bar=False)

        elapsed = time.time() - start_time

        # Get best parameters
        best_params = study.best_params
        best_mrd = study.best_value

        print("")
        print(f"✓ Optimization complete ({elapsed:.1f}s)")
        print(f"  Best MRD: {best_mrd:+.4f}%")
        print(f"  Best params: buy={best_params['buy_threshold']:.2f}, sell={best_params['sell_threshold']:.2f}, lambda={best_params['ewrls_lambda']:.3f}")
        print("")

        # Compare with baseline
        if best_mrd > baseline_mrd:
            print(f"🎯 Optuna is BETTER (+{best_mrd - baseline_mrd:.4f}% improvement)")
            selected_source = "optuna"
            selected_params = best_params
            selected_mrd = best_mrd
        else:
            print(f"📊 Baseline is BETTER or EQUAL ({baseline_mrd:+.4f}% >= {best_mrd:+.4f}%)")
            selected_source = "baseline"
            selected_params = {
                'buy_threshold': self.baseline_buy,
                'sell_threshold': self.baseline_sell,
                'ewrls_lambda': self.baseline_lambda
            }
            selected_mrd = baseline_mrd

        return {
            'source': selected_source,
            'buy_threshold': selected_params['buy_threshold'],
            'sell_threshold': selected_params['sell_threshold'],
            'ewrls_lambda': selected_params['ewrls_lambda'],
            'expected_mrd': selected_mrd,
            'baseline_mrd': baseline_mrd,
            'best_mrd': best_mrd,
            'n_trials': self.n_trials,
            'elapsed_seconds': elapsed
        }

def main():
    parser = argparse.ArgumentParser(description="Hourly Intraday Optuna Optimization")
    parser.add_argument('--data', required=True, help='Path to comprehensive warmup CSV')
    parser.add_argument('--build-dir', default='build', help='Path to build directory')
    parser.add_argument('--output', required=True, help='Path to output JSON file')
    parser.add_argument('--n-trials', type=int, default=20, help='Number of Optuna trials')

    args = parser.parse_args()

    optimizer = HourlyIntraDayOptuna(
        data_file=args.data,
        build_dir=args.build_dir,
        n_trials=args.n_trials
    )

    result = optimizer.optimize()

    # Save result
    with open(args.output, 'w') as f:
        json.dump(result, f, indent=2)

    print(f"✓ Results saved to: {args.output}")

if __name__ == '__main__':
    main()
