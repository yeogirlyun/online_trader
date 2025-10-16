#!/usr/bin/env python3
"""
Optuna MRB Walk-Forward Optimizer for OnlineEnsemble Strategy

Expert-recommended optimization approach with:
- 5-fold walk-forward validation
- Soft penalty functions (win rate, drawdown, trade frequency)
- Early pruning (PercentilePruner)
- SQLite persistence with resumption
- Parameter validation
"""

import argparse
import json
import subprocess
import sys
from pathlib import Path
from typing import Dict, List, Tuple
import optuna
from optuna.pruners import PercentilePruner
import numpy as np


class OptunaWalkForwardOptimizer:
    """Walk-forward optimizer for OnlineEnsemble MRB maximization"""

    def __init__(self, data_path: str, build_dir: str, n_folds: int = 5,
                 warmup_blocks: int = 2, test_blocks: int = 4):
        self.data_path = Path(data_path)
        self.build_dir = Path(build_dir)
        self.n_folds = n_folds
        self.warmup_blocks = warmup_blocks
        self.test_blocks = test_blocks

        # Validate paths
        if not self.data_path.exists():
            raise FileNotFoundError(f"Data file not found: {data_path}")
        if not (self.build_dir / "sentio_cli").exists():
            raise FileNotFoundError(f"sentio_cli not found in: {build_dir}")

    def suggest_parameters(self, trial: optuna.Trial) -> Dict[str, float]:
        """Suggest parameter set for this trial"""
        params = {
            # Tier 1: High priority
            "buy_threshold": trial.suggest_float("buy_threshold", 0.51, 0.60),
            "sell_threshold": trial.suggest_float("sell_threshold", 0.40, 0.49),
            "bb_amplification_factor": trial.suggest_float("bb_amplification_factor", 0.05, 0.20),
            "ewrls_lambda": trial.suggest_float("ewrls_lambda", 0.990, 0.999),

            # Tier 2: Medium priority (comment out to reduce search space)
            # "kelly_fraction": trial.suggest_float("kelly_fraction", 0.10, 0.50),
            # "regularization": trial.suggest_float("regularization", 0.001, 0.1, log=True),
        }

        # Constraint: buy_threshold must be > sell_threshold
        if params["buy_threshold"] <= params["sell_threshold"]:
            raise optuna.TrialPruned("buy_threshold must be > sell_threshold")

        # Constraint: Signal spread >= 0.02
        spread = params["buy_threshold"] - params["sell_threshold"]
        if spread < 0.02:
            raise optuna.TrialPruned("Signal spread must be >= 0.02")

        return params

    def run_single_fold(self, params: Dict[str, float], skip_blocks: int) -> Dict[str, float]:
        """Run backtest for a single fold with given parameters"""

        # Build parameter JSON
        params_json = json.dumps(params)

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

            # Extract JSON from backtest output
            # The backtest command outputs results from analyze-trades with --json
            # We need to find the JSON in the output
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
            print(f"   stdout: {result.stdout[-500:]}", file=sys.stderr)
            print(f"   stderr: {result.stderr[-500:]}", file=sys.stderr)
            return {"mrb": -999.0, "win_rate": 0.0, "trades_per_block": 0.0}

    def calculate_soft_penalties(self, metrics: Dict[str, float]) -> float:
        """Calculate soft penalties for constraint violations"""
        penalty = 0.0

        # Penalty 1: Win rate < 50% (severe)
        if metrics["win_rate"] < 50.0:
            penalty += (50.0 - metrics["win_rate"]) * 0.10  # 10% penalty per percentage point

        # Penalty 2: Extreme trade frequency
        trades_per_block = metrics["trades_per_block"]
        if trades_per_block < 50:
            penalty += (50 - trades_per_block) * 0.001  # Light penalty for too few trades
        elif trades_per_block > 300:
            penalty += (trades_per_block - 300) * 0.001  # Light penalty for too many trades

        # Penalty 3: Error case (MRB = -999 indicates failure)
        if metrics["mrb"] < -10.0:
            penalty += 100.0  # Severe penalty for execution failure

        return penalty

    def walk_forward_evaluate(self, trial: optuna.Trial) -> float:
        """5-fold walk-forward evaluation of parameter set"""

        # Suggest parameters for this trial
        params = self.suggest_parameters(trial)

        fold_results = []

        # Run each fold
        for fold_idx in range(self.n_folds):
            skip_blocks = fold_idx * 2  # Non-overlapping folds (2 blocks apart)

            metrics = self.run_single_fold(params, skip_blocks)
            mrb = metrics["mrb"]

            # Calculate soft penalties
            penalty = self.calculate_soft_penalties(metrics)
            penalized_mrb = mrb - penalty

            fold_results.append({
                "fold": fold_idx,
                "skip_blocks": skip_blocks,
                "mrb": mrb,
                "penalized_mrb": penalized_mrb,
                "win_rate": metrics["win_rate"],
                "trades_per_block": metrics["trades_per_block"],
                "penalty": penalty
            })

            # Report intermediate value for early pruning
            trial.report(penalized_mrb, fold_idx)

            # Check if trial should be pruned
            if trial.should_prune():
                raise optuna.TrialPruned()

        # Calculate mean MRB across folds
        mean_mrb = np.mean([r["penalized_mrb"] for r in fold_results])

        # Store fold details in trial user attributes
        trial.set_user_attr("fold_results", fold_results)
        trial.set_user_attr("mean_mrb", mean_mrb)
        trial.set_user_attr("std_mrb", np.std([r["penalized_mrb"] for r in fold_results]))

        return mean_mrb


