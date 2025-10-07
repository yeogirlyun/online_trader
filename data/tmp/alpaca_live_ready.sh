#!/bin/bash
# Alpaca Paper Live Test - Ready for Tonight
# ==========================================
#
# Quick test using existing SPY data to prepare for live paper trading.
# This uses SPY_3days.csv (2 warmup blocks + 1 test block)

set -e

# Config
WARMUP_BLOCKS=2
TEST_BLOCKS=1
WARMUP_BARS=$((WARMUP_BLOCKS * 480))

BASE_DIR="/Volumes/ExternalSSD/Dev/C++/online_trader"
BUILD_DIR="$BASE_DIR/build"
DATA_DIR="$BASE_DIR/data"
TMP_DIR="$DATA_DIR/tmp"
DASHBOARD_DIR="$DATA_DIR/dashboards"

mkdir -p "$DASHBOARD_DIR"

TIMESTAMP=$(date +%Y%m%d_%H%M%S)
DATA_FILE="$DATA_DIR/equities/SPY_3days.csv"
SIGNALS_FILE="$TMP_DIR/spy_alpaca_ready_signals_${TIMESTAMP}.jsonl"
TRADES_FILE="$TMP_DIR/spy_alpaca_ready_trades_${TIMESTAMP}.jsonl"
EQUITY_FILE="$TMP_DIR/spy_alpaca_ready_equity_${TIMESTAMP}.csv"
DASHBOARD_FILE="$DASHBOARD_DIR/alpaca_live_ready_${TIMESTAMP}.html"

echo ""
echo "================================================"
echo " Alpaca Paper Live Test - Ready Check (v1.4)"
echo "================================================"
echo ""
echo "Using SPY_3days.csv (2 warmup + 1 test blocks)"
echo "Target MRB: ≥0.340% (v1.4 baseline)"
echo ""

# Step 1: Generate signals
echo "Step 1: Generating signals..."
cd "$BUILD_DIR"

time ./sentio_cli generate-signals \
    --data "$DATA_FILE" \
    --output "$SIGNALS_FILE" \
    --warmup $WARMUP_BARS 2>&1 | grep -v "DEBUG:"

echo ""

# Step 2: Execute trades
echo "Step 2: Executing trades..."

./sentio_cli execute-trades \
    --signals "$SIGNALS_FILE" \
    --data "$DATA_FILE" \
    --output "$TRADES_FILE" \
    --warmup $WARMUP_BARS 2>&1 | grep -v "DEBUG:"

echo ""

# Step 3: Analyze
echo "Step 3: Analyzing..."

./sentio_cli analyze-trades \
    --trades "$TRADES_FILE" \
    --output "$EQUITY_FILE" 2>&1

echo ""

# Step 4: Generate dashboard
echo "Step 4: Generating dashboard..."

python3 << PYTHON
import json
import pandas as pd
from datetime import datetime

warmup_bars = 960
test_blocks = 1

# Read trades
trades = []
with open("$TRADES_FILE", "r") as f:
    for line in f:
        try:
            trades.append(json.loads(line))
        except:
            pass

# Read equity
try:
    equity_df = pd.read_csv("$EQUITY_FILE")
except:
    equity_df = pd.DataFrame()

# Calculate metrics
if trades:
    test_trades = [t for t in trades if t.get('bar_index', 0) >= warmup_bars]

    if test_trades and 'total_return' in test_trades[-1]:
        final_return = test_trades[-1]['total_return'] * 100
        mrb = final_return / test_blocks
    else:
        final_return = 0.0
        mrb = 0.0

    num_trades = len([t for t in trades if t.get('action') in ['BUY', 'SELL', 'SHORT', 'COVER']])

    completed_trades = [t for t in trades if 'pnl' in t and t['pnl'] != 0]
    if completed_trades:
        winning_trades = sum(1 for t in completed_trades if t['pnl'] > 0)
        win_rate = (winning_trades / len(completed_trades)) * 100
    else:
        win_rate = 0.0

    if len(equity_df) > 1:
        returns = equity_df['equity'].pct_change().dropna()
        if len(returns) > 0 and returns.std() > 0:
            sharpe = (returns.mean() / returns.std()) * (252 ** 0.5)
        else:
            sharpe = 0.0
    else:
        sharpe = 0.0

    if len(equity_df) > 0:
        running_max = equity_df['equity'].cummax()
        drawdown = (equity_df['equity'] - running_max) / running_max
        max_dd = drawdown.min() * 100
    else:
        max_dd = 0.0
else:
    final_return = 0.0
    mrb = 0.0
    num_trades = 0
    win_rate = 0.0
    sharpe = 0.0
    max_dd = 0.0

print("")
print("=" * 60)
print("ALPACA PAPER LIVE TEST - READY CHECK")
print("=" * 60)
print(f"Test Period Return: {final_return:+.2f}%")
print(f"MRB: {mrb:+.3f}% (target: ≥0.340%)")
print(f"Trades: {num_trades}")
print(f"Win Rate: {win_rate:.1f}%")
print(f"Sharpe: {sharpe:.2f}")
print(f"Max Drawdown: {max_dd:.2f}%")
print("")

