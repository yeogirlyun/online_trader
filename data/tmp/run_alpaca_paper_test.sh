#!/bin/bash
# Alpaca Paper Live Test - Simplified Version
# ===========================================
#
# This script prepares for Alpaca paper trading by:
# 1. Using 2 blocks of SPY historical data for warmup (960 bars)
# 2. Generating realistic 1-block continuation for live test (480 bars)
# 3. Running OnlineEnsemble v1.4 strategy
# 4. Generating performance dashboard
#
# Usage: ./run_alpaca_paper_test.sh

set -e  # Exit on error

# Configuration
SYMBOL="SPY"
WARMUP_BLOCKS=2
TEST_BLOCKS=1
WARMUP_BARS=$((WARMUP_BLOCKS * 480))
TEST_BARS=$((TEST_BLOCKS * 480))
TOTAL_BARS=$((WARMUP_BARS + TEST_BARS))

# Directories
BASE_DIR="/Volumes/ExternalSSD/Dev/C++/online_trader"
DATA_DIR="$BASE_DIR/data"
TMP_DIR="$DATA_DIR/tmp"
DASHBOARD_DIR="$DATA_DIR/dashboards"
BUILD_DIR="$BASE_DIR/build"

# Ensure directories exist
mkdir -p "$TMP_DIR"
mkdir -p "$DASHBOARD_DIR"

# Output files
TIMESTAMP=$(date +%Y%m%d_%H%M%S)
WARMUP_DATA="$TMP_DIR/spy_warmup_2blocks.csv"
LIVE_DATA="$TMP_DIR/spy_live_1block_${TIMESTAMP}.csv"
COMBINED_DATA="$TMP_DIR/SPY_combined_${TIMESTAMP}.csv"
SIGNALS_FILE="$TMP_DIR/spy_live_signals_${TIMESTAMP}.jsonl"
TRADES_FILE="$TMP_DIR/spy_live_trades_${TIMESTAMP}.jsonl"
TRADES_EQUITY_FILE="$TMP_DIR/spy_live_trades_equity_${TIMESTAMP}.csv"
DASHBOARD_FILE="$DASHBOARD_DIR/spy_live_test_${TIMESTAMP}.html"
LOG_FILE="$TMP_DIR/alpaca_live_test_${TIMESTAMP}.log"

# Logging function
log() {
    echo "[$(date '+%Y-%m-%d %H:%M:%S')] $1" | tee -a "$LOG_FILE"
}

echo ""
echo "=========================================="
echo " Alpaca Paper Live Test - SPY"
echo "=========================================="
echo ""
echo "Configuration:"
echo "  Symbol: $SYMBOL"
echo "  Warmup: $WARMUP_BLOCKS blocks ($WARMUP_BARS bars)"
echo "  Live Test: $TEST_BLOCKS block ($TEST_BARS bars)"
echo "  Strategy: OnlineEnsemble v1.4"
echo ""

# Step 1: Prepare warmup data (2 blocks from SPY_3days.csv)
echo "Step 1: Preparing warmup data..."

if [ ! -f "$WARMUP_DATA" ]; then
    log "Extracting $WARMUP_BARS bars from SPY_3days.csv..."
    head -n $((WARMUP_BARS + 1)) "$DATA_DIR/equities/SPY_3days.csv" > "$WARMUP_DATA"
fi

WARMUP_LINE_COUNT=$(wc -l < "$WARMUP_DATA")
log "✅ Warmup data ready: $((WARMUP_LINE_COUNT - 1)) bars"

# Step 2: Generate realistic live test data
echo ""
echo "Step 2: Generating realistic live test data..."

python3 "$TMP_DIR/generate_live_test_data.py" \
    "$WARMUP_DATA" \
    "$LIVE_DATA" \
    $TEST_BARS 2>&1 | tee -a "$LOG_FILE"

if [ $? -ne 0 ]; then
    log "❌ Failed to generate live test data"
    exit 1
fi

# Step 3: Combine warmup + live data
echo ""
echo "Step 3: Combining warmup and live data..."

cat "$WARMUP_DATA" > "$COMBINED_DATA"
tail -n +2 "$LIVE_DATA" >> "$COMBINED_DATA"

COMBINED_LINE_COUNT=$(wc -l < "$COMBINED_DATA")
log "✅ Combined data: $((COMBINED_LINE_COUNT - 1)) bars"

# Step 4: Generate signals with warmup
echo ""
echo "Step 4: Generating signals (v1.4 strategy)..."

cd "$BUILD_DIR"

echo "Running signal generation with $WARMUP_BARS-bar warmup..."
time ./sentio_cli generate-signals \
    --data "$COMBINED_DATA" \
    --output "$SIGNALS_FILE" \
    --warmup $WARMUP_BARS 2>&1 | tee -a "$LOG_FILE"

if [ $? -ne 0 ]; then
    log "❌ Signal generation failed"
    exit 1
fi

SIGNAL_COUNT=$(wc -l < "$SIGNALS_FILE")
log "✅ Generated $SIGNAL_COUNT signals"

