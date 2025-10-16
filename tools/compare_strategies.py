#!/usr/bin/env python3
"""
Strategy Comparison and Analysis Tool

Compares results from Strategy A, B, and C experiments and generates:
- Comparison report (markdown)
- Performance visualizations
- Parameter drift analysis

Author: Claude Code
Date: 2025-10-08
"""

import json
import argparse
from pathlib import Path
from typing import Dict, List
import numpy as np
import pandas as pd


class StrategyComparator:
    """Compare and analyze adaptive strategy results."""

    def __init__(self, results_dir: str):
        self.results_dir = Path(results_dir)
        self.strategies = {}

    def load_strategy(self, strategy_name: str) -> Dict:
        """Load strategy results from JSON file."""
        result_file = self.results_dir / f"strategy_{strategy_name.lower()}_results.json"

        if not result_file.exists():
            print(f"⚠️  Strategy {strategy_name} results not found: {result_file}")
            return None

        with open(result_file, 'r') as f:
            data = json.load(f)

        print(f"✓ Loaded Strategy {strategy_name}: {len(data['results'])} tests")
        return data

    def load_all_strategies(self, strategy_list: List[str]):
        """Load all specified strategies."""
        for strategy_name in strategy_list:
            data = self.load_strategy(strategy_name)
            if data:
                self.strategies[strategy_name] = data

    def compute_statistics(self, strategy_name: str) -> Dict:
        """Compute statistics for a strategy."""
        data = self.strategies[strategy_name]
        mrbs = [r['mrb'] for r in data['results']]

        # Remove outliers (failed runs with -999.0)
        mrbs_clean = [m for m in mrbs if m > -999.0]

        if not mrbs_clean:
            return {
                'count': 0,
                'mean': 0.0,
                'std': 0.0,
                'min': 0.0,
                'max': 0.0,
                'median': 0.0
            }

        return {
            'count': len(mrbs_clean),
            'mean': np.mean(mrbs_clean),
            'std': np.std(mrbs_clean),
            'min': np.min(mrbs_clean),
            'max': np.max(mrbs_clean),
            'median': np.median(mrbs_clean),
            'q25': np.percentile(mrbs_clean, 25),
            'q75': np.percentile(mrbs_clean, 75)
        }

    def analyze_parameter_drift(self, strategy_name: str) -> Dict:
        """Analyze how parameters changed over time."""
        data = self.strategies[strategy_name]
        results = data['results']

        param_names = ['buy_threshold', 'sell_threshold', 'ewrls_lambda',
                       'bb_amplification_factor']

        param_series = {name: [] for name in param_names}

        for result in results:
            params = result['params']
            for name in param_names:
                if name in params:
                    param_series[name].append(params[name])

        # Compute statistics for each parameter
        param_stats = {}
        for name, values in param_series.items():
            if values:
                param_stats[name] = {
                    'mean': np.mean(values),
                    'std': np.std(values),
                    'min': np.min(values),
                    'max': np.max(values),
                    'changes': sum(1 for i in range(1, len(values))
                                  if values[i] != values[i-1])
                }

        return param_stats

    def generate_markdown_report(self, output_file: str):
        """Generate comprehensive comparison report in markdown."""
        lines = []

        lines.append("# Adaptive Optuna Strategy Comparison Report")
        lines.append("")
        lines.append(f"**Generated:** {pd.Timestamp.now().strftime('%Y-%m-%d %H:%M:%S')}")
        lines.append("")
        lines.append("---")
        lines.append("")

        # Executive Summary
        lines.append("## Executive Summary")
        lines.append("")

        summary_table = []
        summary_table.append("| Strategy | Mean MRB | Std MRB | Min MRB | Max MRB | Tests |")
        summary_table.append("|----------|----------|---------|---------|---------|-------|")

        strategy_names = {
            'A': 'Per-Block Adaptive',
            'B': 'Twice-Daily Adaptive',
            'C': 'Static Baseline'
        }

        for strategy in sorted(self.strategies.keys()):
            stats = self.compute_statistics(strategy)
            summary_table.append(
                f"| **{strategy}** ({strategy_names.get(strategy, strategy)}) | "
                f"{stats['mean']:.4f}% | "
                f"{stats['std']:.4f}% | "
                f"{stats['min']:.4f}% | "
                f"{stats['max']:.4f}% | "
                f"{stats['count']} |"
            )

        lines.extend(summary_table)
        lines.append("")

        # Detailed Analysis for Each Strategy
        for strategy in sorted(self.strategies.keys()):
            lines.append(f"## Strategy {strategy}: {strategy_names.get(strategy, strategy)}")
            lines.append("")

            data = self.strategies[strategy]
            stats = self.compute_statistics(strategy)

            # Performance Metrics
            lines.append("### Performance Metrics")
            lines.append("")
            lines.append(f"- **Total Tests:** {stats['count']}")
            lines.append(f"- **Mean MRB:** {stats['mean']:.4f}%")
            lines.append(f"- **Median MRB:** {stats['median']:.4f}%")
            lines.append(f"- **Std MRB:** {stats['std']:.4f}%")
            lines.append(f"- **Min MRB:** {stats['min']:.4f}%")
            lines.append(f"- **Max MRB:** {stats['max']:.4f}%")
            lines.append(f"- **25th Percentile:** {stats['q25']:.4f}%")
            lines.append(f"- **75th Percentile:** {stats['q75']:.4f}%")
            lines.append("")

            # Parameter Analysis
            if strategy != 'C':  # C has fixed params
                lines.append("### Parameter Drift Analysis")
                lines.append("")

                param_stats = self.analyze_parameter_drift(strategy)

                for param_name, param_stat in param_stats.items():
                    lines.append(f"**{param_name}:**")
                    lines.append(f"- Mean: {param_stat['mean']:.4f}")
                    lines.append(f"- Std: {param_stat['std']:.4f}")
                    lines.append(f"- Range: [{param_stat['min']:.4f}, {param_stat['max']:.4f}]")
                    lines.append(f"- Changes: {param_stat['changes']}")
                    lines.append("")

            # Top 5 Best Tests
            lines.append("### Top 5 Best Tests")
            lines.append("")

            results_sorted = sorted(data['results'],
                                   key=lambda x: x['mrb'], reverse=True)[:5]

            lines.append("| Rank | Blocks | MRB | buy_th | sell_th | lambda | bb_amp |")
            lines.append("|------|--------|-----|--------|---------|--------|--------|")

            for rank, result in enumerate(results_sorted, 1):
                params = result['params']
                lines.append(
                    f"| {rank} | "
                    f"{result['block_start']}-{result['block_end']-1} | "
                    f"{result['mrb']:.4f}% | "
                    f"{params.get('buy_threshold', 0):.2f} | "
                    f"{params.get('sell_threshold', 0):.2f} | "
                    f"{params.get('ewrls_lambda', 0):.3f} | "
                    f"{params.get('bb_amplification_factor', 0):.2f} |"
                )

            lines.append("")

            # Bottom 5 Worst Tests
            lines.append("### Bottom 5 Worst Tests")
            lines.append("")

            results_sorted = sorted(data['results'], key=lambda x: x['mrb'])[:5]

            lines.append("| Rank | Blocks | MRB | buy_th | sell_th | lambda | bb_amp |")
            lines.append("|------|--------|-----|--------|---------|--------|--------|")

            for rank, result in enumerate(results_sorted, 1):
                params = result['params']
                lines.append(
                    f"| {rank} | "
                    f"{result['block_start']}-{result['block_end']-1} | "
                    f"{result['mrb']:.4f}% | "
                    f"{params.get('buy_threshold', 0):.2f} | "
                    f"{params.get('sell_threshold', 0):.2f} | "
                    f"{params.get('ewrls_lambda', 0):.3f} | "
                    f"{params.get('bb_amplification_factor', 0):.2f} |"
                )

            lines.append("")
            lines.append("---")
            lines.append("")

        # Comparative Analysis
        if len(self.strategies) > 1:
            lines.append("## Comparative Analysis")
            lines.append("")

            # Compute relative improvements
            if 'C' in self.strategies:
                baseline_mrb = self.compute_statistics('C')['mean']

                lines.append(f"**Baseline (Strategy C) Mean MRB:** {baseline_mrb:.4f}%")
                lines.append("")

                for strategy in ['A', 'B']:
                    if strategy in self.strategies:
                        strat_mrb = self.compute_statistics(strategy)['mean']
                        improvement = strat_mrb - baseline_mrb
                        improvement_pct = (improvement / abs(baseline_mrb)) * 100 if baseline_mrb != 0 else 0

                        lines.append(f"**Strategy {strategy} vs. Baseline:**")
                        lines.append(f"- Absolute improvement: {improvement:+.4f}%")
                        lines.append(f"- Relative improvement: {improvement_pct:+.2f}%")
                        lines.append("")

            # Winner determination
            best_strategy = max(self.strategies.keys(),
                               key=lambda s: self.compute_statistics(s)['mean'])
            best_mrb = self.compute_statistics(best_strategy)['mean']

            lines.append("### 🏆 Winner")
            lines.append("")
            lines.append(f"**Strategy {best_strategy}** ({strategy_names[best_strategy]})")
            lines.append(f"- Mean MRB: {best_mrb:.4f}%")
            lines.append("")

        # Recommendations
        lines.append("## Recommendations")
        lines.append("")

        if 'C' in self.strategies:
            c_stats = self.compute_statistics('C')

        if 'B' in self.strategies:
            b_stats = self.compute_statistics('B')
            lines.append("**For Live Trading:**")
            lines.append(f"- Deploy **Strategy B (Twice-Daily Adaptive)**")
            lines.append(f"  - Tune at 9:30 AM and 12:45 PM")
            lines.append(f"  - Expected MRB: {b_stats['mean']:.4f}% ± {b_stats['std']:.4f}%")

            if 'C' in self.strategies:
                improvement = b_stats['mean'] - c_stats['mean']
                lines.append(f"  - Improvement over static: {improvement:+.4f}%")
            lines.append("")

        if 'A' in self.strategies:
            a_stats = self.compute_statistics('A')
            lines.append("**For Research/High-Frequency:**")
            lines.append(f"- Consider **Strategy A (Per-Block Adaptive)** if:")
            lines.append(f"  - Can handle {a_stats['count']} parameter updates per experiment")
            lines.append(f"  - Willing to accept {a_stats['std']:.4f}% volatility")
            lines.append(f"  - Chasing maximum performance: {a_stats['max']:.4f}% peak MRB")
            lines.append("")

        # Footer
        lines.append("---")
        lines.append("")
        lines.append("**Report generated by:** `compare_strategies.py`")
        lines.append("")
        lines.append("🤖 Generated with [Claude Code](https://claude.com/claude-code)")

        # Write to file
        with open(output_file, 'w') as f:
            f.write('\n'.join(lines))

        print(f"✓ Comparison report saved to: {output_file}")

        return '\n'.join(lines)


