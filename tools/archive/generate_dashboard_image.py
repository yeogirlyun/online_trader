#!/usr/bin/env python3
"""
Generate Dashboard Image for Email
===================================

Creates a comprehensive dashboard image from trading session data.
Uses Plotly to generate charts and combines them into a single image.

Requirements:
    pip install plotly kaleido pandas

Usage:
    python3 generate_dashboard_image.py \
        --trades logs/mock_trading/trades_20251009_163724.jsonl \
        --signals logs/mock_trading/signals_20251009_163724.jsonl \
        --positions logs/mock_trading/positions_20251009_163724.jsonl \
        --decisions logs/mock_trading/decisions_20251009_163724.jsonl \
        --output /tmp/dashboard_preview.png
"""

import argparse
import json
import sys
from pathlib import Path
from datetime import datetime
import pandas as pd
import numpy as np

try:
    import plotly.graph_objects as go
    from plotly.subplots import make_subplots
except ImportError:
    print("❌ Plotly not installed. Install with: pip install plotly")
    sys.exit(1)


def load_jsonl(file_path):
    """Load JSONL file into list of dicts"""
    data = []
    if not Path(file_path).exists():
        return data

    with open(file_path, 'r') as f:
        for line in f:
            line = line.strip()
            if line:
                try:
                    data.append(json.loads(line))
                except:
                    pass
    return data


def calculate_metrics(positions):
    """Calculate trading metrics from positions data"""
    if not positions:
        return {
            'total_trades': 0,
            'final_equity': 100000.0,
            'total_return': 0.0,
            'max_drawdown': 0.0,
            'win_rate': 0.0,
            'sharpe_ratio': 0.0,
            'total_pnl': 0.0
        }

    # Extract equity curve
    equities = [p.get('equity', 100000.0) for p in positions]

    # Calculate metrics
    initial_equity = 100000.0
    final_equity = equities[-1] if equities else initial_equity
    total_return = ((final_equity - initial_equity) / initial_equity) * 100

    # Calculate drawdown
    peak = initial_equity
    max_dd = 0.0
    for eq in equities:
        if eq > peak:
            peak = eq
        dd = ((peak - eq) / peak) * 100
        max_dd = max(max_dd, dd)

    # Count trades with P&L
    trades_with_pnl = [p for p in positions if 'realized_pnl' in p and p.get('realized_pnl') != 0]
    total_trades = len(trades_with_pnl)

    wins = sum(1 for p in trades_with_pnl if p.get('realized_pnl', 0) > 0)
    win_rate = (wins / total_trades * 100) if total_trades > 0 else 0

    # Calculate Sharpe (simplified - assuming daily returns)
    returns = np.diff(equities) / equities[:-1] if len(equities) > 1 else [0]
    sharpe = (np.mean(returns) / np.std(returns) * np.sqrt(252)) if len(returns) > 0 and np.std(returns) > 0 else 0

    total_pnl = final_equity - initial_equity

    return {
        'total_trades': total_trades,
        'final_equity': final_equity,
        'total_return': total_return,
        'max_drawdown': max_dd,
        'win_rate': win_rate,
        'sharpe_ratio': sharpe,
        'total_pnl': total_pnl
    }


