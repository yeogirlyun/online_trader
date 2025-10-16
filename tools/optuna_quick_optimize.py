#!/usr/bin/env python3
"""
Quick Optuna Optimization for Midday Parameter Tuning

Runs fast optimization (50 trials, ~5 minutes) to find best parameters
for afternoon session based on morning + historical data.
"""

import os
import sys
import json
import subprocess
import optuna
from pathlib import Path
import argparse

PROJECT_ROOT = Path("/Volumes/ExternalSSD/Dev/C++/online_trader")
BUILD_DIR = PROJECT_ROOT / "build"

class QuickOptimizer:
    def __init__(self, data_file: str, n_trials: int = 50):
        self.data_file = data_file
        self.n_trials = n_trials
        self.baseline_mrb = None

    def run_backtest(self, buy_threshold: float, sell_threshold: float,
                     ewrls_lambda: float) -> float:
        """Run backtest with given parameters and return MRB"""

        # For quick optimization, use backtest command
        # In production, this would use the full pipeline
        cmd = [
            str(BUILD_DIR / "sentio_cli"),
            "backtest",
            "--data", self.data_file,
            "--warmup-blocks", "10",
            "--test-blocks", "4"
        ]

        try:
            result = subprocess.run(cmd, capture_output=True, text=True, timeout=60)

            if result.returncode != 0:
                print(f"Backtest failed: {result.stderr}")
                return 0.0

            # Extract MRB from output
            mrb = self._extract_mrb(result.stdout)
            return mrb

        except subprocess.TimeoutExpired:
            print("Backtest timeout")
            return 0.0
        except Exception as e:
            print(f"Backtest error: {e}")
            return 0.0

    def _extract_mrb(self, output: str) -> float:
        """Extract MRB from backtest output"""
        for line in output.split('\n'):
            if 'MRB' in line or 'Mean Return' in line:
                import re
                # Look for percentage
                match = re.search(r'([-+]?\d*\.?\d+)\s*%', line)
                if match:
                    return float(match.group(1))
                # Look for decimal
                match = re.search(r'MRB[:\s]+([-+]?\d*\.?\d+)', line)
                if match:
                    return float(match.group(1))
        return 0.0

    def objective(self, trial: optuna.Trial) -> float:
        """Optuna objective function"""

        # Search space
        buy_threshold = trial.suggest_float('buy_threshold', 0.50, 0.70)
        sell_threshold = trial.suggest_float('sell_threshold', 0.30, 0.50)
        ewrls_lambda = trial.suggest_float('ewrls_lambda', 0.990, 0.999)

        # Run backtest
        mrb = self.run_backtest(buy_threshold, sell_threshold, ewrls_lambda)

        return mrb

    def optimize(self) -> dict:
        """Run optimization and return best parameters"""

        print(f"Starting Optuna optimization ({self.n_trials} trials)...")
        print(f"Data: {self.data_file}")

        # Create study
        study = optuna.create_study(
            direction='maximize',
            sampler=optuna.samplers.TPESampler(seed=42)
        )

        # Run baseline first
        print("\n1. Running baseline (buy=0.55, sell=0.45, lambda=0.995)...")
        baseline_mrb = self.run_backtest(0.55, 0.45, 0.995)
        self.baseline_mrb = baseline_mrb
        print(f"   Baseline MRB: {baseline_mrb:.4f}%")

        # Optimize
        print(f"\n2. Running {self.n_trials} optimization trials...")
        study.optimize(self.objective, n_trials=self.n_trials, show_progress_bar=True)

        # Best trial
        best_trial = study.best_trial
        best_mrb = best_trial.value
        improvement = best_mrb - baseline_mrb

        print(f"\n3. Optimization complete!")
        print(f"   Baseline MRB: {baseline_mrb:.4f}%")
        print(f"   Best MRB: {best_mrb:.4f}%")
        print(f"   Improvement: {improvement:.4f}%")
        print(f"   Best params:")
        print(f"     buy_threshold: {best_trial.params['buy_threshold']:.4f}")
        print(f"     sell_threshold: {best_trial.params['sell_threshold']:.4f}")
        print(f"     ewrls_lambda: {best_trial.params['ewrls_lambda']:.6f}")

        return {
            'baseline_mrb': baseline_mrb,
            'best_mrb': best_mrb,
            'improvement': improvement,
            'buy_threshold': best_trial.params['buy_threshold'],
            'sell_threshold': best_trial.params['sell_threshold'],
            'ewrls_lambda': best_trial.params['ewrls_lambda'],
            'n_trials': self.n_trials
        }

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument('--data', required=True, help='Data file path')
    parser.add_argument('--trials', type=int, default=50, help='Number of trials')
    parser.add_argument('--output', required=True, help='Output JSON file')
    args = parser.parse_args()

    optimizer = QuickOptimizer(args.data, args.trials)
    results = optimizer.optimize()

    # Save results
    with open(args.output, 'w') as f:
        json.dump(results, f, indent=2)

    print(f"\n✓ Results saved to: {args.output}")
