#!/usr/bin/env python3
"""
Trade Dashboard Generator
=========================

Reads trade JSONL file and SPY data to generate comprehensive HTML dashboard.

Usage:
    python3 generate_trade_dashboard.py \
        --trades /path/to/trades.jsonl \
        --data /path/to/SPY_data.csv \
        --output /path/to/dashboard.html \
        --warmup 960

Features:
    - SPY price chart with BUY/SELL markers
    - Portfolio value & cumulative P&L chart
    - Complete trade statement table
    - Performance metrics
    - All data embedded in single HTML file
"""

import json
import csv
import argparse
from pathlib import Path
from datetime import datetime


def load_trades(trades_file):
    """Load trades from JSONL file"""
    trades = []
    with open(trades_file, 'r') as f:
        for line in f:
            try:
                trades.append(json.loads(line))
            except:
                pass
    return trades


def load_spy_data(data_file):
    """Load SPY price data from CSV"""
    spy_data = []
    with open(data_file, 'r') as f:
        reader = csv.DictReader(f)
        for idx, row in enumerate(reader):
            spy_data.append({
                'bar_index': idx,
                'close': float(row['close']),
                'timestamp_ms': int(row.get('ts_nyt_epoch', 0)) * 1000 if row.get('ts_nyt_epoch') else 0
            })
    return spy_data


def calculate_metrics(trades, warmup_bars, test_blocks):
    """Calculate performance metrics"""
    test_trades = [t for t in trades if t['bar_index'] >= warmup_bars]

    if not test_trades:
        return None

    starting_capital = 100000.0
    final_value = test_trades[-1]['portfolio_value']
    total_return = (final_value / starting_capital - 1) * 100
    mrb = total_return / test_blocks if test_blocks > 0 else total_return

    # Calculate max drawdown
    peak = starting_capital
    max_dd = 0
    for trade in test_trades:
        pv = trade['portfolio_value']
        if pv > peak:
            peak = pv
        dd = (peak - pv) / peak * 100
        if dd > max_dd:
            max_dd = dd

    # Calculate win rate
    positive_moves = 0
    negative_moves = 0
    for i in range(1, len(test_trades)):
        change = test_trades[i]['portfolio_value'] - test_trades[i-1]['portfolio_value']
        if change > 0:
            positive_moves += 1
        elif change < 0:
            negative_moves += 1

    win_rate = positive_moves / (positive_moves + negative_moves) * 100 if (positive_moves + negative_moves) > 0 else 0

    return {
        'starting_capital': starting_capital,
        'final_value': final_value,
        'total_return': total_return,
        'mrb': mrb,
        'num_trades': len(test_trades),
        'win_rate': win_rate,
        'max_drawdown': max_dd
    }


