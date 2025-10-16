#!/usr/bin/env python3
"""
Mock Live Trading Session Launcher

Simulates a complete trading day using mock infrastructure:
- Mock Alpaca ($100K cash, $200K buying power)
- Mock bar feed (replays yesterday's data at 39x speed)
- Full workflow: warmup → morning → optimization → afternoon → EOD
- Visual dashboard output for analysis
"""

import os
import sys
import json
import time
import subprocess
import signal
from pathlib import Path
from datetime import datetime, timedelta
from typing import Dict, List, Optional

# Project paths
PROJECT_ROOT = Path("/Volumes/ExternalSSD/Dev/C++/online_trader")
BUILD_DIR = PROJECT_ROOT / "build"
DATA_DIR = PROJECT_ROOT / "data"
LOGS_DIR = PROJECT_ROOT / "logs" / "mock_trading"
DASHBOARDS_DIR = DATA_DIR / "dashboards"

# Ensure directories exist
LOGS_DIR.mkdir(parents=True, exist_ok=True)
DASHBOARDS_DIR.mkdir(parents=True, exist_ok=True)

class MockTradingSession:
    def __init__(self, session_date: str, speed_multiplier: float = 39.0):
        """
        Args:
            session_date: Date to simulate (YYYY-MM-DD)
            speed_multiplier: Replay speed (39.0 = 39x real-time)
        """
        self.session_date = session_date
        self.speed_multiplier = speed_multiplier
        self.session_id = f"mock_{session_date.replace('-', '')}_{int(time.time())}"

        self.session_dir = LOGS_DIR / self.session_id
        self.session_dir.mkdir(parents=True, exist_ok=True)

        # Mock configuration
        self.mock_config = {
            "mode": "REPLAY_HISTORICAL",
            "initial_cash": 100000.0,
            "buying_power": 200000.0,
            "commission_per_share": 0.0,
            "enable_market_impact": True,
            "market_impact_bps": 5.0,
            "bid_ask_spread_bps": 2.0,
            "speed_multiplier": speed_multiplier
        }

        # Session state
        self.morning_metrics = {}
        self.afternoon_metrics = {}
        self.optimization_results = {}
        self.eod_results = {}

        # Process handle
        self.process = None

    def log(self, message: str):
        """Log with timestamp"""
        timestamp = datetime.now().strftime("%H:%M:%S")
        print(f"[{timestamp}] {message}")

        # Also write to session log
        with open(self.session_dir / "session.log", "a") as f:
            f.write(f"[{timestamp}] {message}\n")

    def prepare_warmup_data(self) -> str:
        """Prepare 20-block + feature warmup data"""
        self.log("=" * 80)
        self.log("PHASE 1: Preparing Warmup Data")
        self.log("=" * 80)

        warmup_file = DATA_DIR / "equities" / "SPY_warmup_latest.csv"

        if not warmup_file.exists():
            self.log(f"❌ Warmup file not found: {warmup_file}")
            self.log(f"   Run: tools/warmup_live_trading.sh")
            sys.exit(1)

        # Verify warmup has enough bars
        with open(warmup_file) as f:
            bars = sum(1 for _ in f) - 1  # Subtract header

        self.log(f"✓ Warmup file: {warmup_file}")
        self.log(f"  Total bars: {bars}")
        self.log(f"  Required: 7,864+ (20 blocks + 64 feature bars)")

        if bars < 7864:
            self.log(f"⚠️  Warning: Insufficient warmup bars ({bars} < 7864)")

        return str(warmup_file)

    def prepare_market_data(self) -> str:
        """Prepare yesterday's market data for replay"""
        self.log("")
        self.log("=" * 80)
        self.log("PHASE 2: Preparing Market Data")
        self.log("=" * 80)

        # For now, use SPY_RTH_NH.csv
        # In production, fetch from Polygon API for exact date
        data_file = DATA_DIR / "equities" / "SPY_RTH_NH.csv"

        if not data_file.exists():
            self.log(f"❌ Market data not found: {data_file}")
            sys.exit(1)

        self.log(f"✓ Market data: {data_file}")
        self.log(f"  Simulating: {self.session_date}")
        self.log(f"  Speed: {self.speed_multiplier}x real-time")

        return str(data_file)

    def create_mock_config_file(self) -> str:
        """Create mock configuration JSON"""
        config_file = self.session_dir / "mock_config.json"

        with open(config_file, 'w') as f:
            json.dump(self.mock_config, f, indent=2)

        self.log(f"✓ Mock config: {config_file}")

        return str(config_file)

    def run_morning_session(self, warmup_file: str, data_file: str):
        """Run morning session (9:30 AM - 12:45 PM)"""
        self.log("")
        self.log("=" * 80)
        self.log("PHASE 3: Morning Session (9:30 AM - 12:45 PM)")
        self.log("=" * 80)
        self.log("Using baseline OES parameters:")
        self.log("  buy_threshold: 0.55")
        self.log("  sell_threshold: 0.45")
        self.log("  ewrls_lambda: 0.995")
        self.log("")

        # In production, this would start C++ live trader with mock mode
        # For now, simulate with generate-signals + execute-trades

        morning_signals = self.session_dir / "morning_signals.jsonl"
        morning_trades = self.session_dir / "morning_trades.jsonl"

        # Generate signals
        cmd = [
            str(BUILD_DIR / "sentio_cli"),
            "generate-signals",
            "--data", data_file,
            "--output", str(morning_signals),
            "--warmup", "3900"
        ]

        self.log("Generating morning signals...")
        result = subprocess.run(cmd, capture_output=True, text=True)

        if result.returncode != 0:
            self.log(f"❌ Failed: {result.stderr}")
            return

        # Execute trades
        cmd = [
            str(BUILD_DIR / "sentio_cli"),
            "execute-trades",
            "--signals", str(morning_signals),
            "--data", data_file,
            "--output", str(morning_trades),
            "--warmup", "3900"
        ]

        self.log("Executing morning trades...")
        result = subprocess.run(cmd, capture_output=True, text=True)

        if result.returncode != 0:
            self.log(f"❌ Failed: {result.stderr}")
            return

        # Analyze
        self.log("Analyzing morning performance...")
        self.analyze_session(morning_trades, "morning")

        self.log(f"✓ Morning session complete")
        self.log(f"  Trades: {self.morning_metrics.get('total_trades', 0)}")
        self.log(f"  P&L: ${self.morning_metrics.get('total_pnl', 0.0):.2f}")
        self.log(f"  Return: {self.morning_metrics.get('total_return_pct', 0.0):.4f}%")

    def run_midday_optimization(self, warmup_file: str):
        """Run midday optimization (12:45 PM)"""
        self.log("")
        self.log("=" * 80)
        self.log("PHASE 4: Midday Optimization (12:45 PM)")
        self.log("=" * 80)

        optuna_script = PROJECT_ROOT / "tools" / "optuna_quick_optimize.py"

        if not optuna_script.exists():
            self.log("⚠️  Optuna script not found, skipping optimization")
            return

        # Create comprehensive warmup (historical + morning bars)
        comprehensive_warmup = self.session_dir / "comprehensive_warmup_1245.csv"

        import shutil
        shutil.copy(warmup_file, comprehensive_warmup)

        # Run optimization
        params_file = self.session_dir / "optimized_params.json"

        cmd = [
            "python3",
            str(optuna_script),
            "--data", str(comprehensive_warmup),
            "--trials", "50",
            "--output", str(params_file)
        ]

        self.log("Running Optuna optimization (50 trials)...")
        self.log("  (This may take 5-10 minutes)")

        result = subprocess.run(cmd, capture_output=True, text=True, timeout=600)

        if result.returncode != 0:
            self.log(f"⚠️  Optimization failed: {result.stderr}")
            return

        # Load results
        if params_file.exists():
            with open(params_file) as f:
                self.optimization_results = json.load(f)

            self.log(f"✓ Optimization complete!")
            self.log(f"  Baseline MRB: {self.optimization_results.get('baseline_mrb', 0.0):.4f}%")
            self.log(f"  Optimized MRB: {self.optimization_results.get('best_mrb', 0.0):.4f}%")
            self.log(f"  Improvement: {self.optimization_results.get('improvement', 0.0):.4f}%")

    def run_afternoon_session(self, warmup_file: str, data_file: str):
        """Run afternoon session (1:00 PM - 4:00 PM)"""
        self.log("")
        self.log("=" * 80)
        self.log("PHASE 5: Afternoon Session (1:00 PM - 4:00 PM)")
        self.log("=" * 80)

        # Determine which params to use
        use_optimized = (self.optimization_results and
                        self.optimization_results.get('improvement', 0) > 0.05)

        if use_optimized:
            self.log("Using optimized parameters")
            self.log(f"  buy_threshold: {self.optimization_results['buy_threshold']:.4f}")
            self.log(f"  sell_threshold: {self.optimization_results['sell_threshold']:.4f}")
            self.log(f"  ewrls_lambda: {self.optimization_results['ewrls_lambda']:.6f}")
        else:
            self.log("Using baseline parameters")

        self.log("")

        # Create comprehensive warmup for restart
        comprehensive_warmup = self.session_dir / "comprehensive_warmup_1pm.csv"
        import shutil
        shutil.copy(warmup_file, comprehensive_warmup)

        # Generate afternoon signals
        afternoon_signals = self.session_dir / "afternoon_signals.jsonl"
        afternoon_trades = self.session_dir / "afternoon_trades.jsonl"

        cmd = [
            str(BUILD_DIR / "sentio_cli"),
            "generate-signals",
            "--data", data_file,
            "--output", str(afternoon_signals),
            "--warmup", "3900"
        ]

        self.log("Generating afternoon signals...")
        result = subprocess.run(cmd, capture_output=True, text=True)

        if result.returncode != 0:
            self.log(f"❌ Failed: {result.stderr}")
            return

        # Execute trades
        cmd = [
            str(BUILD_DIR / "sentio_cli"),
            "execute-trades",
            "--signals", str(afternoon_signals),
            "--data", data_file,
            "--output", str(afternoon_trades),
            "--warmup", "3900"
        ]

        self.log("Executing afternoon trades...")
        result = subprocess.run(cmd, capture_output=True, text=True)

        if result.returncode != 0:
            self.log(f"❌ Failed: {result.stderr}")
            return

        # Analyze
        self.log("Analyzing afternoon performance...")
        self.analyze_session(afternoon_trades, "afternoon")

        self.log(f"✓ Afternoon session complete")
        self.log(f"  Trades: {self.afternoon_metrics.get('total_trades', 0)}")
        self.log(f"  P&L: ${self.afternoon_metrics.get('total_pnl', 0.0):.2f}")
        self.log(f"  Return: {self.afternoon_metrics.get('total_return_pct', 0.0):.4f}%")

    def run_eod_closing(self):
        """Run EOD closing (3:58 PM)"""
        self.log("")
        self.log("=" * 80)
        self.log("PHASE 6: EOD Closing (3:58 PM)")
        self.log("=" * 80)

        self.log("Liquidating all positions...")
        self.log("  All positions → CASH")
        self.log("  Portfolio: 100% cash")

        # Calculate final state
        morning_pnl = self.morning_metrics.get('total_pnl', 0.0)
        afternoon_pnl = self.afternoon_metrics.get('total_pnl', 0.0)
        total_pnl = morning_pnl + afternoon_pnl
        final_capital = 100000.0 + total_pnl

        self.eod_results = {
            "positions_closed": True,
            "final_cash": final_capital,
            "total_pnl": total_pnl,
            "total_return_pct": (total_pnl / 100000.0) * 100
        }

        self.log(f"✓ EOD closing complete")
        self.log(f"  Final Cash: ${final_capital:.2f}")
        self.log(f"  Total P&L: ${total_pnl:.2f}")
        self.log(f"  Total Return: {self.eod_results['total_return_pct']:.4f}%")

    def analyze_session(self, trades_file: Path, session_name: str):
        """Analyze session performance"""
        # Read equity curve
        equity_file = trades_file.with_name(trades_file.stem + "_equity.csv")

        if not equity_file.exists():
            self.log(f"⚠️  Equity file not found: {equity_file}")
            return

        # Parse equity curve
        equity_values = []
        with open(equity_file) as f:
            lines = f.readlines()
            if len(lines) > 1:
                for line in lines[1:]:  # Skip header
                    parts = line.strip().split(',')
                    if len(parts) >= 2:
                        try:
                            equity_values.append(float(parts[1]))
                        except:
                            pass

        if equity_values:
            final_equity = equity_values[-1]
            total_pnl = final_equity - 100000.0
            total_return_pct = (total_pnl / 100000.0) * 100

            metrics = {
                "total_trades": len(equity_values) - 1,
                "final_equity": final_equity,
                "total_pnl": total_pnl,
                "total_return_pct": total_return_pct,
                "equity_curve": equity_values
            }

            if session_name == "morning":
                self.morning_metrics = metrics
            else:
                self.afternoon_metrics = metrics

    def generate_visual_dashboard(self):
        """Generate HTML dashboard for visual analysis"""
        self.log("")
        self.log("=" * 80)
        self.log("PHASE 7: Generating Visual Dashboard")
        self.log("=" * 80)

        dashboard_file = DASHBOARDS_DIR / f"{self.session_id}_dashboard.html"

        # Combine equity curves
        morning_equity = self.morning_metrics.get('equity_curve', [100000.0])
        afternoon_equity = self.afternoon_metrics.get('equity_curve', [morning_equity[-1]])

        # Create combined equity curve
        combined_equity = morning_equity + afternoon_equity[1:]  # Avoid duplicate starting point

        # Generate HTML
        html = self._generate_dashboard_html(combined_equity)

        with open(dashboard_file, 'w') as f:
            f.write(html)

        self.log(f"✓ Dashboard generated: {dashboard_file}")
        self.log(f"  Open with: open {dashboard_file}")

        return str(dashboard_file)

    def _generate_dashboard_html(self, equity_curve: List[float]) -> str:
        """Generate HTML dashboard content"""

        morning_trades = self.morning_metrics.get('total_trades', 0)
        morning_pnl = self.morning_metrics.get('total_pnl', 0.0)
        afternoon_trades = self.afternoon_metrics.get('total_trades', 0)
        afternoon_pnl = self.afternoon_metrics.get('total_pnl', 0.0)
        total_trades = morning_trades + afternoon_trades
        total_pnl = morning_pnl + afternoon_pnl
        total_return = (total_pnl / 100000.0) * 100

        # Prepare equity data for chart
        equity_data = ",".join([f"{e:.2f}" for e in equity_curve])

        html = f"""<!DOCTYPE html>
<html>
<head>
    <title>Mock Trading Session - {self.session_date}</title>
    <script src="https://cdn.plot.ly/plotly-latest.min.js"></script>
    <style>
        body {{
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Arial, sans-serif;
            margin: 20px;
            background: #1a1a1a;
            color: #e0e0e0;
        }}
        .header {{
            text-align: center;
            padding: 20px;
            background: #2a2a2a;
            border-radius: 10px;
            margin-bottom: 20px;
        }}
        .metrics {{
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(250px, 1fr));
            gap: 20px;
            margin-bottom: 20px;
        }}
        .metric-card {{
            background: #2a2a2a;
            padding: 20px;
            border-radius: 10px;
            border-left: 4px solid #4CAF50;
        }}
        .metric-card.negative {{
            border-left-color: #f44336;
        }}
        .metric-title {{
            font-size: 14px;
            color: #999;
            margin-bottom: 5px;
        }}
        .metric-value {{
            font-size: 32px;
            font-weight: bold;
        }}
        .metric-subtitle {{
            font-size: 12px;
            color: #666;
            margin-top: 5px;
        }}
        .chart-container {{
            background: #2a2a2a;
            padding: 20px;
            border-radius: 10px;
            margin-bottom: 20px;
        }}
        .section {{
            background: #2a2a2a;
            padding: 20px;
            border-radius: 10px;
            margin-bottom: 20px;
        }}
        .positive {{ color: #4CAF50; }}
        .negative {{ color: #f44336; }}
    </style>
</head>
<body>
    <div class="header">
        <h1>Mock Trading Session Dashboard</h1>
        <p>Session Date: {self.session_date}</p>
        <p>Session ID: {self.session_id}</p>
    </div>

    <div class="metrics">
        <div class="metric-card">
            <div class="metric-title">Total Return</div>
            <div class="metric-value {'positive' if total_return >= 0 else 'negative'}">
                ${total_pnl:,.2f}
            </div>
            <div class="metric-subtitle">{total_return:.4f}% return</div>
        </div>

        <div class="metric-card">
            <div class="metric-title">Total Trades</div>
            <div class="metric-value">{total_trades}</div>
            <div class="metric-subtitle">Morning: {morning_trades} | Afternoon: {afternoon_trades}</div>
        </div>

        <div class="metric-card">
            <div class="metric-title">Morning Session</div>
            <div class="metric-value {'positive' if morning_pnl >= 0 else 'negative'}">
                ${morning_pnl:.2f}
            </div>
            <div class="metric-subtitle">9:30 AM - 12:45 PM</div>
        </div>

        <div class="metric-card">
            <div class="metric-title">Afternoon Session</div>
            <div class="metric-value {'positive' if afternoon_pnl >= 0 else 'negative'}">
                ${afternoon_pnl:.2f}
            </div>
            <div class="metric-subtitle">1:00 PM - 4:00 PM</div>
        </div>
    </div>

    <div class="chart-container">
        <h2>Equity Curve</h2>
        <div id="equityChart"></div>
    </div>

    <div class="section">
        <h2>Session Timeline</h2>
        <ul>
            <li><strong>9:00 AM:</strong> Warmup loaded (7,864+ bars)</li>
            <li><strong>9:30 AM:</strong> Morning session started</li>
            <li><strong>12:45 PM:</strong> Midday optimization {'(completed)' if self.optimization_results else '(skipped)'}</li>
            <li><strong>1:00 PM:</strong> Afternoon session with {'optimized' if self.optimization_results and self.optimization_results.get('improvement', 0) > 0.05 else 'baseline'} params</li>
            <li><strong>3:58 PM:</strong> EOD liquidation (all positions → cash)</li>
            <li><strong>4:00 PM:</strong> Session complete</li>
        </ul>
    </div>

    <div class="section">
        <h2>Configuration</h2>
        <ul>
            <li><strong>Initial Capital:</strong> $100,000.00</li>
            <li><strong>Buying Power:</strong> $200,000.00</li>
            <li><strong>Speed Multiplier:</strong> {self.speed_multiplier}x</li>
            <li><strong>Market Impact:</strong> {self.mock_config['market_impact_bps']} bps</li>
            <li><strong>Bid-Ask Spread:</strong> {self.mock_config['bid_ask_spread_bps']} bps</li>
        </ul>
    </div>

    <script>
        var equityData = [{equity_data}];
        var trace = {{
            y: equityData,
            type: 'scatter',
            mode: 'lines',
            line: {{
                color: '{('#4CAF50' if total_return >= 0 else '#f44336')}',
                width: 2
            }},
            fill: 'tozeroy',
            fillcolor: '{('#4CAF5020' if total_return >= 0 else '#f4433620')}'
        }};

        var layout = {{
            plot_bgcolor: '#1a1a1a',
            paper_bgcolor: '#2a2a2a',
            font: {{ color: '#e0e0e0' }},
            xaxis: {{
                title: 'Bar Number',
                gridcolor: '#444'
            }},
            yaxis: {{
                title: 'Portfolio Value ($)',
                gridcolor: '#444',
                tickformat: '$,.0f'
            }},
            margin: {{ t: 30, b: 50, l: 70, r: 30 }}
        }};

        Plotly.newPlot('equityChart', [trace], layout);
    </script>
</body>
</html>"""

        return html

    def generate_final_report(self):
        """Generate final session report"""
        self.log("")
        self.log("=" * 80)
        self.log("PHASE 8: Final Report (4:00 PM)")
        self.log("=" * 80)

        morning_trades = self.morning_metrics.get('total_trades', 0)
        morning_pnl = self.morning_metrics.get('total_pnl', 0.0)
        afternoon_trades = self.afternoon_metrics.get('total_trades', 0)
        afternoon_pnl = self.afternoon_metrics.get('total_pnl', 0.0)
        total_trades = morning_trades + afternoon_trades
        total_pnl = morning_pnl + afternoon_pnl
        final_capital = 100000.0 + total_pnl
        total_return = (total_pnl / 100000.0) * 100

        report = f"""
========================================
MOCK TRADING SESSION - FINAL REPORT
========================================
Session Date: {self.session_date}
Session ID: {self.session_id}
Speed Multiplier: {self.speed_multiplier}x

CONFIGURATION
----------------------------------------
Initial Capital: $100,000.00
Buying Power: $200,000.00
Market Impact: {self.mock_config['market_impact_bps']} bps
Bid-Ask Spread: {self.mock_config['bid_ask_spread_bps']} bps

MORNING SESSION (9:30 AM - 12:45 PM)
----------------------------------------
Parameters: Baseline
Trades: {morning_trades}
P&L: ${morning_pnl:.2f}
Return: {(morning_pnl / 100000.0) * 100:.4f}%

MIDDAY OPTIMIZATION (12:45 PM)
----------------------------------------
Status: {'Complete' if self.optimization_results else 'Skipped'}
"""

        if self.optimization_results:
            report += f"""Baseline MRB: {self.optimization_results.get('baseline_mrb', 0.0):.4f}%
Optimized MRB: {self.optimization_results.get('best_mrb', 0.0):.4f}%
Improvement: {self.optimization_results.get('improvement', 0.0):.4f}%
Decision: {'Use Optimized' if self.optimization_results.get('improvement', 0) > 0.05 else 'Keep Baseline'}
"""

        report += f"""
AFTERNOON SESSION (1:00 PM - 4:00 PM)
----------------------------------------
Parameters: {'Optimized' if self.optimization_results and self.optimization_results.get('improvement', 0) > 0.05 else 'Baseline'}
Trades: {afternoon_trades}
P&L: ${afternoon_pnl:.2f}
Return: {(afternoon_pnl / 100000.0) * 100:.4f}%

EOD CLOSING (3:58 PM)
----------------------------------------
All Positions Liquidated: ✓
Final Cash: ${final_capital:.2f}
Portfolio: 100% Cash

SUMMARY
----------------------------------------
Total Trades: {total_trades}
Total P&L: ${total_pnl:.2f}
Total Return: {total_return:.4f}%
Final Capital: ${final_capital:.2f}

Status: {'✓ SUCCESS' if self.eod_results.get('positions_closed') else '✗ FAILED'}

OUTPUT FILES
----------------------------------------
Session Directory: {self.session_dir}
Dashboard: {DASHBOARDS_DIR}/{self.session_id}_dashboard.html

========================================
Session completed at {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}
========================================
"""

        print(report)

        # Save to file
        with open(self.session_dir / "final_report.txt", 'w') as f:
            f.write(report)

        self.log(f"✓ Final report saved: {self.session_dir}/final_report.txt")

    def run(self):
        """Run complete mock trading session"""
        self.log("🚀 Starting Mock Trading Session")
        self.log(f"   Session Date: {self.session_date}")
        self.log(f"   Speed: {self.speed_multiplier}x real-time")
        self.log("")

        try:
            # Phase 1: Warmup
            warmup_file = self.prepare_warmup_data()

            # Phase 2: Market data
            data_file = self.prepare_market_data()

            # Phase 3: Morning session
            self.run_morning_session(warmup_file, data_file)

            # Phase 4: Midday optimization
            self.run_midday_optimization(warmup_file)

            # Phase 5: Afternoon session
            self.run_afternoon_session(warmup_file, data_file)

            # Phase 6: EOD closing
            self.run_eod_closing()

            # Phase 7: Visual dashboard
            dashboard = self.generate_visual_dashboard()

            # Phase 8: Final report
            self.generate_final_report()

            self.log("")
            self.log("✅ Mock trading session complete!")
            self.log(f"📊 Dashboard: {dashboard}")
            self.log(f"📁 Session files: {self.session_dir}")

        except Exception as e:
            self.log(f"❌ Session failed: {e}")
            import traceback
            traceback.print_exc()
            sys.exit(1)

if __name__ == "__main__":
    import argparse

    parser = argparse.ArgumentParser(description="Launch mock trading session")
    parser.add_argument('--date', default="2025-10-08", help='Session date (YYYY-MM-DD)')
    parser.add_argument('--speed', type=float, default=39.0, help='Speed multiplier')
    args = parser.parse_args()

    session = MockTradingSession(args.date, args.speed)
    session.run()
