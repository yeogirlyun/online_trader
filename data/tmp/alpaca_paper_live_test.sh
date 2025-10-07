#!/bin/bash
# Alpaca Paper Live Test with MarS Real-Time Simulation
# ======================================================
#
# This script prepares for live paper trading by:
# 1. Using 2 blocks (2 days) of SPY historical data for warmup
# 2. Using MarS package to simulate real-time Alpaca quotes for 1 block
# 3. Running the OnlineEnsemble strategy with v1.4 optimizations
# 4. Generating dashboard for review
#
# Timeline:
# - Warmup: 2 blocks (960 bars, ~2 days)
# - Live test: 1 block (480 bars, ~1 day)
# - Total: 3 blocks (1440 bars)

set -e  # Exit on error

# Configuration
SYMBOL="SPY"
WARMUP_BLOCKS=2
TEST_BLOCKS=1
TOTAL_BLOCKS=$((WARMUP_BLOCKS + TEST_BLOCKS))
WARMUP_BARS=$((WARMUP_BLOCKS * 480))
TEST_BARS=$((TEST_BLOCKS * 480))
TOTAL_BARS=$((TOTAL_BLOCKS * 480))

# Directories
BASE_DIR="/Volumes/ExternalSSD/Dev/C++/online_trader"
DATA_DIR="$BASE_DIR/data"
EQUITIES_DIR="$DATA_DIR/equities"
TMP_DIR="$DATA_DIR/tmp"
DASHBOARD_DIR="$DATA_DIR/dashboards"
BUILD_DIR="$BASE_DIR/build"
QUOTE_SIM_DIR="$BASE_DIR/quote_simulation"

# Output files
TIMESTAMP=$(date +%Y%m%d_%H%M%S)
WARMUP_DATA="$TMP_DIR/spy_warmup_${WARMUP_BLOCKS}blocks.csv"
LIVE_DATA="$TMP_DIR/spy_live_test_${TEST_BLOCKS}block.csv"
COMBINED_DATA="$TMP_DIR/spy_warmup_plus_live_${TIMESTAMP}.csv"
SIGNALS_FILE="$TMP_DIR/spy_live_signals_${TIMESTAMP}.jsonl"
TRADES_FILE="$TMP_DIR/spy_live_trades_${TIMESTAMP}.jsonl"
TRADES_EQUITY_FILE="$TMP_DIR/spy_live_trades_equity_${TIMESTAMP}.csv"
DASHBOARD_FILE="$DASHBOARD_DIR/spy_live_test_${TIMESTAMP}.html"
LOG_FILE="$TMP_DIR/alpaca_live_test_${TIMESTAMP}.log"

# Logging function
log() {
    echo "[$(date '+%Y-%m-%d %H:%M:%S')] $1" | tee -a "$LOG_FILE"
}

log "=========================================="
log "Alpaca Paper Live Test with MarS"
log "=========================================="
log ""
log "Configuration:"
log "  Symbol: $SYMBOL"
log "  Warmup: $WARMUP_BLOCKS blocks ($WARMUP_BARS bars)"
log "  Live Test: $TEST_BLOCKS block ($TEST_BARS bars)"
log "  Total: $TOTAL_BLOCKS blocks ($TOTAL_BARS bars)"
log ""

# Step 1: Prepare warmup data (2 blocks from historical SPY data)
log "Step 1: Preparing warmup data..."

if [ -f "$EQUITIES_DIR/SPY_3days.csv" ]; then
    # Use SPY_3days.csv (contains ~3 blocks)
    # Extract first 2 blocks (960 bars + header)
    head -n $((WARMUP_BARS + 1)) "$EQUITIES_DIR/SPY_3days.csv" > "$WARMUP_DATA"
    log "✅ Extracted $WARMUP_BARS bars from SPY_3days.csv for warmup"
elif [ -f "$EQUITIES_DIR/SPY_4blocks.csv" ]; then
    # Use SPY_4blocks.csv
    # Extract first 2 blocks (960 bars + header)
    head -n $((WARMUP_BARS + 1)) "$EQUITIES_DIR/SPY_4blocks.csv" > "$WARMUP_DATA"
    log "✅ Extracted $WARMUP_BARS bars from SPY_4blocks.csv for warmup"