def generate_html(trades, spy_data, metrics, warmup_bars, output_file, title="Trade Dashboard"):
    """Generate complete HTML dashboard with embedded data"""

    # Filter to test period
    test_trades = [t for t in trades if t['bar_index'] >= warmup_bars]
    test_spy = [s for s in spy_data if s['bar_index'] >= warmup_bars]

    # Prepare data for embedding
    spy_json = json.dumps(test_spy)
    trades_json = json.dumps(test_trades)

    html_content = f"""<!DOCTYPE html>
<html>
<head>
    <title>{title}</title>
    <script src="https://cdn.plot.ly/plotly-latest.min.js"></script>
    <style>
        body {{
            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
            margin: 20px;
            background-color: #f5f5f5;
        }}
        .header {{
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            color: white;
            padding: 30px;
            border-radius: 10px;
            margin-bottom: 20px;
            box-shadow: 0 4px 6px rgba(0,0,0,0.1);
        }}
        .header h1 {{ margin: 0 0 10px 0; font-size: 32px; }}
        .status {{
            background-color: #10b981;
            color: white;
            padding: 20px;
            border-radius: 10px;
            font-size: 20px;
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
            padding: 25px;
            border-radius: 10px;
            box-shadow: 0 2px 4px rgba(0,0,0,0.1);
        }}
        .metric-label {{
            font-size: 14px;
            color: #6b7280;
            margin-bottom: 8px;
            text-transform: uppercase;
        }}
        .metric-value {{
            font-size: 32px;
            font-weight: bold;
            color: #1f2937;
        }}
        .metric-value.positive {{ color: #10b981; }}
        .metric-value.negative {{ color: #ef4444; }}
        .chart-container {{
            background-color: white;
            padding: 25px;
            border-radius: 10px;
            box-shadow: 0 2px 4px rgba(0,0,0,0.1);
            margin-bottom: 20px;
            overflow-x: auto;
        }}
        .chart-container h3 {{ margin-top: 0; color: #1f2937; }}
        .chart-wrapper {{
            min-width: 800px;
            height: 800px;
        }}
        .trade-table {{
            background-color: white;
            padding: 25px;
            border-radius: 10px;
            box-shadow: 0 2px 4px rgba(0,0,0,0.1);
            margin-bottom: 20px;
            overflow-x: auto;
        }}
        table {{
            width: 100%;
            border-collapse: collapse;
            font-size: 12px;
        }}
        th {{
            background-color: #f3f4f6;
            padding: 10px;
            text-align: left;
            font-weight: 600;
            position: sticky;
            top: 0;
            z-index: 10;
        }}
        td {{
            padding: 8px 10px;
            border-bottom: 1px solid #e5e7eb;
        }}
        tr:hover {{ background-color: #f9fafb; }}
        .buy {{ color: #10b981; font-weight: bold; }}
        .sell {{ color: #ef4444; font-weight: bold; }}
        .positive-pnl {{ color: #10b981; }}
        .negative-pnl {{ color: #ef4444; }}
        .table-wrapper {{
            max-height: 600px;
            overflow-y: auto;
        }}
    </style>
</head>
<body>
    <div class="header">
        <h1>📊 {title}</h1>
        <p><strong>Generated:</strong> {datetime.now().strftime('%Y-%m-%d %H:%M:%S ET')}</p>
        <p><strong>Test Period:</strong> Bars {warmup_bars}+</p>
    </div>

    <div class="status">
        ✅ MRB: {metrics['mrb']:.4f}% | Total Return: {metrics['total_return']:+.2f}% | Trades: {metrics['num_trades']}
    </div>

    <div class="metrics">
        <div class="metric-card">
            <div class="metric-label">Starting Capital</div>
            <div class="metric-value">${metrics['starting_capital']:,.0f}</div>
        </div>
        <div class="metric-card">
            <div class="metric-label">Final Value</div>
            <div class="metric-value positive">${metrics['final_value']:,.0f}</div>
        </div>
        <div class="metric-card">
            <div class="metric-label">Total P&L</div>
            <div class="metric-value {'positive' if metrics['total_return'] > 0 else 'negative'}">${metrics['final_value'] - metrics['starting_capital']:+,.0f}</div>
        </div>
        <div class="metric-card">
            <div class="metric-label">MRB</div>
            <div class="metric-value positive">{metrics['mrb']:.4f}%</div>
        </div>
        <div class="metric-card">
            <div class="metric-label">Win Rate</div>
            <div class="metric-value positive">{metrics['win_rate']:.1f}%</div>
        </div>
        <div class="metric-card">
            <div class="metric-label">Max Drawdown</div>
            <div class="metric-value negative">-{metrics['max_drawdown']:.2f}%</div>
        </div>
    </div>

    <div class="trade-table">
        <h3>🏁 End of Day Summary</h3>
        <table style="width: auto; margin: 0;">
            <tr>
                <th style="text-align: left; padding: 10px 20px;">Final Cash</th>
                <td style="padding: 10px 20px; font-weight: bold;">${test_trades[-1]['cash_balance']:,.2f}</td>
            </tr>
            <tr>
                <th style="text-align: left; padding: 10px 20px;">Final Portfolio Value</th>
                <td style="padding: 10px 20px; font-weight: bold; color: {'#10b981' if metrics['total_return'] >= 0 else '#ef4444'};">${metrics['final_value']:,.2f}</td>
            </tr>
            <tr>
                <th style="text-align: left; padding: 10px 20px;">Total Return</th>
                <td style="padding: 10px 20px; font-weight: bold; color: {'#10b981' if metrics['total_return'] >= 0 else '#ef4444'};">${metrics['final_value'] - metrics['starting_capital']:+,.2f} ({metrics['total_return']:+.2f}%)</td>
            </tr>
        </table>
    </div>

    <div class="chart-container">
        <h3>📈 SPY Price Chart with Trades & Portfolio Performance</h3>
        <div class="chart-wrapper" id="combined-chart"></div>
    </div>

    <div class="trade-table">
        <h3>📋 Trade Statement ({metrics['num_trades']} Trades)</h3>
        <div class="table-wrapper">
            <table id="trade-statement">
                <thead>
                    <tr>
                        <th>#</th>
                        <th>Bar</th>
                        <th>Time</th>
                        <th>Symbol</th>
                        <th>Action</th>
                        <th>Qty</th>
                        <th>Price</th>
                        <th>Value</th>
                        <th>Cash</th>
                        <th>Portfolio</th>
                        <th>Trade P&L</th>
                        <th>Cum P&L</th>
                        <th>Reason</th>
                    </tr>
                </thead>
                <tbody id="trade-tbody"></tbody>
            </table>
        </div>
    </div>

    <script>
        const SPY_DATA = {spy_json};
        const TRADES = {trades_json};
        const STARTING_CAPITAL = {metrics['starting_capital']};

        // Prepare data
        const spyPriceMap = {{}};
        SPY_DATA.forEach(s => {{ spyPriceMap[s.bar_index] = s.close; }});

        const buyTrades = TRADES.filter(t => t.action === 'BUY');
        const sellTrades = TRADES.filter(t => t.action === 'SELL');

        // Create subplots with shared x-axis and single range slider
        const priceTrace = {{
            x: SPY_DATA.map(s => s.bar_index),
            y: SPY_DATA.map(s => s.close),
            type: 'scatter',
            mode: 'lines',
            name: 'SPY Price',
            xaxis: 'x',
            yaxis: 'y',
            line: {{color: '#6366f1', width: 2}},
            hovertemplate: '<b>Bar %{{x}}</b><br>Price: $%{{y:.2f}}<extra></extra>'
        }};

        const buyTrace = {{
            x: buyTrades.map(t => t.bar_index),
            y: buyTrades.map(t => spyPriceMap[t.bar_index] || 670),
            type: 'scatter',
            mode: 'markers',
            name: 'BUY',
            xaxis: 'x',
            yaxis: 'y',
            marker: {{
                color: '#10b981',
                size: 10,
                symbol: 'triangle-up',
                line: {{color: '#065f46', width: 1}}
            }},
            text: buyTrades.map(t => `${{t.symbol}} x${{t.quantity.toFixed(2)}} @ $${{t.price.toFixed(2)}}<br>${{t.reason}}`),
            hovertemplate: '<b>BUY at Bar %{{x}}</b><br>%{{text}}<extra></extra>'
        }};

        const sellTrace = {{
            x: sellTrades.map(t => t.bar_index),
            y: sellTrades.map(t => spyPriceMap[t.bar_index] || 670),
            type: 'scatter',
            mode: 'markers',
            name: 'SELL',
            xaxis: 'x',
            yaxis: 'y',
            marker: {{
                color: '#ef4444',
                size: 10,
                symbol: 'triangle-down',
                line: {{color: '#991b1b', width: 1}}
            }},
            text: sellTrades.map(t => `${{t.symbol}} x${{t.quantity.toFixed(2)}} @ $${{t.price.toFixed(2)}}<br>${{t.reason}}`),
            hovertemplate: '<b>SELL at Bar %{{x}}</b><br>%{{text}}<extra></extra>'
        }};

        const portfolioTrace = {{
            x: TRADES.map(t => t.bar_index),
            y: TRADES.map(t => t.portfolio_value),
            type: 'scatter',
            mode: 'lines',
            name: 'Portfolio Value',
            xaxis: 'x2',
            yaxis: 'y2',
            line: {{color: '#667eea', width: 3}},
            hovertemplate: '<b>Bar %{{x}}</b><br>Portfolio: $%{{y:,.2f}}<extra></extra>'
        }};

        const pnlTrace = {{
            x: TRADES.map(t => t.bar_index),
            y: TRADES.map(t => t.portfolio_value - STARTING_CAPITAL),
            type: 'scatter',
            mode: 'lines',
            name: 'Cumulative P&L',
            xaxis: 'x2',
            yaxis: 'y3',
            line: {{color: '#10b981', width: 2}},
            fill: 'tozeroy',
            fillcolor: 'rgba(16, 185, 129, 0.1)',
            hovertemplate: '<b>Bar %{{x}}</b><br>P&L: $%{{y:,.2f}}<extra></extra>'
        }};

        const layout = {{
            grid: {{rows: 2, columns: 1, pattern: 'independent', roworder: 'top to bottom'}},

            // Top subplot - Price chart
            xaxis: {{
                title: '',
                showgrid: true,
                gridcolor: '#e5e7eb',
                domain: [0, 1],
                anchor: 'y'
            }},
            yaxis: {{
                title: 'SPY Price ($)',
                showgrid: true,
                gridcolor: '#e5e7eb',
                domain: [0.55, 1]
            }},

            // Bottom subplot - Portfolio & P&L
            xaxis2: {{
                title: 'Bar Index',
                showgrid: true,
                gridcolor: '#e5e7eb',
                domain: [0, 1],
                anchor: 'y2',
                rangeslider: {{visible: true, thickness: 0.05}},
                matches: 'x'
            }},
            yaxis2: {{
                title: 'Portfolio Value ($)',
                showgrid: true,
                gridcolor: '#e5e7eb',
                domain: [0, 0.45],
                side: 'left'
            }},
            yaxis3: {{
                title: 'Cumulative P&L ($)',
                overlaying: 'y2',
                side: 'right',
                showgrid: false
            }},

            hovermode: 'x unified',
            plot_bgcolor: '#ffffff',
            paper_bgcolor: '#ffffff',
            showlegend: true,
            legend: {{x: 0, y: 1.05, orientation: 'h'}},
            height: 800
        }};

        Plotly.newPlot('combined-chart', [priceTrace, buyTrace, sellTrace, portfolioTrace, pnlTrace], layout, {{responsive: true, scrollZoom: true}});

        // Trade Table
        const tbody = document.getElementById('trade-tbody');
        TRADES.forEach((trade, idx) => {{
            const pnl = idx > 0 ? trade.portfolio_value - TRADES[idx-1].portfolio_value : 0;
            const cumPnl = trade.portfolio_value - STARTING_CAPITAL;

            const timestamp = new Date(trade.timestamp_ms);
            const timeStr = timestamp.toLocaleString('en-US', {{
                month: 'short',
                day: 'numeric',
                hour: '2-digit',
                minute: '2-digit'
            }});

            const row = document.createElement('tr');
            row.innerHTML = `
                <td>${{idx + 1}}</td>
                <td>${{trade.bar_index}}</td>
                <td>${{timeStr}}</td>
                <td><strong>${{trade.symbol}}</strong></td>
                <td class="${{trade.action.toLowerCase()}}">${{trade.action}}</td>
                <td>${{trade.quantity.toFixed(2)}}</td>
                <td>${{trade.price.toFixed(2)}}</td>
                <td>${{trade.trade_value.toFixed(2)}}</td>
                <td>${{trade.cash_balance.toFixed(2)}}</td>
                <td>${{trade.portfolio_value.toFixed(2)}}</td>
                <td class="${{pnl >= 0 ? 'positive-pnl' : 'negative-pnl'}}">${{pnl.toFixed(2)}}</td>
                <td class="${{cumPnl >= 0 ? 'positive-pnl' : 'negative-pnl'}}">${{cumPnl.toFixed(2)}}</td>
                <td style="font-size: 10px;">${{trade.reason}}</td>
            `;
            tbody.appendChild(row);
        }});
    </script>
</body>
</html>
"""

    with open(output_file, 'w') as f:
        f.write(html_content)

    print(f"✅ Dashboard generated: {output_file}")
    print(f"   - {len(test_spy)} price bars")
    print(f"   - {len(test_trades)} trades")
    print(f"   - MRB: {metrics['mrb']:.4f}%")