# Step 5: Execute trades
echo ""
echo "Step 5: Executing trades..."

./sentio_cli execute-trades \
    --signals "$SIGNALS_FILE" \
    --data "$COMBINED_DATA" \
    --output "$TRADES_FILE" \
    --warmup $WARMUP_BARS 2>&1 | tee -a "$LOG_FILE"

if [ $? -ne 0 ]; then
    log "❌ Trade execution failed"
    exit 1
fi

TRADE_COUNT=$(wc -l < "$TRADES_FILE")
log "✅ Executed $TRADE_COUNT trades"

# Step 6: Analyze trades
echo ""
echo "Step 6: Analyzing trades..."

./sentio_cli analyze-trades \
    --trades "$TRADES_FILE" \
    --output "$TRADES_EQUITY_FILE" 2>&1 | tee -a "$LOG_FILE"

if [ $? -ne 0 ]; then
    log "❌ Trade analysis failed"
    exit 1
fi

log "✅ Trade analysis completed"

# Step 7: Extract performance metrics
echo ""
echo "Step 7: Calculating performance metrics..."

WARMUP_BARS_VAR=$WARMUP_BARS
TEST_BLOCKS_VAR=$TEST_BLOCKS
TRADES_FILE_VAR="$TRADES_FILE"
TRADES_EQUITY_FILE_VAR="$TRADES_EQUITY_FILE"
DASHBOARD_FILE_VAR="$DASHBOARD_FILE"
TIMESTAMP_VAR="$TIMESTAMP"

python3 << PYTHON_SCRIPT
import json
import pandas as pd
import sys
from datetime import datetime

warmup_bars = $WARMUP_BARS_VAR
test_blocks = $TEST_BLOCKS_VAR

# Read trades
trades = []
with open("$TRADES_FILE_VAR", "r") as f:
    for line in f:
        try:
            trades.append(json.loads(line))
        except:
            pass

# Read equity curve
try:
    equity_df = pd.read_csv("$TRADES_EQUITY_FILE_VAR")
except:
    equity_df = pd.DataFrame(columns=['bar_index', 'equity'])

# Calculate performance metrics
if trades:
    # Get test period trades (after warmup)
    test_trades = [t for t in trades if t.get('bar_index', 0) >= warmup_bars]

    if test_trades and 'total_return' in test_trades[-1]:
        final_return = test_trades[-1]['total_return'] * 100
        mrb = final_return / test_blocks
    else:
        final_return = 0.0
        mrb = 0.0

    # Total number of trades (excluding HOLD)
    num_trades = len([t for t in trades if t.get('action') in ['BUY', 'SELL', 'SHORT', 'COVER']])

    # Win rate
    completed_trades = [t for t in trades if 'pnl' in t and t['pnl'] != 0]
    if completed_trades:
        winning_trades = sum(1 for t in completed_trades if t['pnl'] > 0)
        win_rate = (winning_trades / len(completed_trades)) * 100
    else:
        win_rate = 0.0

    # Sharpe ratio (simplified)
    if len(equity_df) > 1:
        returns = equity_df['equity'].pct_change().dropna()
        if len(returns) > 0 and returns.std() > 0:
            sharpe = (returns.mean() / returns.std()) * (252 ** 0.5)  # Annualized
        else:
            sharpe = 0.0
    else:
        sharpe = 0.0

    # Max drawdown
    if len(equity_df) > 0:
        running_max = equity_df['equity'].cummax()
        drawdown = (equity_df['equity'] - running_max) / running_max
        max_drawdown = drawdown.min() * 100
    else:
        max_drawdown = 0.0
else:
    final_return = 0.0
    mrb = 0.0
    num_trades = 0
    win_rate = 0.0
    sharpe = 0.0
    max_drawdown = 0.0

print(f"")
print(f"========================================")
print(f"Performance Summary")
print(f"========================================")
print(f"Total Return (Test Period): {final_return:+.2f}%")
print(f"MRB (Mean Return per Block): {mrb:+.3f}%")
print(f"Target MRB (v1.4): ≥0.340%")
print(f"Number of Trades: {num_trades}")
print(f"Win Rate: {win_rate:.1f}%")
print(f"Sharpe Ratio: {sharpe:.2f}")
print(f"Max Drawdown: {max_drawdown:.2f}%")
print(f"")