else
    log "❌ ERROR: No suitable SPY historical data found"
    log "   Please ensure SPY_3days.csv or SPY_4blocks.csv exists in $EQUITIES_DIR"
    exit 1
fi

# Verify warmup data
WARMUP_LINE_COUNT=$(wc -l < "$WARMUP_DATA")
log "Warmup data: $((WARMUP_LINE_COUNT - 1)) bars (expected: $WARMUP_BARS)"

# Step 2: Generate MarS real-time simulation data (1 block)
log ""
log "Step 2: Generating MarS real-time simulation data for live test..."

# Check if MarS is available
cd "$QUOTE_SIM_DIR"

# Generate 1 block (480 bars = 480 minutes = 8 hours trading day)
log "Running MarS simulation for 480 minutes (1 trading day)..."

python3 tools/online_quote_simulator.py \
    --symbol "$SYMBOL" \
    --duration 480 \
    --interval 60000 \
    --regime normal \
    --mars \
    --output "$TMP_DIR/mars_live_quotes_${TIMESTAMP}.json" \
    --format json 2>&1 | tee -a "$LOG_FILE"

if [ $? -eq 0 ]; then
    log "✅ MarS simulation completed"
else
    log "⚠️  MarS simulation failed, falling back to basic simulation"
    # Fallback: Use basic simulation without MarS
    python3 tools/online_quote_simulator.py \
        --symbol "$SYMBOL" \
        --duration 480 \
        --interval 60000 \
        --regime normal \
        --output "$TMP_DIR/mars_live_quotes_${TIMESTAMP}.json" \
        --format json 2>&1 | tee -a "$LOG_FILE"
fi

# Convert JSON quotes to CSV bars
log "Converting MarS quotes to CSV format..."

python3 << 'PYTHON_SCRIPT'
import json
import csv
import sys
from datetime import datetime

# Read JSON quotes
with open("$TMP_DIR/mars_live_quotes_${TIMESTAMP}.json", "r") as f:
    quotes = json.load(f)

# Group quotes into 1-minute bars
bars = []
current_bar = None
bar_quotes = []

for quote in quotes:
    timestamp = datetime.fromtimestamp(quote['timestamp'])
    minute_key = timestamp.replace(second=0, microsecond=0)

    if current_bar != minute_key:
        if bar_quotes:
            # Create OHLCV bar from quotes
            open_price = bar_quotes[0]['last_price']
            close_price = bar_quotes[-1]['last_price']
            high_price = max(q['last_price'] for q in bar_quotes)
            low_price = min(q['last_price'] for q in bar_quotes)
            volume = sum(q['volume'] for q in bar_quotes) // len(bar_quotes)

            bars.append({
                'timestamp': int(minute_key.timestamp()),
                'open': open_price,
                'high': high_price,
                'low': low_price,
                'close': close_price,
                'volume': volume
            })

        current_bar = minute_key
        bar_quotes = [quote]
    else:
        bar_quotes.append(quote)

# Add last bar
if bar_quotes:
    open_price = bar_quotes[0]['last_price']
    close_price = bar_quotes[-1]['last_price']
    high_price = max(q['last_price'] for q in bar_quotes)
    low_price = min(q['last_price'] for q in bar_quotes)
    volume = sum(q['volume'] for q in bar_quotes) // len(bar_quotes)

    bars.append({
        'timestamp': int(current_bar.timestamp()),
        'open': open_price,
        'high': high_price,
        'low': low_price,
        'close': close_price,
        'volume': volume
    })

# Write to CSV
with open("$LIVE_DATA", "w", newline='') as f:
    writer = csv.DictWriter(f, fieldnames=['timestamp', 'open', 'high', 'low', 'close', 'volume'])
    writer.writeheader()
    writer.writerows(bars)

print(f"✅ Converted {len(quotes)} quotes to {len(bars)} bars")
PYTHON_SCRIPT

log "✅ Generated $TEST_BARS bars for live test simulation"

# Step 3: Combine warmup + live data
log ""
log "Step 3: Combining warmup and live data..."

# Combine: warmup (with header) + live (without header)
cat "$WARMUP_DATA" > "$COMBINED_DATA"
tail -n +2 "$LIVE_DATA" >> "$COMBINED_DATA"