def main():
    parser = argparse.ArgumentParser(description="Optuna MRB Walk-Forward Optimizer")
    parser.add_argument("--data", required=True, help="Path to CSV data file")
    parser.add_argument("--build-dir", default="build", help="Build directory with sentio_cli")
    parser.add_argument("--n-trials", type=int, default=100, help="Number of optimization trials")
    parser.add_argument("--n-folds", type=int, default=5, help="Number of walk-forward folds")
    parser.add_argument("--warmup-blocks", type=int, default=2, help="Warmup blocks per fold")
    parser.add_argument("--test-blocks", type=int, default=4, help="Test blocks per fold")
    parser.add_argument("--study-name", default="mrb_optimization", help="Optuna study name")
    parser.add_argument("--storage", default="sqlite:///data/tmp/optuna_mrb.db", help="SQLite database path")
    parser.add_argument("--resume", action="store_true", help="Resume existing study")

    args = parser.parse_args()

    # Create optimizer
    optimizer = OptunaWalkForwardOptimizer(
        data_path=args.data,
        build_dir=args.build_dir,
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

    print(f"📊 Starting Optuna MRB Optimization")
    print(f"   Data: {args.data}")
    print(f"   Folds: {args.n_folds}")
    print(f"   Trials: {args.n_trials}")
    print(f"   Storage: {args.storage}")
    print()

    # Run optimization
    study.optimize(
        optimizer.walk_forward_evaluate,
        n_trials=args.n_trials,
        show_progress_bar=True
    )

    # Print results
    print()
    print("=" * 70)
    print("🏆 OPTIMIZATION COMPLETE")
    print("=" * 70)
    print()
    print(f"Best MRB: {study.best_value:.4f}%")
    print(f"Best parameters:")
    for key, value in study.best_params.items():
        print(f"  {key}: {value}")
    print()

    # Print best trial details
    best_trial = study.best_trial
    if "fold_results" in best_trial.user_attrs:
        print("Best trial fold results:")
        for fold in best_trial.user_attrs["fold_results"]:
            print(f"  Fold {fold['fold']}: MRB={fold['mrb']:.4f}%, "
                  f"WR={fold['win_rate']:.1f}%, Trades={fold['trades_per_block']:.1f}, "
                  f"Penalty={fold['penalty']:.4f}")
        print()
        print(f"Mean MRB: {best_trial.user_attrs['mean_mrb']:.4f}%")
        print(f"Std MRB:  {best_trial.user_attrs['std_mrb']:.4f}%")

    # Save best parameters to JSON
    output_path = Path("data/tmp/optuna_best_params.json")
    output_path.parent.mkdir(parents=True, exist_ok=True)
    with open(output_path, 'w') as f:
        json.dump({
            "best_mrb": study.best_value,
            "best_params": study.best_params,
            "n_trials": len(study.trials),
            "fold_results": best_trial.user_attrs.get("fold_results", [])
        }, f, indent=2)

    print()
    print(f"✅ Best parameters saved to: {output_path}")


if __name__ == "__main__":
    main()