def main():
    parser = argparse.ArgumentParser(description='Generate trade dashboard from JSONL trade file')
    parser.add_argument('--trades', required=True, help='Path to trades JSONL file')
    parser.add_argument('--data', required=True, help='Path to SPY CSV data file')
    parser.add_argument('--output', required=True, help='Output HTML file path')
    parser.add_argument('--warmup', type=int, default=960, help='Warmup bars (default: 960)')
    parser.add_argument('--test-blocks', type=int, default=2, help='Number of test blocks for MRB calculation')
    parser.add_argument('--title', default='Trade Dashboard', help='Dashboard title')

    args = parser.parse_args()

    # Load data
    print(f"Loading trades from: {args.trades}")
    trades = load_trades(args.trades)
    print(f"✅ Loaded {len(trades)} trades")

    print(f"Loading SPY data from: {args.data}")
    spy_data = load_spy_data(args.data)
    print(f"✅ Loaded {len(spy_data)} bars")

    # Calculate metrics
    print("Calculating metrics...")
    metrics = calculate_metrics(trades, args.warmup, args.test_blocks)

    if not metrics:
        print("❌ ERROR: No test period trades found")
        return 1

    # Generate HTML
    print("Generating dashboard...")
    generate_html(trades, spy_data, metrics, args.warmup, args.output, args.title)

    return 0


if __name__ == '__main__':
    import sys
    sys.exit(main())