def main():
    parser = argparse.ArgumentParser(
        description="Compare adaptive strategy results"
    )
    parser.add_argument('--strategies', required=True,
                        help='Comma-separated list of strategies (A,B,C)')
    parser.add_argument('--results-dir', required=True,
                        help='Path to results directory')
    parser.add_argument('--output', required=True,
                        help='Path to output markdown report')

    args = parser.parse_args()

    # Parse strategies
    strategy_list = [s.strip().upper() for s in args.strategies.split(',')]

    print("="*80)
    print("STRATEGY COMPARISON AND ANALYSIS")
    print("="*80)
    print(f"Strategies: {', '.join(strategy_list)}")
    print(f"Results dir: {args.results_dir}")
    print(f"Output: {args.output}")
    print("="*80)
    print("")

    # Create comparator
    comparator = StrategyComparator(args.results_dir)

    # Load strategies
    print("Loading strategy results...")
    comparator.load_all_strategies(strategy_list)
    print("")

    if not comparator.strategies:
        print("❌ No strategies loaded successfully")
        return 1

    # Generate report
    print("Generating comparison report...")
    report = comparator.generate_markdown_report(args.output)
    print("")

    # Print summary to console
    print("="*80)
    print("SUMMARY")
    print("="*80)
    for strategy in sorted(comparator.strategies.keys()):
        stats = comparator.compute_statistics(strategy)
        print(f"Strategy {strategy}: Mean MRB = {stats['mean']:.4f}% "
              f"(± {stats['std']:.4f}%), {stats['count']} tests")
    print("")
    print(f"Full report: {args.output}")
    print("="*80)

    return 0


if __name__ == '__main__':
    exit(main())