# Generate HTML dashboard
html = f"""<!DOCTYPE html>
<html>
<head>
    <title>Alpaca Paper Live Test - {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}</title>
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
        .metric-value.target-met {{ color: #27ae60; font-weight: bold; }}
        .metric-value.target-missed {{ color: #e67e22; font-weight: bold; }}
        .chart-container {{
            background-color: white;
            padding: 20px;
            border-radius: 8px;
            box-shadow: 0 2px 4px rgba(0,0,0,0.1);
            margin-bottom: 20px;
        }}
        .info-section {{
            background-color: #ecf0f1;
            padding: 15px;
            border-radius: 8px;
            margin-bottom: 20px;
        }}
        .badge {{
            display: inline-block;
            padding: 5px 10px;
            border-radius: 4px;
            font-size: 12px;
            font-weight: bold;
            margin-right: 10px;
        }}
        .badge-warmup {{ background-color: #3498db; color: white; }}
        .badge-live {{ background-color: #e67e22; color: white; }}
        .badge-v14 {{ background-color: #27ae60; color: white; }}
    </style>
</head>
<body>
    <div class="header">
        <h1>🚀 Alpaca Paper Live Test - SPY</h1>
        <p>Generated: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}</p>
        <div>
            <span class="badge badge-warmup">Warmup: {warmup_bars} bars</span>
            <span class="badge badge-live">Live Test: {test_blocks} block</span>
            <span class="badge badge-v14">v1.4 Production</span>
        </div>
    </div>

    <div class="info-section">
        <h3>📊 Test Configuration</h3>
        <p><strong>Strategy:</strong> OnlineEnsemble v1.4 (Multi-Horizon EWRLS)</p>
        <p><strong>Warmup:</strong> {warmup_bars} bars (2 days historical data)</p>
        <p><strong>Live Test:</strong> {test_blocks} block (1 day simulated continuation)</p>
        <p><strong>Target MRB:</strong> ≥0.340% (v1.4 baseline)</p>
    </div>

    <div class="metrics">
        <div class="metric-card">
            <div class="metric-label">Total Return</div>
            <div class="metric-value {'positive' if final_return >= 0 else 'negative'}">
                {final_return:+.2f}%
            </div>
        </div>
        <div class="metric-card">
            <div class="metric-label">MRB (Mean Return per Block)</div>
            <div class="metric-value {'target-met' if mrb >= 0.340 else 'target-missed'}">
                {mrb:+.3f}%
            </div>
            <div style="font-size: 12px; color: {'#27ae60' if mrb >= 0.340 else '#e67e22'};">
                {'✅ Target met!' if mrb >= 0.340 else '⚠️ Below target (0.340%)'}
            </div>
        </div>
        <div class="metric-card">
            <div class="metric-label">Number of Trades</div>
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
                {max_drawdown:.2f}%
            </div>
        </div>
    </div>

    <div class="chart-container">
        <h3>📈 Equity Curve</h3>
        <div id="equity-chart"></div>
    </div>

    <div class="chart-container">
        <h3>📊 Trade Analysis</h3>
        <div id="trade-chart"></div>
    </div>

    <script>
        // Equity curve
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
                text: 'Live Test Starts',
                showarrow: true,
                arrowhead: 2,
                ax: -40,
                ay: -40
            }}]
        }};

        Plotly.newPlot('equity-chart', [equityData], equityLayout);

        // Trade distribution
        var tradeTypes = {{}};
        {json.dumps(trades)}.forEach(function(trade) {{
            if (trade.action && trade.action !== 'HOLD') {{
                tradeTypes[trade.action] = (tradeTypes[trade.action] || 0) + 1;
            }}
        }});

        var tradeData = [{{
            x: Object.keys(tradeTypes),
            y: Object.values(tradeTypes),
            type: 'bar',
            marker: {{color: '#3498db'}}
        }}];

        var tradeLayout = {{
            xaxis: {{title: 'Trade Type'}},
            yaxis: {{title: 'Count'}}
        }};

        Plotly.newPlot('trade-chart', tradeData, tradeLayout);
    </script>
</body>
</html>
"""

# Write dashboard
with open("$DASHBOARD_FILE_VAR", "w") as f:
    f.write(html)

print(f"✅ Dashboard: $DASHBOARD_FILE_VAR")
print(f"")

# Status message
if mrb >= 0.340:
    print(f"✅ SUCCESS: MRB meets v1.4 target (≥0.340%)")
    print(f"   Ready for Alpaca paper trading!")
else:
    print(f"⚠️  WARNING: MRB below v1.4 target ({mrb:.3f}% < 0.340%)")
    print(f"   Review results before proceeding to live trading")
PYTHON_SCRIPT

echo ""
echo "=========================================="
echo "Alpaca Paper Live Test Completed!"
echo "=========================================="
echo ""
echo "Output Files:"
echo "  Warmup Data: $WARMUP_DATA"
echo "  Live Data: $LIVE_DATA"
echo "  Combined Data: $COMBINED_DATA"
echo "  Signals: $SIGNALS_FILE"
echo "  Trades: $TRADES_FILE"
echo "  Equity Curve: $TRADES_EQUITY_FILE"
echo "  Dashboard: $DASHBOARD_FILE"
echo "  Log: $LOG_FILE"
echo ""
echo "Next Steps:"
echo "  1. Review dashboard: open $DASHBOARD_FILE"
echo "  2. Verify MRB ≥ 0.340% (v1.4 baseline)"
echo "  3. If acceptable, proceed with Alpaca paper trading"
echo ""

# Open dashboard (macOS)
if command -v open &> /dev/null; then
    echo "Opening dashboard..."
    open "$DASHBOARD_FILE"
fi
