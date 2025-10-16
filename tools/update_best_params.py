#!/usr/bin/env python3
"""
Update best_params.json with Optuna optimization results.

This script reads Optuna results and updates the production parameter file
that live trading uses.

Usage:
    python3 tools/update_best_params.py --optuna-results data/tmp/optuna_results.json
"""

import json
import argparse
from pathlib import Path
from datetime import datetime


def update_best_params(optuna_results_file: str, best_params_file: str = "config/best_params.json"):
    """
    Update best_params.json with results from Optuna optimization.

    Args:
        optuna_results_file: Path to Optuna results JSON
        best_params_file: Path to best parameters file (default: config/best_params.json)
    """
    # Load Optuna results
    with open(optuna_results_file) as f:
        optuna_data = json.load(f)

    # Extract best parameters
    best_params = optuna_data.get('best_params', {})
    best_mrb = optuna_data.get('best_value', 0.0)

    if not best_params:
        print(f"❌ Error: No best_params found in {optuna_results_file}")
        return False

    # Load existing best_params.json (for metadata preservation)
    project_root = Path(__file__).parent.parent
    best_params_path = project_root / best_params_file

    try:
        with open(best_params_path) as f:
            existing_data = json.load(f)
    except FileNotFoundError:
        existing_data = {}

    # Create updated configuration
    updated_config = {
        "last_updated": datetime.now().isoformat(),
        "optimization_source": optuna_data.get('strategy', 'unknown'),
        "optimization_date": datetime.now().strftime("%Y-%m-%d"),
        "data_used": optuna_data.get('data_file', 'unknown'),
        "n_trials": optuna_data.get('total_tests', 0),
        "best_mrb": best_mrb,
        "parameters": {
            "buy_threshold": best_params.get('buy_threshold', 0.55),
            "sell_threshold": best_params.get('sell_threshold', 0.45),
            "ewrls_lambda": best_params.get('ewrls_lambda', 0.995),
            "bb_amplification_factor": best_params.get('bb_amplification_factor', 0.10)
        },
        "previous_best_mrb": existing_data.get('best_mrb', None),
        "note": f"Updated from Optuna optimization on {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}"
    }

    # Save updated configuration
    with open(best_params_path, 'w') as f:
        json.dump(updated_config, f, indent=2)

    # Print summary
    print("="*80)
    print("✅ BEST PARAMETERS UPDATED")
    print("="*80)
    print(f"File: {best_params_path}")
    print(f"Source: {updated_config['optimization_source']}")
    print(f"MRB: {best_mrb:.6f}")
    print("")
    print("Parameters:")
    for key, value in updated_config['parameters'].items():
        print(f"  {key:30s} = {value}")
    print("")

    if updated_config['previous_best_mrb'] is not None:
        improvement = best_mrb - updated_config['previous_best_mrb']
        print(f"Improvement over previous: {improvement:+.6f}")

    print("="*80)
    print("⚠️  IMPORTANT: Restart live trading to use new parameters")
    print("="*80)

    return True


def main():
    parser = argparse.ArgumentParser(
        description="Update best_params.json from Optuna results"
    )
    parser.add_argument('--optuna-results', required=True,
                        help='Path to Optuna results JSON file')
    parser.add_argument('--best-params', default='config/best_params.json',
                        help='Path to best parameters file (default: config/best_params.json)')

    args = parser.parse_args()

    success = update_best_params(args.optuna_results, args.best_params)
    exit(0 if success else 1)


if __name__ == "__main__":
    main()