COMBINED_LINE_COUNT=$(wc -l < "$COMBINED_DATA")
log "✅ Combined data: $((COMBINED_LINE_COUNT - 1)) bars (expected: $TOTAL_BARS)"

# Step 4: Generate signals (with warmup)
log ""
log "Step 4: Generating signals with $WARMUP_BLOCKS-block warmup..."

cd "$BUILD_DIR"

time ./sentio_cli generate-signals \
    --data "$COMBINED_DATA" \
    --output "$SIGNALS_FILE" \
    --warmup $WARMUP_BARS \
    --verbose 2>&1 | tee -a "$LOG_FILE"

if [ $? -eq 0 ]; then
    SIGNAL_COUNT=$(wc -l < "$SIGNALS_FILE")
    log "✅ Generated $SIGNAL_COUNT signals"
else
    log "❌ Signal generation failed"
    exit 1
fi

# Step 5: Execute trades
log ""
log "Step 5: Executing trades..."

./sentio_cli execute-trades \
    --signals "$SIGNALS_FILE" \
    --data "$COMBINED_DATA" \
    --output "$TRADES_FILE" \
    --warmup $WARMUP_BARS \
    --verbose 2>&1 | tee -a "$LOG_FILE"

if [ $? -eq 0 ]; then
    TRADE_COUNT=$(wc -l < "$TRADES_FILE")
    log "✅ Executed $TRADE_COUNT trades"
else
    log "❌ Trade execution failed"
    exit 1
fi

# Step 6: Analyze trades
log ""
log "Step 6: Analyzing trades..."

./sentio_cli analyze-trades \
    --trades "$TRADES_FILE" \
    --output "$TRADES_EQUITY_FILE" \
    --verbose 2>&1 | tee -a "$LOG_FILE"

if [ $? -eq 0 ]; then
    log "✅ Trade analysis completed"
else
    log "❌ Trade analysis failed"
    exit 1
fi

# Step 7: Generate dashboard
log ""
log "Step 7: Generating dashboard..."

python3 << 'PYTHON_DASHBOARD'
import json
import pandas as pd
import sys
from datetime import datetime

# Read trades
trades = []
with open("$TRADES_FILE", "r") as f:
    for line in f:
        trades.append(json.loads(line))

# Read equity curve
equity_df = pd.read_csv("$TRADES_EQUITY_FILE")

# Calculate performance metrics
if trades:
    total_return = trades[-1].get('total_return', 0.0) * 100
    num_trades = len([t for t in trades if t.get('action') in ['BUY', 'SELL', 'SHORT', 'COVER']])

    # Calculate MRB (mean return per block)
    test_trades = [t for t in trades if t.get('bar_index', 0) >= $WARMUP_BARS]
    if test_trades:
        final_return = test_trades[-1].get('total_return', 0.0) * 100
        mrb = final_return / $TEST_BLOCKS
    else:
        mrb = 0.0

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
            sharpe = (returns.mean() / returns.std()) * (252 ** 0.5)
        else:
            sharpe = 0.0
    else:
        sharpe = 0.0
else:
    total_return = 0.0
    num_trades = 0
    mrb = 0.0
    win_rate = 0.0
    sharpe = 0.0

