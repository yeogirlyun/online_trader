#!/usr/bin/env python3
"""
Rotation Trading Multi-Symbol Dashboard
=======================================

Professional dashboard for multi-symbol rotation trading system.
Shows all 6 instruments with:
- Price charts with entry/exit markers
- Complete trade statement tables
- Per-symbol performance metrics
- Combined portfolio summary

Usage:
    python rotation_trading_dashboard.py \
        --trades logs/rotation_trading/trades.jsonl \
        --output data/dashboards/rotation_report.html
"""

import argparse
import json
import sys
from datetime import datetime
from collections import defaultdict
import pandas as pd
import numpy as np

try:
    import plotly.graph_objects as go
    from plotly.subplots import make_subplots
    import plotly.express as px
except ImportError:
    print("❌ Plotly not installed. Install with: pip install plotly pandas")
    sys.exit(1)


class RotationTradingDashboard:
    """Multi-symbol rotation trading dashboard"""

    def __init__(self, trades_path, signals_path=None, positions_path=None,
                 decisions_path=None, start_equity=100000.0, data_dir='data/equities'):
        self.trades_path = trades_path
        self.signals_path = signals_path
        self.positions_path = positions_path
        self.decisions_path = decisions_path
        self.start_equity = start_equity
        self.data_dir = data_dir

        # All expected symbols (rotation strategy uses 12 instruments - removed gold miners NUGT/DUST)
        self.all_symbols = ['ERX', 'ERY', 'FAS', 'FAZ', 'SDS', 'SSO', 'SQQQ', 'SVXY', 'TNA', 'TQQQ', 'TZA', 'UVXY']

        # Data structures
        self.trades = []
        self.trades_by_symbol = defaultdict(list)
        self.equity_by_symbol = defaultdict(list)
        self.portfolio_equity = []

        # Performance metrics
        self.symbol_metrics = {}
        self.portfolio_metrics = {}

    def load_data(self):
        """Load all trade data"""
        print(f"📊 Loading rotation trading data...")
        print(f"   Trades: {self.trades_path}")

        # Load trades
        with open(self.trades_path, 'r') as f:
            for line in f:
                if line.strip():
                    trade = json.loads(line)
                    self.trades.append(trade)
                    symbol = trade.get('symbol', 'UNKNOWN')
                    self.trades_by_symbol[symbol].append(trade)

        print(f"✓ Loaded {len(self.trades)} total trades")
        for symbol, symbol_trades in sorted(self.trades_by_symbol.items()):
            print(f"   {symbol}: {len(symbol_trades)} trades")

    def calculate_metrics(self):
        """Calculate performance metrics for each symbol and portfolio"""
        print(f"\n📊 Calculating performance metrics...")

        # Calculate per-symbol metrics for ALL symbols (including those with 0 trades)
        for symbol in self.all_symbols:
            symbol_trades = self.trades_by_symbol.get(symbol, [])
            self.symbol_metrics[symbol] = self._calculate_symbol_metrics(symbol, symbol_trades)

        # Calculate portfolio-level metrics
        self.portfolio_metrics = self._calculate_portfolio_metrics()

        print(f"✓ Metrics calculated")

    def _calculate_symbol_metrics(self, symbol, symbol_trades):
        """Calculate performance metrics for a single symbol"""
        metrics = {
            'symbol': symbol,
            'total_trades': len(symbol_trades),
            'entries': sum(1 for t in symbol_trades if t.get('action') == 'ENTRY'),
            'exits': sum(1 for t in symbol_trades if t.get('action') == 'EXIT'),
            'total_pnl': 0.0,
            'total_pnl_pct': 0.0,
            'wins': 0,
            'losses': 0,
            'avg_win': 0.0,
            'avg_loss': 0.0,
            'largest_win': 0.0,
            'largest_loss': 0.0,
            'win_rate': 0.0,
            'avg_bars_held': 0.0,
        }

        # Calculate from EXIT trades (which have P&L)
        exit_trades = [t for t in symbol_trades if t.get('action') == 'EXIT']

        if not exit_trades:
            return metrics

        wins = []
        losses = []
        bars_held = []

        for trade in exit_trades:
            pnl = trade.get('pnl', 0.0)
            if pnl is not None:
                metrics['total_pnl'] += pnl

                if pnl > 0:
                    wins.append(pnl)
                    metrics['wins'] += 1
                elif pnl < 0:
                    losses.append(pnl)
                    metrics['losses'] += 1

                if pnl > metrics['largest_win']:
                    metrics['largest_win'] = pnl
                if pnl < metrics['largest_loss']:
                    metrics['largest_loss'] = pnl

            pnl_pct = trade.get('pnl_pct', 0.0)
            if pnl_pct is not None:
                metrics['total_pnl_pct'] += pnl_pct

            bars = trade.get('bars_held', 0)
            if bars > 0:
                bars_held.append(bars)

        # Calculate averages
        if wins:
            metrics['avg_win'] = np.mean(wins)
        if losses:
            metrics['avg_loss'] = np.mean(losses)
        if bars_held:
            metrics['avg_bars_held'] = np.mean(bars_held)

        # Win rate
        total_closed = metrics['wins'] + metrics['losses']
        if total_closed > 0:
            metrics['win_rate'] = (metrics['wins'] / total_closed) * 100

        return metrics

    def _calculate_portfolio_metrics(self):
        """Calculate portfolio-level metrics"""
        # Sum metrics across all symbols
        metrics = {
            'total_trades': len(self.trades),
            'symbols_traded': len(self.trades_by_symbol),
            'total_pnl': 0.0,
            'total_pnl_pct': 0.0,
            'total_wins': 0,
            'total_losses': 0,
            'portfolio_win_rate': 0.0,
            'final_equity': self.start_equity,
            'return_pct': 0.0,
        }

        for symbol, sym_metrics in self.symbol_metrics.items():
            metrics['total_pnl'] += sym_metrics['total_pnl']
            metrics['total_pnl_pct'] += sym_metrics['total_pnl_pct']
            metrics['total_wins'] += sym_metrics['wins']
            metrics['total_losses'] += sym_metrics['losses']

        # Final equity and return
        metrics['final_equity'] = self.start_equity + metrics['total_pnl']
        metrics['return_pct'] = (metrics['total_pnl'] / self.start_equity) * 100

        # Win rate
        total_closed = metrics['total_wins'] + metrics['total_losses']
        if total_closed > 0:
            metrics['portfolio_win_rate'] = (metrics['total_wins'] / total_closed) * 100

        return metrics

    def _load_price_data(self, symbol, target_date):
        """Load historical price data for a symbol on a specific date"""
        import os
        csv_path = os.path.join(self.data_dir, f'{symbol}_RTH_NH.csv')

        if not os.path.exists(csv_path):
            print(f"⚠️  Price data not found for {symbol}: {csv_path}")
            return [], []

        try:
            df = pd.read_csv(csv_path)
            # Filter for target date
            df['date'] = pd.to_datetime(df['ts_utc']).dt.date
            target_date_obj = pd.to_datetime(target_date).date()
            df_filtered = df[df['date'] == target_date_obj]

            if df_filtered.empty:
                print(f"⚠️  No price data for {symbol} on {target_date}")
                return [], []

            times = pd.to_datetime(df_filtered['ts_utc']).tolist()
            prices = df_filtered['close'].tolist()
            return times, prices
        except Exception as e:
            print(f"❌ Error loading price data for {symbol}: {e}")
            return [], []

    def _create_symbol_chart_and_table(self, symbol, symbol_trades, color, test_date=None):
        """Create price chart and trade table for a single symbol"""

        # Load continuous historical price data for ALL symbols
        if test_date:
            times, prices = self._load_price_data(symbol, test_date)
            if times and prices:
                price_trace = go.Scatter(
                    x=times,
                    y=prices,
                    mode='lines+markers',
                    name=f'{symbol} Price',
                    line=dict(color=color, width=2),
                    marker=dict(size=6, color=color),
                    hovertemplate='%{x}<br>Price: $%{y:.2f}<extra></extra>',
                    showlegend=False
                )
            else:
                # Fallback: empty price trace if data not available
                price_trace = go.Scatter(
                    x=[],
                    y=[],
                    mode='lines+markers',
                    name=f'{symbol} Price',
                    line=dict(color=color, width=2),
                    marker=dict(size=6, color=color),
                    showlegend=False
                )
        else:
            # No test date: use trade data if available
            if symbol_trades:
                sorted_trades = sorted(symbol_trades, key=lambda t: t['timestamp_ms'])
                times = [datetime.fromtimestamp(t['timestamp_ms'] / 1000) for t in sorted_trades]
                prices = [t['exec_price'] for t in sorted_trades]
                price_trace = go.Scatter(
                    x=times,
                    y=prices,
                    mode='lines+markers',
                    name=f'{symbol} Price',
                    line=dict(color=color, width=2),
                    marker=dict(size=6, color=color),
                    hovertemplate='%{x}<br>Price: $%{y:.2f}<extra></extra>',
                    showlegend=False
                )
            else:
                # Empty trace
                price_trace = go.Scatter(
                    x=[],
                    y=[],
                    mode='lines+markers',
                    name=f'{symbol} Price',
                    line=dict(color=color, width=2),
                    marker=dict(size=6, color=color),
                    showlegend=False
                )

        # Handle symbols with no trades
        if not symbol_trades:
            # Create a single row table with "No trades" message
            table_rows = [['N/A', 'NO TRADES', 'N/A', '0', '$0.00', '-', '-', '-', 'Symbol not selected by rotation strategy']]
            return price_trace, None, None, table_rows

        # Sort trades by timestamp
        sorted_trades = sorted(symbol_trades, key=lambda t: t['timestamp_ms'])

        # Separate entries and exits
        entries = [t for t in sorted_trades if t.get('action') == 'ENTRY']
        exits = [t for t in sorted_trades if t.get('action') == 'EXIT']

        # Entry markers
        entry_trace = None
        if entries:
            entry_times = [datetime.fromtimestamp(t['timestamp_ms'] / 1000) for t in entries]
            entry_prices = [t['exec_price'] for t in entries]
            entry_texts = []
            for t in entries:
                text = f"<b>ENTRY</b><br>Price: ${t['exec_price']:.2f}<br>Shares: {t['shares']}<br>Value: ${t['value']:.2f}"
                if 'signal_probability' in t:
                    text += f"<br>Signal Prob: {t['signal_probability']:.3f}"
                if 'signal_rank' in t:
                    text += f"<br>Rank: {t['signal_rank']}"
                entry_texts.append(text)

            entry_trace = go.Scatter(
                x=entry_times,
                y=entry_prices,
                mode='markers',
                name=f'{symbol} ENTRY',
                marker=dict(
                    symbol='triangle-up',
                    size=15,
                    color='green',
                    line=dict(width=2, color='white')
                ),
                text=entry_texts,
                hoverinfo='text',
                showlegend=False
            )

        # Exit markers
        exit_trace = None
        if exits:
            exit_times = [datetime.fromtimestamp(t['timestamp_ms'] / 1000) for t in exits]
            exit_prices = [t['exec_price'] for t in exits]
            exit_texts = []
            for t in exits:
                pnl = t.get('pnl', 0)
                pnl_pct = t.get('pnl_pct', 0)
                text = f"<b>EXIT</b><br>Price: ${t['exec_price']:.2f}<br>Shares: {t['shares']}"
                text += f"<br>P&L: ${pnl:.2f} ({pnl_pct*100:.2f}%)<br>Bars: {t.get('bars_held', 0)}"
                if 'reason' in t:
                    text += f"<br>Reason: {t['reason']}"
                exit_texts.append(text)

            exit_trace = go.Scatter(
                x=exit_times,
                y=exit_prices,
                mode='markers',
                name=f'{symbol} EXIT',
                marker=dict(
                    symbol='triangle-down',
                    size=15,
                    color='red',
                    line=dict(width=2, color='white')
                ),
                text=exit_texts,
                hoverinfo='text',
                showlegend=False
            )

        # Create trade table data
        table_rows = []
        for trade in sorted_trades:
            timestamp = datetime.fromtimestamp(trade['timestamp_ms'] / 1000).strftime('%Y-%m-%d %H:%M')
            action = trade.get('action', 'N/A')
            price = f"${trade.get('exec_price', 0):.2f}"
            shares = str(trade.get('shares', 0))
            value = f"${trade.get('value', 0):.2f}"

            if action == 'EXIT':
                pnl = trade.get('pnl', 0)
                pnl_pct = trade.get('pnl_pct', 0)
                pnl_str = f"${pnl:.2f}"
                pnl_pct_str = f"{pnl_pct*100:.2f}%"
                bars_held = str(trade.get('bars_held', 0))
                reason = trade.get('reason', '')[:50]  # Truncate long reasons
            else:
                pnl_str = '-'
                pnl_pct_str = '-'
                bars_held = '-'
                reason = trade.get('reason', '')[:50]

            table_rows.append([timestamp, action, price, shares, value, pnl_str, pnl_pct_str, bars_held, reason])

        return price_trace, entry_trace, exit_trace, table_rows

    def create_dashboard(self, test_date="Unknown Date"):
        """Create comprehensive multi-symbol dashboard"""
        print(f"\n🎨 Creating dashboard...")

        # Use ALL configured symbols sorted by total P&L (descending)
        sorted_symbols = sorted(
            self.all_symbols,
            key=lambda s: self.symbol_metrics[s]['total_pnl'],
            reverse=True
        )
        num_symbols = len(sorted_symbols)

        # Create subplots: 2 rows per symbol (chart + table) + 1 portfolio equity + 1 summary table
        total_rows = (num_symbols * 2) + 2

        # Build subplot specs and calculate dynamic row heights
        subplot_specs = []
        subplot_titles = []
        row_heights = []

        for symbol in sorted_symbols:
            # Chart row
            subplot_specs.append([{"type": "xy"}])
            subplot_titles.append(f"<b>{symbol}</b> Price & Trades")
            row_heights.append(500)  # Fixed chart height at 500px

            # Table row - fixed height for 6 rows with vertical scrollbar
            subplot_specs.append([{"type": "table"}])
            num_trades = len(self.trades_by_symbol.get(symbol, []))
            if num_trades == 0:
                subplot_titles.append(f"<b>{symbol}</b> Trade Statement (NO TRADES - 0% allocation)")
            else:
                subplot_titles.append(f"<b>{symbol}</b> Trade Statement (ALL {num_trades} trades)")

            # Fixed table height to ensure minimum 6 visible rows with scrollbar for overflow
            # Header (60px) + 6 rows (6 * 38px = 228px) + generous padding for scrollbar and spacing
            row_heights.append(550)  # Fixed at 550px to ensure 6 rows are visible

        # Portfolio equity row
        subplot_specs.append([{"type": "xy"}])
        subplot_titles.append("<b>Portfolio Equity Curve</b>")
        row_heights.append(400)

        # Summary table row (6 symbols + 1 portfolio row = 7 rows)
        subplot_specs.append([{"type": "table"}])
        subplot_titles.append("<b>Performance Summary</b>")
        # Calculate height: header (70px) + (7 rows * 45px) + padding = 70 + 315 + 80 = 465px
        row_heights.append(520)  # Enough for all 7 rows without scrolling (increased to accommodate taller header)

        # Calculate safe vertical spacing for any number of symbols
        # Max spacing = 1/(rows-1), use 80% of max to be safe
        max_spacing = 1.0 / (total_rows - 1) if total_rows > 1 else 0.05
        vertical_spacing = min(0.025, max_spacing * 0.8)

        fig = make_subplots(
            rows=total_rows,
            cols=1,
            subplot_titles=subplot_titles,
            vertical_spacing=vertical_spacing,
            specs=subplot_specs,
            row_heights=row_heights
        )

        # Color map for symbols (12 distinct colors - removed gold miners NUGT/DUST)
        colors = {
            'ERX': '#FF4500',   # Orange Red
            'ERY': '#8B0000',   # Dark Red
            'FAS': '#00CED1',   # Dark Turquoise
            'FAZ': '#4169E1',   # Royal Blue
            'SDS': '#FF6B6B',   # Red
            'SSO': '#32CD32',   # Lime Green
            'SQQQ': '#FF1493',  # Deep Pink
            'SVXY': '#7FFF00',  # Chartreuse
            'TNA': '#FF8C00',   # Dark Orange
            'TQQQ': '#00BFFF',  # Deep Sky Blue
            'TZA': '#DC143C',   # Crimson
            'UVXY': '#9370DB',  # Medium Purple
        }

        # Plot each symbol (chart + table)
        row = 1
        for symbol in sorted_symbols:
            symbol_trades = self.trades_by_symbol.get(symbol, [])
            color = colors.get(symbol, '#888888')

            # Create chart and table
            price_trace, entry_trace, exit_trace, table_rows = self._create_symbol_chart_and_table(
                symbol, symbol_trades, color, test_date
            )

            # Add price line
            fig.add_trace(price_trace, row=row, col=1)

            # Add entry markers (only if they exist)
            if entry_trace is not None:
                fig.add_trace(entry_trace, row=row, col=1)

            # Add exit markers (only if they exist)
            if exit_trace is not None:
                fig.add_trace(exit_trace, row=row, col=1)

            # Update chart axes
            fig.update_yaxes(title_text=f"Price ($)", row=row, col=1)
            fig.update_xaxes(title_text="Time (ET)", row=row, col=1)

            row += 1

            # Add trade statement table
            # NOTE: Plotly tables don't support vertical text alignment (valign).
            # Headers may appear top-aligned. This is a known Plotly limitation.
            # See: megadocs/PLOTLY_TABLE_VERTICAL_CENTERING_BUG.md for details
            table_headers = ['Timestamp', 'Action', 'Price', 'Shares', 'Value', 'P&L', 'P&L %', 'Bars', 'Reason']
            table_values = list(zip(*table_rows)) if table_rows else [[] for _ in table_headers]

            fig.add_trace(
                go.Table(
                    header=dict(
                        values=table_headers,
                        fill_color=color,
                        font=dict(color='white', size=16, family='Arial'),  # Reduced from 18 for better visual balance
                        align='center',
                        height=48,  # Reduced from 60 to minimize visual misalignment impact
                        line=dict(color=color, width=0)  # Clean look without borders
                    ),
                    cells=dict(
                        values=table_values,
                        fill_color=['white', '#f9f9f9'] * (len(table_rows) // 2 + 1),
                        font=dict(size=15),
                        align='left',
                        height=38
                    )
                ),
                row=row, col=1
            )

            row += 1

        # Portfolio equity curve
        equity_times = []
        equity_values = []
        current_equity = self.start_equity

        for trade in sorted(self.trades, key=lambda t: t['timestamp_ms']):
            trade_time = datetime.fromtimestamp(trade['timestamp_ms'] / 1000)

            # Update equity on EXIT (when P&L is realized)
            if trade.get('action') == 'EXIT':
                pnl = trade.get('pnl', 0.0)
                if pnl is not None:
                    current_equity += pnl

            equity_times.append(trade_time)
            equity_values.append(current_equity)

        if equity_times:
            # Add starting equity reference line as a scatter trace
            fig.add_trace(
                go.Scatter(
                    x=[equity_times[0], equity_times[-1]],
                    y=[self.start_equity, self.start_equity],
                    mode='lines',
                    name='Start Equity',
                    line=dict(color='gray', width=2, dash='dash'),
                    hovertemplate=f'Start Equity: ${self.start_equity:,.2f}<extra></extra>',
                    showlegend=False
                ),
                row=row, col=1
            )

            # Add portfolio equity curve
            fig.add_trace(
                go.Scatter(
                    x=equity_times,
                    y=equity_values,
                    mode='lines',
                    name='Portfolio Equity',
                    line=dict(color='#667eea', width=3),
                    fill='tonexty',
                    fillcolor='rgba(102, 126, 234, 0.1)',
                    hovertemplate='%{x}<br>Equity: $%{y:,.2f}<extra></extra>',
                    showlegend=False
                ),
                row=row, col=1
            )

        fig.update_yaxes(title_text="Portfolio Equity ($)", row=row, col=1)
        fig.update_xaxes(title_text="Time (ET)", row=row, col=1)

        row += 1

        # Performance summary table
        table_data = []

        # Header
        headers = ['Symbol', 'Total Trades', 'Wins', 'Losses', 'Win Rate', 'Total P&L', 'Avg Win', 'Avg Loss', 'Avg Hold']

        # Per-symbol rows
        for symbol in sorted_symbols:
            metrics = self.symbol_metrics[symbol]
            table_data.append([
                symbol,
                f"{metrics['total_trades']} ({metrics['entries']}E / {metrics['exits']}X)",
                metrics['wins'],
                metrics['losses'],
                f"{metrics['win_rate']:.1f}%",
                f"${metrics['total_pnl']:.2f}",
                f"${metrics['avg_win']:.2f}",
                f"${metrics['avg_loss']:.2f}",
                f"{metrics['avg_bars_held']:.1f}",
            ])

        # Portfolio totals row
        pm = self.portfolio_metrics
        table_data.append([
            '<b>PORTFOLIO</b>',
            f"<b>{pm['total_trades']}</b>",
            f"<b>{pm['total_wins']}</b>",
            f"<b>{pm['total_losses']}</b>",
            f"<b>{pm['portfolio_win_rate']:.1f}%</b>",
            f"<b>${pm['total_pnl']:.2f}</b>",
            '',
            '',
            '',
        ])

        # Transpose for table
        table_values = list(zip(*table_data))

        # NOTE: Plotly tables don't support vertical text alignment (valign).
        # See: megadocs/PLOTLY_TABLE_VERTICAL_CENTERING_BUG.md for details
        fig.add_trace(
            go.Table(
                header=dict(
                    values=headers,
                    fill_color='#667eea',
                    font=dict(color='white', size=18, family='Arial'),  # Reduced from 21 for better visual balance
                    align='center',
                    height=50,  # Reduced from 70 to minimize visual misalignment impact
                    line=dict(color='#667eea', width=0)  # Clean look without borders
                ),
                cells=dict(
                    values=table_values,
                    fill_color=[['white', '#f0f0f0'] * (len(table_data) - 1) + ['#ffeaa7']],  # Highlight totals
                    font=dict(size=18),
                    align='center',
                    height=45
                )
            ),
            row=row, col=1
        )

        # Update layout
        # Calculate total height: sum of rows + spacing between rows + extra padding
        total_figure_height = sum(row_heights) + (total_rows * 50) + 100  # 50px per title/spacing + 100px top margin

        fig.update_layout(
            height=total_figure_height,
            showlegend=False,
            hovermode='closest',
            plot_bgcolor='white',
            paper_bgcolor='#f8f9fa',
            font=dict(family='Arial', size=17, color='#2c3e50'),
            margin=dict(t=80, b=20, l=50, r=50)  # Increased top margin for caption visibility
        )

        # Update subplot title annotations (20% smaller than 32px = 26px, non-bold)
        # and adjust y position to add more space above charts
        for annotation in fig.layout.annotations:
            annotation.font.size = 26
            annotation.font.family = 'Arial'
            # Move annotations up to ensure visibility (yshift in pixels)
            annotation.yshift = 15

        return fig

    def generate(self, output_path):
        """Generate the complete dashboard"""
        self.load_data()
        self.calculate_metrics()

        # Extract test date from trades
        test_date = "Unknown Date"
        if self.trades:
            # Get timestamp from first trade and convert to date
            first_timestamp_ms = self.trades[0].get('timestamp_ms', 0)
            if first_timestamp_ms:
                test_date = datetime.fromtimestamp(first_timestamp_ms / 1000).strftime('%Y-%m-%d')

        fig = self.create_dashboard(test_date)

        # Add summary statistics at the top
        pm = self.portfolio_metrics

        html_header = f"""
        <div style="background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
                    color: white; padding: 30px; margin-bottom: 20px; border-radius: 10px;">
            <h1 style="margin: 0; text-align: center;">🎯 Rotation Trading Report</h1>
            <p style="text-align: center; font-size: 27px; margin: 10px 0 0 0;">
                Multi-Symbol Position Rotation Strategy
            </p>
            <p style="text-align: center; font-size: 24px; margin: 10px 0 0 0; opacity: 0.9;">
                Test Date: {test_date}
            </p>
        </div>

        <div style="display: grid; grid-template-columns: repeat(auto-fit, minmax(250px, 1fr));
                    gap: 20px; margin-bottom: 30px;">
            <div style="background: white; padding: 20px; border-radius: 10px; box-shadow: 0 2px 4px rgba(0,0,0,0.1);">
                <div style="font-size: 21px; color: #666; margin-bottom: 5px;">Starting Equity</div>
                <div style="font-size: 42px; font-weight: bold; color: #2c3e50;">
                    ${self.start_equity:,.2f}
                </div>
            </div>

            <div style="background: white; padding: 20px; border-radius: 10px; box-shadow: 0 2px 4px rgba(0,0,0,0.1);">
                <div style="font-size: 21px; color: #666; margin-bottom: 5px;">Final Equity</div>
                <div style="font-size: 42px; font-weight: bold; color: {'#27ae60' if pm['total_pnl'] >= 0 else '#e74c3c'};">
                    ${pm['final_equity']:,.2f}
                </div>
            </div>

            <div style="background: white; padding: 20px; border-radius: 10px; box-shadow: 0 2px 4px rgba(0,0,0,0.1);">
                <div style="font-size: 21px; color: #666; margin-bottom: 5px;">Total P&L</div>
                <div style="font-size: 42px; font-weight: bold; color: {'#27ae60' if pm['total_pnl'] >= 0 else '#e74c3c'};">
                    ${pm['total_pnl']:+,.2f}
                </div>
                <div style="font-size: 24px; color: #666;">
                    ({pm['return_pct']:+.2f}%)
                </div>
            </div>

            <div style="background: white; padding: 20px; border-radius: 10px; box-shadow: 0 2px 4px rgba(0,0,0,0.1);">
                <div style="font-size: 21px; color: #666; margin-bottom: 5px;">Total Trades</div>
                <div style="font-size: 42px; font-weight: bold; color: #2c3e50;">
                    {pm['total_trades']}
                </div>
                <div style="font-size: 24px; color: #666;">
                    {pm['symbols_traded']} symbols
                </div>
            </div>

            <div style="background: white; padding: 20px; border-radius: 10px; box-shadow: 0 2px 4px rgba(0,0,0,0.1);">
                <div style="font-size: 21px; color: #666; margin-bottom: 5px;">Win Rate</div>
                <div style="font-size: 42px; font-weight: bold; color: #3498db;">
                    {pm['portfolio_win_rate']:.1f}%
                </div>
                <div style="font-size: 24px; color: #666;">
                    {pm['total_wins']}W / {pm['total_losses']}L
                </div>
            </div>
        </div>
        """

        # Save with custom HTML wrapper
        html_str = fig.to_html(include_plotlyjs='cdn', full_html=False)

        full_html = f"""
        <!DOCTYPE html>
        <html>
        <head>
            <meta charset="utf-8">
            <title>Rotation Trading Dashboard</title>
            <style>
                body {{
                    font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Arial, sans-serif;
                    margin: 0;
                    padding: 20px;
                    background-color: #f8f9fa;
                }}
                .container {{
                    max-width: 1600px;
                    margin: 0 auto;
                }}
            </style>
        </head>
        <body>
            <div class="container">
                {html_header}
                {html_str}
                <div style="text-align: center; margin-top: 30px; padding: 20px; color: #666; border-top: 1px solid #ddd;">
                    <p>🤖 Generated by OnlineTrader Rotation System v2.0</p>
                    <p style="font-size: 18px;">Generated at {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}</p>
                </div>
            </div>
        </body>
        </html>
        """

        with open(output_path, 'w') as f:
            f.write(full_html)

        print(f"\n✅ Dashboard saved: {output_path}")

        # Print summary
        print(f"\n{'='*60}")
        print(f"ROTATION TRADING SUMMARY")
        print(f"{'='*60}")
        print(f"Starting Equity:  ${self.start_equity:,.2f}")
        print(f"Final Equity:     ${pm['final_equity']:,.2f}")
        print(f"Total P&L:        ${pm['total_pnl']:+,.2f} ({pm['return_pct']:+.2f}%)")
        print(f"Total Trades:     {pm['total_trades']} ({pm['symbols_traded']} symbols)")
        print(f"Win Rate:         {pm['portfolio_win_rate']:.1f}% ({pm['total_wins']}W / {pm['total_losses']}L)")
        print(f"{'='*60}")
        print(f"\nPer-Symbol Breakdown:")
        for symbol in sorted(self.symbol_metrics.keys()):
            metrics = self.symbol_metrics[symbol]
            print(f"  {symbol:6s}: {metrics['total_trades']:2d} trades, "
                  f"P&L ${metrics['total_pnl']:+8.2f}, "
                  f"WR {metrics['win_rate']:5.1f}%")
        print(f"{'='*60}\n")


def main():
    parser = argparse.ArgumentParser(description='Generate rotation trading dashboard')
    parser.add_argument('--trades', required=True, help='Path to trades.jsonl')
    parser.add_argument('--signals', help='Path to signals.jsonl')
    parser.add_argument('--positions', help='Path to positions.jsonl')
    parser.add_argument('--decisions', help='Path to decisions.jsonl')
    parser.add_argument('--output', required=True, help='Output HTML path')
    parser.add_argument('--start-equity', type=float, default=100000.0, help='Starting equity')

    args = parser.parse_args()

    print("="*60)
    print("ROTATION TRADING DASHBOARD GENERATOR")
    print("="*60)

    dashboard = RotationTradingDashboard(
        trades_path=args.trades,
        signals_path=args.signals,
        positions_path=args.positions,
        decisions_path=args.decisions,
        start_equity=args.start_equity
    )

    dashboard.generate(args.output)

    return 0


if __name__ == '__main__':
    sys.exit(main())