def create_comprehensive_dashboard(trades, signals, positions, decisions):
    """Create comprehensive dashboard with all charts"""

    metrics = calculate_metrics(positions)

    # Create subplots: 2 rows
    # Row 1: Metrics summary (text)
    # Row 2: Equity curve
    # Row 3: Position distribution

    fig = make_subplots(
        rows=3, cols=2,
        specs=[
            [{"type": "indicator"}, {"type": "indicator"}],
            [{"colspan": 2}, None],
            [{"type": "bar"}, {"type": "pie"}]
        ],
        subplot_titles=(
            '', '',
            'Equity Curve & Drawdown',
            'Trades by Symbol', 'Position Distribution'
        ),
        row_heights=[0.2, 0.5, 0.3],
        vertical_spacing=0.12,
        horizontal_spacing=0.1
    )

    # Row 1: Key Metrics (Indicators)
    fig.add_trace(
        go.Indicator(
            mode="number+delta",
            value=metrics['final_equity'],
            title={"text": "Final Equity"},
            delta={'reference': 100000, 'relative': False, 'valueformat': '.2f'},
            number={'prefix': "$", 'valueformat': ',.2f'},
            domain={'x': [0, 0.5], 'y': [0.8, 1]}
        ),
        row=1, col=1
    )

    fig.add_trace(
        go.Indicator(
            mode="number+delta",
            value=metrics['total_return'],
            title={"text": "Total Return"},
            delta={'reference': 0, 'relative': False},
            number={'suffix': "%", 'valueformat': '.3f'},
            domain={'x': [0.5, 1], 'y': [0.8, 1]}
        ),
        row=1, col=2
    )

    # Row 2: Equity Curve
    if positions:
        timestamps = [p.get('timestamp', '') for p in positions]
        equities = [p.get('equity', 100000.0) for p in positions]

        # Calculate running drawdown
        peak_equity = [100000.0]
        drawdowns = [0.0]
        for eq in equities[1:]:
            peak_equity.append(max(peak_equity[-1], eq))
            dd = ((peak_equity[-1] - eq) / peak_equity[-1]) * 100
            drawdowns.append(dd)

        fig.add_trace(
            go.Scatter(
                x=timestamps,
                y=equities,
                name='Equity',
                line=dict(color='#2ecc71', width=2),
                fill='tonexty',
                fillcolor='rgba(46, 204, 113, 0.1)'
            ),
            row=2, col=1
        )

        fig.add_trace(
            go.Scatter(
                x=timestamps,
                y=drawdowns,
                name='Drawdown %',
                line=dict(color='#e74c3c', width=1, dash='dot'),
                yaxis='y2'
            ),
            row=2, col=1
        )

    # Row 3: Trade Distribution by Symbol
    if trades:
        symbols = [t.get('symbol', 'UNKNOWN') for t in trades]
        symbol_counts = pd.Series(symbols).value_counts()

        fig.add_trace(
            go.Bar(
                x=symbol_counts.index,
                y=symbol_counts.values,
                marker_color=['#3498db', '#e74c3c', '#f39c12', '#9b59b6'],
                text=symbol_counts.values,
                textposition='auto'
            ),
            row=3, col=1
        )

    # Row 3: Position Type Distribution (from decisions)
    if decisions:
        states = [d.get('psm_state', 'UNKNOWN') for d in decisions]
        state_counts = pd.Series(states).value_counts()

        fig.add_trace(
            go.Pie(
                labels=state_counts.index,
                values=state_counts.values,
                marker=dict(colors=['#2ecc71', '#e74c3c', '#95a5a6', '#f39c12'])
            ),
            row=3, col=2
        )

    # Update layout
    fig.update_layout(
        title={
            'text': f"<b>Trading Session Dashboard</b><br>" \
                    f"<sub>Trades: {metrics['total_trades']} | " \
                    f"Win Rate: {metrics['win_rate']:.1f}% | " \
                    f"Max DD: {metrics['max_drawdown']:.2f}% | " \
                    f"Sharpe: {metrics['sharpe_ratio']:.2f}</sub>",
            'x': 0.5,
            'xanchor': 'center',
            'font': {'size': 24, 'color': '#2c3e50'}
        },
        height=1200,
        width=1600,
        showlegend=True,
        legend=dict(x=0.01, y=0.99),
        plot_bgcolor='white',
        paper_bgcolor='#f8f9fa',
        font=dict(family="Arial, sans-serif", size=12, color="#2c3e50")
    )

    # Update axes
    fig.update_xaxes(showgrid=True, gridcolor='#ecf0f1', row=2, col=1)
    fig.update_yaxes(showgrid=True, gridcolor='#ecf0f1', row=2, col=1)

    return fig, metrics


def main():
    parser = argparse.ArgumentParser(description='Generate dashboard image for email')
    parser.add_argument('--trades', required=True, help='Trades JSONL file')
    parser.add_argument('--signals', help='Signals JSONL file')
    parser.add_argument('--positions', help='Positions JSONL file')
    parser.add_argument('--decisions', help='Decisions JSONL file')
    parser.add_argument('--output', required=True, help='Output PNG file path')
    parser.add_argument('--width', type=int, default=1600, help='Image width')
    parser.add_argument('--height', type=int, default=1200, help='Image height')

    args = parser.parse_args()

    print(f"📊 Loading session data...")
    trades = load_jsonl(args.trades)
    signals = load_jsonl(args.signals) if args.signals else []
    positions = load_jsonl(args.positions) if args.positions else []
    decisions = load_jsonl(args.decisions) if args.decisions else []

    print(f"   Trades: {len(trades)}")
    print(f"   Signals: {len(signals)}")
    print(f"   Positions: {len(positions)}")
    print(f"   Decisions: {len(decisions)}")
    print()

    print("🎨 Creating dashboard...")
    fig, metrics = create_comprehensive_dashboard(trades, signals, positions, decisions)

    print("💾 Saving image...")
    try:
        fig.write_image(args.output, width=args.width, height=args.height, scale=2)
        print(f"✅ Dashboard image saved: {args.output}")
        print()
        print("📊 Session Metrics:")
        print(f"   Total Trades: {metrics['total_trades']}")
        print(f"   Final Equity: ${metrics['final_equity']:,.2f}")
        print(f"   Total Return: {metrics['total_return']:.3f}%")
        print(f"   Max Drawdown: {metrics['max_drawdown']:.2f}%")
        print(f"   Win Rate: {metrics['win_rate']:.1f}%")
        print(f"   Sharpe Ratio: {metrics['sharpe_ratio']:.2f}")
        return 0
    except Exception as e:
        print(f"❌ Failed to save image: {e}")
        print("   Install kaleido: pip install kaleido")
        return 1


if __name__ == '__main__':
    sys.exit(main())
