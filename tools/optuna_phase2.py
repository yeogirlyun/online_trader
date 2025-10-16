#!/usr/bin/env python3
"""
Optuna Phase 2 Optimizer - OnlineEnsemble Multi-Horizon and Bollinger Band Parameters

Phase 2 optimizes:
- h1_weight, h5_weight, h10_weight (horizon weights, must sum to 1.0)
- bb_period, bb_std_dev, bb_proximity (Bollinger Band parameters)
- regularization (L2 regularization)

Phase 1 parameters are FIXED to best values from phase 1 optimization.
"""

import argparse
import json
import subprocess
import sys
from pathlib import Path
from typing import Dict, List
import optuna
from optuna.pruners import PercentilePruner
import numpy as np


class OptunaPhase2Optimizer:
    """Phase 2 optimizer for multi-horizon and BB parameters"""

    def __init__(self, data_path: str, build_dir: str,
                 phase1_params: Dict[str, float],
                 n_folds: int = 5,
                 warmup_blocks: int = 2, test_blocks: int = 4):
        self.data_path = Path(data_path)
        self.build_dir = Path(build_dir)
        self.phase1_params = phase1_params  # Fixed phase 1 parameters
        self.n_folds = n_folds
        self.warmup_blocks = warmup_blocks
        self.test_blocks = test_blocks

        # Validate paths
        if not self.data_path.exists():
            raise FileNotFoundError(f"Data file not found: {data_path}")
        if not (self.build_dir / "sentio_cli").exists():
            raise FileNotFoundError(f"sentio_cli not found in: {build_dir}")

    def suggest_parameters(self, trial: optuna.Trial) -> Dict[str, float]:
        """Suggest phase 2 parameters with constraints"""

        # Horizon weights (must sum to 1.0)
        # Use simplex sampling: sample 2 weights, calculate 3rd
        w1 = trial.suggest_float("h1_weight", 0.1, 0.6)
        w2_max = min(0.7, 1.0 - w1 - 0.1)  # Ensure w3 >= 0.1
        w2 = trial.suggest_float("h5_weight", 0.1, w2_max)
        w3 = 1.0 - w1 - w2

        # Validate constraint
        if w3 < 0.1 or w3 > 0.7:
            raise optuna.TrialPruned("Invalid horizon weight distribution")

        # Bollinger Band parameters
        bb_period = trial.suggest_int("bb_period", 10, 30)
        bb_std_dev = trial.suggest_float("bb_std_dev", 1.5, 3.0)
        bb_proximity = trial.suggest_float("bb_proximity", 0.1, 0.5)

        # Regularization
        regularization = trial.suggest_float("regularization", 0.001, 0.1, log=True)

        return {
            "h1_weight": w1,
            "h5_weight": w2,
            "h10_weight": w3,
            "bb_period": bb_period,
            "bb_std_dev": bb_std_dev,
            "bb_proximity": bb_proximity,
            "regularization": regularization
        }

    def run_single_fold(self, phase2_params: Dict[str, float], skip_blocks: int) -> Dict[str, float]:
        """Run backtest with fixed phase1 + phase2 params"""

        # Combine phase 1 (fixed) and phase 2 (optimizing) parameters
        all_params = {**self.phase1_params, **phase2_params}
        params_json = json.dumps(all_params)

        # Run backtest command
        cmd = [
            str(self.build_dir / "sentio_cli"),
            "backtest",
            "--data", str(self.data_path),
            "--blocks", str(self.test_blocks),
            "--warmup-blocks", str(self.warmup_blocks),
            "--skip-blocks", str(skip_blocks),
            "--params", params_json
        ]

        try:
            result = subprocess.run(
                cmd,
                capture_output=True,
                text=True,
                timeout=120,
                check=False
            )

            if result.returncode != 0:
                print(f"❌ Backtest failed (skip={skip_blocks}): {result.stderr[:200]}", file=sys.stderr)
                return {"mrb": -999.0, "win_rate": 0.0, "trades_per_block": 0.0}

            # Extract JSON from output
            lines = result.stdout.strip().split('\n')
            json_line = None
            for line in reversed(lines):
                if line.strip().startswith('{'):
                    json_line = line.strip()
                    break

            if not json_line:
                print(f"❌ No JSON output found (skip={skip_blocks})", file=sys.stderr)
                print(f"   Last 5 lines: {lines[-5:]}", file=sys.stderr)
                return {"mrb": -999.0, "win_rate": 0.0, "trades_per_block": 0.0}

            try:
                metrics = json.loads(json_line)
                return metrics
            except json.JSONDecodeError as e:
                print(f"❌ JSON parse error (skip={skip_blocks}): {e}", file=sys.stderr)
                print(f"   Attempted to parse: {repr(json_line[:200])}", file=sys.stderr)
                return {"mrb": -999.0, "win_rate": 0.0, "trades_per_block": 0.0}

        except subprocess.TimeoutExpired:
            print(f"❌ Backtest timeout (skip={skip_blocks})", file=sys.stderr)
            return {"mrb": -999.0, "win_rate": 0.0, "trades_per_block": 0.0}
        except Exception as e:
            print(f"❌ Error running backtest (skip={skip_blocks}): {e}", file=sys.stderr)
            return {"mrb": -999.0, "win_rate": 0.0, "trades_per_block": 0.0}

    def calculate_soft_penalties(self, metrics: Dict[str, float]) -> float:
        """Calculate soft penalties for constraint violations"""
        penalty = 0.0

        # Penalty for low win rate (<45%)
        if metrics["win_rate"] < 45.0:
            penalty += (45.0 - metrics["win_rate"]) * 0.01

        # Penalty for very high trade frequency (>150 trades/block)
        if metrics["trades_per_block"] > 150.0:
            penalty += (metrics["trades_per_block"] - 150.0) * 0.001

        # Penalty for very low trade frequency (<50 trades/block)
        if metrics["trades_per_block"] < 50.0:
            penalty += (50.0 - metrics["trades_per_block"]) * 0.002

        return penalty

    def objective(self, trial: optuna.Trial) -> float:
        """Optuna objective function with walk-forward validation"""

        # Suggest phase 2 parameters
        phase2_params = self.suggest_parameters(trial)

        # Walk-forward validation across folds
        fold_results = []
        for fold_idx in range(self.n_folds):
            skip_blocks = fold_idx * 2  # Skip 0, 2, 4, 6, 8 blocks

            metrics = self.run_single_fold(phase2_params, skip_blocks)

            # Apply soft penalties
            penalty = self.calculate_soft_penalties(metrics)
            penalized_mrb = metrics["mrb"] - penalty

            fold_results.append({
                "fold": fold_idx,
                "skip_blocks": skip_blocks,
                "mrb": metrics["mrb"],
                "penalized_mrb": penalized_mrb,
                "win_rate": metrics["win_rate"],
                "trades_per_block": metrics["trades_per_block"],
                "penalty": penalty
            })

            # Report intermediate result for pruning
            trial.report(penalized_mrb, fold_idx)

            # Check if trial should be pruned
            if trial.should_prune():
                raise optuna.TrialPruned()

        # Return mean penalized MRB across folds
        mean_mrb = np.mean([f["penalized_mrb"] for f in fold_results])

        # Store fold results in trial user attributes
        trial.set_user_attr("fold_results", fold_results)
        trial.set_user_attr("mean_mrb", mean_mrb)
        trial.set_user_attr("std_mrb", np.std([f["penalized_mrb"] for f in fold_results]))

        return mean_mrb