# Generate HTML dashboard
html = f"""
<!DOCTYPE html>
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
        .metric-value.positive {{
            color: #27ae60;
        }}
        .metric-value.negative {{
            color: #e74c3c;
        }}
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
        .info-section h3 {{
            margin-top: 0;
            color: #2c3e50;
        }}
        .badge {{
            display: inline-block;
            padding: 5px 10px;
            border-radius: 4px;
            font-size: 12px;
            font-weight: bold;
            margin-right: 10px;
        }}
        .badge-warmup {{
            background-color: #3498db;
            color: white;
        }}
        .badge-live {{
            background-color: #e67e22;
            color: white;
        }}
        .badge-mars {{
            background-color: #9b59b6;
            color: white;
        }}
        .footer {{
            text-align: center;
            color: #7f8c8d;
            padding: 20px;
            font-size: 12px;
        }}
    </style>
</head>
<body>
    <div class="header">
        <h1>🚀 Alpaca Paper Live Test - SPY</h1>
        <p>Generated: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}</p>
        <div>
            <span class="badge badge-warmup">Warmup: {$WARMUP_BLOCKS} blocks ({$WARMUP_BARS} bars)</span>
            <span class="badge badge-live">Live Test: {$TEST_BLOCKS} block ({$TEST_BARS} bars)</span>
            <span class="badge badge-mars">MarS Real-Time Simulation</span>
        </div>
    </div>

    <div class="info-section">
        <h3>📊 Test Configuration</h3>
        <p><strong>Strategy:</strong> OnlineEnsemble v1.4 (Multi-Horizon EWRLS)</p>
        <p><strong>Warmup Period:</strong> {$WARMUP_BLOCKS} blocks (2 days) - Uses historical SPY data</p>
        <p><strong>Live Test Period:</strong> {$TEST_BLOCKS} block (1 day) - Uses MarS real-time simulation</p>
        <p><strong>Data Source:</strong> MarS (Market Simulation Engine) - Realistic market behavior</p>
        <p><strong>Version:</strong> v1.4 Production Baseline (0.345% MRB target)</p>
    </div>

    <div class="metrics">
        <div class="metric-card">
            <div class="metric-label">Total Return</div>
            <div class="metric-value {'positive' if total_return >= 0 else 'negative'}">
                {total_return:+.2f}%
            </div>
        </div>
        <div class="metric-card">
            <div class="metric-label">MRB (Mean Return per Block)</div>
            <div class="metric-value {'positive' if mrb >= 0 else 'negative'}">
                {mrb:+.3f}%
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
    </div>

    <div class="chart-container">
        <h3>📈 Equity Curve</h3>
        <div id="equity-chart"></div>
    </div>

    <div class="chart-container">
        <h3>📊 Trade Distribution</h3>
        <div id="trade-chart"></div>
    </div>

    <div class="footer">
        <p>🤖 Generated with OnlineEnsemble v1.4 (Production Baseline)</p>
        <p>MarS Integration: Real-time quote simulation for Alpaca paper trading</p>
    </div>

    <script>
        // Equity curve
        var equityData = {{
            x: {equity_df['bar_index'].tolist()},
            y: {equity_df['equity'].tolist()},
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
                x0: {$WARMUP_BARS},
                x1: {$WARMUP_BARS},
                y0: 0,
                y1: 1,
                yref: 'paper',
                line: {{
                    color: '#e67e22',
                    width: 2,
                    dash: 'dash'
                }}
            }}],
            annotations: [{{
                x: {$WARMUP_BARS},
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
        var tradePnls = [];

        {json.dumps(trades)}.forEach(function(trade) {{
            if (trade.action && trade.action !== 'HOLD') {{
                tradeTypes[trade.action] = (tradeTypes[trade.action] || 0) + 1;
            }}
            if (trade.pnl !== undefined && trade.pnl !== 0) {{
                tradePnls.push(trade.pnl);
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
with open("$DASHBOARD_FILE", "w") as f:
    f.write(html)

print(f"✅ Dashboard generated: $DASHBOARD_FILE")
print(f"")
print(f"Performance Summary:")
print(f"  Total Return: {total_return:+.2f}%")
print(f"  MRB: {mrb:+.3f}% (target: ≥0.340%)")
print(f"  Trades: {num_trades}")
print(f"  Win Rate: {win_rate:.1f}%")
print(f"  Sharpe: {sharpe:.2f}")
PYTHON_DASHBOARD

# Step 8: Summary
log ""
log "=========================================="
log "Alpaca Paper Live Test Completed!"
log "=========================================="
log ""
log "Output Files:"
log "  Signals: $SIGNALS_FILE"
log "  Trades: $TRADES_FILE"
log "  Equity Curve: $TRADES_EQUITY_FILE"
log "  Dashboard: $DASHBOARD_FILE"
log "  Log: $LOG_FILE"
log ""
log "Next Steps:"
log "  1. Review dashboard: open $DASHBOARD_FILE"
log "  2. Verify MRB ≥ 0.340% (v1.4 target)"
log "  3. If MRB acceptable, proceed with Alpaca paper trading"
log "  4. If MRB < 0.340%, investigate and retest"
log ""
log "Ready for Alpaca paper trading at market open!"

# Open dashboard automatically (macOS)
if command -v open &> /dev/null; then
    log "Opening dashboard..."
    open "$DASHBOARD_FILE"
fi