if mrb >= 0.340:
    print("✅ PASS: MRB meets v1.4 target!")
    print("   System ready for Alpaca paper trading")
    status = "READY"
    status_color = "#27ae60"
else:
    print(f"⚠️  WARNING: MRB below target ({mrb:.3f}% < 0.340%)")
    print("   Review results before live trading")
    status = "REVIEW NEEDED"
    status_color = "#e67e22"

print("")

# Generate dashboard
html = f"""<!DOCTYPE html>
<html>
<head>
    <title>Alpaca Live Ready Check - {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}</title>
    <script src="https://cdn.plot.ly/plotly-latest.min.js"></script>
    <style>
        body {{
            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
            margin: 20px;
            background-color: #f5f5f5;
        }}
        .header {{
            background-color: #2c3e50;
            color: white;
            padding: 20px;
            border-radius: 8px;
            margin-bottom: 20px;
        }}
        .status {{
            background-color: {status_color};
            color: white;
            padding: 15px;
            border-radius: 8px;
            font-size: 18px;
            font-weight: bold;
            text-align: center;
            margin-bottom: 20px;
        }}
        .metrics {{
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(200px, 1fr));
            gap: 15px;
            margin-bottom: 20px;
        }}
        .metric-card {{
            background-color: white;
            padding: 20px;
            border-radius: 8px;
            box-shadow: 0 2px 4px rgba(0,0,0,0.1);
        }}
        .metric-label {{
            font-size: 14px;
            color: #7f8c8d;
            margin-bottom: 5px;
        }}
        .metric-value {{
            font-size: 28px;
            font-weight: bold;
            color: #2c3e50;
        }}
        .metric-value.positive {{ color: #27ae60; }}
        .metric-value.negative {{ color: #e74c3c; }}
        .chart-container {{
            background-color: white;
            padding: 20px;
            border-radius: 8px;
            box-shadow: 0 2px 4px rgba(0,0,0,0.1);
            margin-bottom: 20px;
        }}
    </style>
</head>
<body>
    <div class="header">
        <h1>🚀 Alpaca Paper Live Test - Ready Check</h1>
        <p>Generated: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}</p>
        <p>Strategy: OnlineEnsemble v1.4 (Production Baseline)</p>
    </div>

    <div class="status">
        {status}
    </div>

    <div class="metrics">
        <div class="metric-card">
            <div class="metric-label">Total Return</div>
            <div class="metric-value {'positive' if final_return >= 0 else 'negative'}">
                {final_return:+.2f}%
            </div>
        </div>
        <div class="metric-card">
            <div class="metric-label">MRB (Target: ≥0.340%)</div>
            <div class="metric-value {'positive' if mrb >= 0.340 else 'negative'}">
                {mrb:+.3f}%
            </div>
        </div>
        <div class="metric-card">
            <div class="metric-label">Trades</div>
            <div class="metric-value">{num_trades}</div>
        </div>
        <div class="metric-card">
            <div class="metric-label">Win Rate</div>
            <div class="metric-value">{win_rate:.1f}%</div>
        </div>
        <div class="metric-card">
            <div class="metric-label">Sharpe Ratio</div>
            <div class="metric-value {'positive' if sharpe >= 0 else 'negative'}">
                {sharpe:.2f}
            </div>
        </div>
        <div class="metric-card">
            <div class="metric-label">Max Drawdown</div>
            <div class="metric-value negative">
                {max_dd:.2f}%
            </div>
        </div>
    </div>

    <div class="chart-container">
        <h3>📈 Equity Curve</h3>
        <div id="equity-chart"></div>
    </div>

    <script>
        var equityData = {{
            x: {equity_df['bar_index'].tolist() if len(equity_df) > 0 else []},
            y: {equity_df['equity'].tolist() if len(equity_df) > 0 else []},
            type: 'scatter',
            mode: 'lines',
            name: 'Equity',
            line: {{color: '#3498db', width: 2}}
        }};

        var equityLayout = {{
            xaxis: {{title: 'Bar Index'}},
            yaxis: {{title: 'Equity'}},
            hovermode: 'closest',
            shapes: [{{
                type: 'line',
                x0: {warmup_bars},
                x1: {warmup_bars},
                y0: 0,
                y1: 1,
                yref: 'paper',
                line: {{color: '#e67e22', width: 2, dash: 'dash'}}
            }}],
            annotations: [{{
                x: {warmup_bars},
                y: 1,
                yref: 'paper',
                text: 'Test Starts',
                showarrow: true,
                arrowhead: 2,
                ax: -40,
                ay: -40
            }}]
        }};

        Plotly.newPlot('equity-chart', [equityData], equityLayout);
    </script>
</body>
</html>
"""

with open("$DASHBOARD_FILE", "w") as f:
    f.write(html)

print(f"✅ Dashboard: $DASHBOARD_FILE")
PYTHON

echo ""
echo "================================================"
echo "Files:"
echo "  Dashboard: $DASHBOARD_FILE"
echo "  Signals: $SIGNALS_FILE"
echo "  Trades: $TRADES_FILE"
echo "  Equity: $EQUITY_FILE"
echo "================================================"
echo ""

# Open dashboard
if command -v open &> /dev/null; then
    open "$DASHBOARD_FILE"
fi