def main():
    parser = argparse.ArgumentParser(description="Optuna Phase 2 Optimizer")
    parser.add_argument("--data", required=True, help="Path to data file")
    parser.add_argument("--build-dir", default="build", help="Build directory")
    parser.add_argument("--phase1-params", required=True, help="JSON file with best phase 1 params")
    parser.add_argument("--n-trials", type=int, default=50, help="Number of trials")
    parser.add_argument("--n-folds", type=int, default=5, help="Number of walk-forward folds")
    parser.add_argument("--warmup-blocks", type=int, default=2, help="Warmup blocks")
    parser.add_argument("--test-blocks", type=int, default=10, help="Test blocks per fold")
    parser.add_argument("--study-name", default="phase2_opt", help="Study name")
    parser.add_argument("--storage", default="sqlite:///optuna_phase2.db", help="Optuna storage")
    parser.add_argument("--resume", action="store_true", help="Resume existing study")
    args = parser.parse_args()

    # Load phase 1 parameters
    with open(args.phase1_params, 'r') as f:
        phase1_data = json.load(f)
        phase1_params = phase1_data["best_params"]

    print("📊 Starting Optuna Phase 2 Optimization")
    print(f"   Data: {args.data}")
    print(f"   Phase 1 Params: {phase1_params}")
    print(f"   Folds: {args.n_folds}")
    print(f"   Trials: {args.n_trials}")
    print(f"   Storage: {args.storage}")
    print()

    # Create optimizer
    optimizer = OptunaPhase2Optimizer(
        data_path=args.data,
        build_dir=args.build_dir,
        phase1_params=phase1_params,
        n_folds=args.n_folds,
        warmup_blocks=args.warmup_blocks,
        test_blocks=args.test_blocks
    )

    # Create or load study
    study = optuna.create_study(
        study_name=args.study_name,
        storage=args.storage,
        load_if_exists=args.resume,
        direction="maximize",
        pruner=PercentilePruner(percentile=50.0, n_min_trials=10)
    )

    # Run optimization
    study.optimize(optimizer.objective, n_trials=args.n_trials, show_progress_bar=True)

    # Print results
    print("\n" + "="*70)
    print("🏆 OPTIMIZATION COMPLETE")
    print("="*70)
    print()
    print(f"Best MRB: {study.best_value:.4f}%")
    print("Best parameters:")
    for key, value in study.best_params.items():
        print(f"  {key}: {value}")
    print()

    # Get best trial details
    best_trial = study.best_trial
    fold_results = best_trial.user_attrs["fold_results"]
    print("Best trial fold results:")
    for result in fold_results:
        print(f"  Fold {result['fold']}: MRB={result['mrb']:.4f}%, "
              f"WR={result['win_rate']:.1f}%, "
              f"Trades={result['trades_per_block']:.1f}, "
              f"Penalty={result['penalty']:.4f}")

    print()
    print(f"Mean MRB: {best_trial.user_attrs['mean_mrb']:.4f}%")
    print(f"Std MRB:  {best_trial.user_attrs['std_mrb']:.4f}%")
    print()

    # Save best parameters
    output_file = "data/tmp/optuna_phase2_best_params.json"
    with open(output_file, 'w') as f:
        json.dump({
            "best_mrb": study.best_value,
            "best_params": study.best_params,
            "phase1_params": phase1_params,
            "combined_params": {**phase1_params, **study.best_params},
            "n_trials": args.n_trials,
            "fold_results": fold_results
        }, f, indent=2)

    print(f"✅ Best parameters saved to: {output_file}")


if __name__ == "__main__":
    main()
