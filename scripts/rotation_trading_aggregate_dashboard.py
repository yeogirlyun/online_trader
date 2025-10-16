#!/usr/bin/env python3
"""
Rotation Trading Aggregate Summary Dashboard
===========================================

Aggregates trading data across multiple days to create comprehensive summary:
- Overall performance summary
- Per-symbol performance breakdown
- Daily performance table
- Price charts with trades across all days for each symbol
- Complete trade statements for each symbol

Usage:
    python rotation_trading_aggregate_dashboard.py \
        --batch-dir logs/october_2025 \
        --output logs/october_2025/dashboards/aggregate_summary.html \
        --start-date 2025-10-01 \
        --end-date 2025-10-15
"""

import argparse
import json
import sys
import os
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


class AggregateDashboard:
    """Aggregate summary dashboard across multiple trading days"""

    def __init__(self, batch_dir, start_date, end_date, start_equity=100000.0, data_dir='data/equities'):
        self.batch_dir = batch_dir
        self.start_date = start_date
        self.end_date = end_date
        self.start_equity = start_equity
        self.data_dir = data_dir

        # All expected symbols (12 instruments - removed gold miners NUGT/DUST)
        self.all_symbols = ['ERX', 'ERY', 'FAS', 'FAZ', 'SDS', 'SSO', 'SQQQ', 'SVXY', 'TNA', 'TQQQ', 'TZA', 'UVXY']

        # Data structures
        self.daily_data = {}  # date -> {trades, equity_curve, pnl, etc}
        self.all_trades = []
        self.trades_by_symbol = defaultdict(list)

        # Aggregate metrics
        self.daily_metrics = []
        self.symbol_metrics = {}
        self.portfolio_metrics = {}

    def load_all_data(self):
        """Load trade data from all days in batch directory"""
        print(f"📊 Loading aggregate data from {self.batch_dir}...")

        # Find all day subdirectories
        trading_days = []
        for item in sorted(os.listdir(self.batch_dir)):
            day_path = os.path.join(self.batch_dir, item)
            if os.path.isdir(day_path) and item.startswith('2025-'):
                trading_days.append(item)

        print(f"✓ Found {len(trading_days)} trading days: {trading_days[0]} to {trading_days[-1]}")

        # Load data from each day
        cumulative_equity = self.start_equity

        for day in trading_days:
            day_path = os.path.join(self.batch_dir, day)
            trades_file = os.path.join(day_path, 'trades.jsonl')

            if not os.path.exists(trades_file):
                print(f"⚠️  No trades file for {day}")
                continue

            # Load day's trades
            day_trades = []
            with open(trades_file, 'r') as f:
                for line in f:
                    if line.strip():
                        trade = json.loads(line)
                        trade['date'] = day  # Tag with date
                        day_trades.append(trade)
                        self.all_trades.append(trade)

                        symbol = trade.get('symbol', 'UNKNOWN')
                        self.trades_by_symbol[symbol].append(trade)

            # Calculate day's P&L
            day_pnl = sum(t.get('pnl', 0.0) for t in day_trades if t.get('action') == 'EXIT')
            cumulative_equity += day_pnl

            # Store day data
            self.daily_data[day] = {
                'trades': day_trades,
                'pnl': day_pnl,
                'equity': cumulative_equity,
                'num_trades': len(day_trades)
            }

            print(f"   {day}: {len(day_trades)} trades, P&L: ${day_pnl:+,.2f}, Equity: ${cumulative_equity:,.2f}")

        print(f"\n✓ Loaded {len(self.all_trades)} total trades across {len(trading_days)} days")

    def calculate_metrics(self):
        """Calculate aggregate metrics"""
        print(f"\n📊 Calculating aggregate metrics...")

        # Daily metrics for chart
        for day in sorted(self.daily_data.keys()):
            data = self.daily_data[day]
            self.daily_metrics.append({
                'date': day,
                'pnl': data['pnl'],
                'equity': data['equity'],
                'num_trades': data['num_trades']
            })

        # Per-symbol aggregate metrics
        for symbol in self.all_symbols:
            symbol_trades = self.trades_by_symbol.get(symbol, [])
            self.symbol_metrics[symbol] = self._calculate_symbol_metrics(symbol, symbol_trades)

        # Portfolio aggregate metrics
        self.portfolio_metrics = self._calculate_portfolio_metrics()

        print(f"✓ Aggregate metrics calculated")

    def _calculate_symbol_metrics(self, symbol, symbol_trades):
        """Calculate metrics for a symbol across all days"""
        entries = [t for t in symbol_trades if t.get('action') == 'ENTRY']
        exits = [t for t in symbol_trades if t.get('action') == 'EXIT']

        total_pnl = sum(t.get('pnl', 0.0) for t in exits)
        winning_trades = [t for t in exits if t.get('pnl', 0.0) > 0]
        losing_trades = [t for t in exits if t.get('pnl', 0.0) < 0]

        return {
            'symbol': symbol,
            'total_trades': len(symbol_trades),
            'entries': len(entries),
            'exits': len(exits),
            'total_pnl': total_pnl,
            'winning_trades': len(winning_trades),
            'losing_trades': len(losing_trades),
            'win_rate': len(winning_trades) / len(exits) * 100 if exits else 0.0,
            'avg_win': np.mean([t.get('pnl', 0.0) for t in winning_trades]) if winning_trades else 0.0,
            'avg_loss': np.mean([t.get('pnl', 0.0) for t in losing_trades]) if losing_trades else 0.0,
        }

    def _calculate_portfolio_metrics(self):
        """Calculate overall portfolio metrics"""
        all_exits = [t for t in self.all_trades if t.get('action') == 'EXIT']
        total_pnl = sum(t.get('pnl', 0.0) for t in all_exits)
        winning_trades = [t for t in all_exits if t.get('pnl', 0.0) > 0]
        losing_trades = [t for t in all_exits if t.get('pnl', 0.0) < 0]

        final_equity = self.daily_metrics[-1]['equity'] if self.daily_metrics else self.start_equity
        total_return = (final_equity - self.start_equity) / self.start_equity * 100

        num_days = len(self.daily_data)

        # Calculate MRD (Mean Return per Day) from daily metrics
        daily_returns = []
        if len(self.daily_metrics) > 0:
            prev_equity = self.start_equity
            for day_metric in self.daily_metrics:
                day_equity = day_metric['equity']
                day_return = (day_equity - prev_equity) / prev_equity * 100
                daily_returns.append(day_return)
                prev_equity = day_equity
        avg_mrd = np.mean(daily_returns) if daily_returns else 0.0

        return {
            'total_trades': len(self.all_trades),
            'total_exits': len(all_exits),
            'total_pnl': total_pnl,
            'total_return': total_return,
            'final_equity': final_equity,
            'winning_trades': len(winning_trades),
            'losing_trades': len(losing_trades),
            'win_rate': len(winning_trades) / len(all_exits) * 100 if all_exits else 0.0,
            'avg_win': np.mean([t.get('pnl', 0.0) for t in winning_trades]) if winning_trades else 0.0,
            'avg_loss': np.mean([t.get('pnl', 0.0) for t in losing_trades]) if losing_trades else 0.0,
            'profit_factor': abs(sum(t.get('pnl', 0.0) for t in winning_trades) / sum(t.get('pnl', 0.0) for t in losing_trades)) if losing_trades else float('inf'),
            'num_days': num_days,
            'avg_pnl_per_day': total_pnl / num_days if num_days > 0 else 0.0,
            'avg_mrd': avg_mrd
        }

    def _load_price_data(self, symbol, date_list):
        """Load historical price data for a symbol across multiple dates (RTH only)"""
        csv_path = os.path.join(self.data_dir, f'{symbol}_RTH_NH.csv')

        if not os.path.exists(csv_path):
            print(f"⚠️  Price data not found for {symbol}: {csv_path}")
            return [], []

        try:
            df = pd.read_csv(csv_path)
            df['datetime'] = pd.to_datetime(df['ts_utc'])
            df['date'] = df['datetime'].dt.date

            # Filter for dates in our range
            date_objs = [pd.to_datetime(d).date() for d in date_list]
            df_filtered = df[df['date'].isin(date_objs)].copy()

            if df_filtered.empty:
                print(f"⚠️  No price data for {symbol} in date range")
                return [], []

            # Filter for RTH only (9:30 AM - 4:00 PM ET)
            # Convert to ET timezone for hour filtering
            if df_filtered['datetime'].dt.tz is None:
                df_filtered['datetime_et'] = df_filtered['datetime'].dt.tz_localize('UTC').dt.tz_convert('America/New_York')
            else:
                df_filtered['datetime_et'] = df_filtered['datetime'].dt.tz_convert('America/New_York')

            df_filtered['hour'] = df_filtered['datetime_et'].dt.hour
            df_filtered['minute'] = df_filtered['datetime_et'].dt.minute

            # RTH filter: 9:30 AM (09:30) to 4:00 PM (16:00) ET
            mask = (
                ((df_filtered['hour'] == 9) & (df_filtered['minute'] >= 30)) |
                ((df_filtered['hour'] > 9) & (df_filtered['hour'] < 16)) |
                ((df_filtered['hour'] == 16) & (df_filtered['minute'] == 0))
            )
            df_rth = df_filtered[mask]

            if df_rth.empty:
                print(f"⚠️  No RTH price data for {symbol} in date range")
                return [], []

            times = df_rth['datetime'].tolist()
            prices = df_rth['close'].tolist()
            return times, prices
        except Exception as e:
            print(f"❌ Error loading price data for {symbol}: {e}")
            return [], []

    def generate_dashboard(self, output_path):
        """Generate comprehensive aggregate dashboard"""
        print(f"\n📈 Generating aggregate dashboard...")

        # Sort symbols by total P&L descending
        sorted_symbols = sorted(
            self.all_symbols,
            key=lambda s: self.symbol_metrics[s]['total_pnl'],
            reverse=True
        )

        # Calculate layout: Portfolio Summary + Per-Symbol Summary + Daily Performance + (Chart + Table) per symbol
        # Single column layout
        total_rows = 3 + (len(sorted_symbols) * 2)  # 3 summary tables + 2 rows per symbol

        subplot_specs = []
        subplot_titles = []
        row_heights = []

        # Row 1: Portfolio Summary (transposed: 2 rows - header + data row)
        subplot_specs.append([{"type": "table"}])
        subplot_titles.append("<b>Portfolio Performance Summary</b>")
        row_heights.append(150)

        # Row 2: Per-Symbol Performance Summary
        subplot_specs.append([{"type": "table"}])
        subplot_titles.append("<b>Per-Symbol Performance Summary</b>")
        row_heights.append(350)

        # Row 3: Daily Performance
        subplot_specs.append([{"type": "table"}])
        subplot_titles.append("<b>Daily Performance</b>")
        row_heights.append(450)

        # Rows for each symbol (chart + table)
        for symbol in sorted_symbols:
            # Chart row
            subplot_specs.append([{"type": "xy"}])
            num_trades = len(self.trades_by_symbol.get(symbol, []))
            subplot_titles.append(f"<b>{symbol}</b> Price & Trades ({self.start_date} to {self.end_date})")
            row_heights.append(500)

            # Table row
            subplot_specs.append([{"type": "table"}])
            subplot_titles.append(f"<b>{symbol}</b> Trade Statement (ALL {num_trades} trades)")
            row_heights.append(550)

        # Create figure
        fig = make_subplots(
            rows=total_rows,
            cols=1,
            subplot_titles=subplot_titles,
            specs=subplot_specs,
            vertical_spacing=0.015,
            row_heights=row_heights
        )

        current_row = 1

        # Add Portfolio Summary Table
        self._add_portfolio_summary(fig, current_row)
        current_row += 1

        # Add Per-Symbol Performance Summary
        self._add_symbol_summary_table(fig, current_row)
        current_row += 1

        # Add Daily Performance Table
        self._add_daily_performance_table(fig, current_row)
        current_row += 1

        # Add symbol charts and tables
        for symbol in sorted_symbols:
            self._add_symbol_chart_and_table(fig, symbol, current_row)
            current_row += 2

        # Update layout
        total_height = sum(row_heights) + (total_rows * 30)  # Add spacing
        fig.update_layout(
            title=dict(
                text=f'<b>Aggregate Trading Summary: {self.start_date} to {self.end_date}</b>',
                font=dict(size=36, family='Arial Black'),
                x=0.5,
                xanchor='center'
            ),
            showlegend=False,
            height=total_height,
            plot_bgcolor='white',
            paper_bgcolor='white',
            font=dict(family='Arial', size=18),
            margin=dict(t=120, b=40, l=40, r=40)
        )

        # Increase caption font sizes and make them bold
        for annotation in fig['layout']['annotations']:
            annotation['font'] = dict(size=24, family='Arial', color='#333')

        # Save
        fig.write_html(output_path)
        print(f"✅ Aggregate dashboard saved: {output_path}")

        return output_path

    def _add_portfolio_summary(self, fig, row):
        """Add portfolio summary table (transposed: metrics as columns)"""
        pm = self.portfolio_metrics

        fig.add_trace(
            go.Table(
                header=dict(
                    values=['<b>Test Period</b>', '<b>Trading Days</b>', '<b>Total Trades</b>', '<b>Total Exits</b>',
                            '<b>Win Rate</b>', '<b>Total P&L</b>', '<b>Total Return</b>', '<b>Average MRD</b>',
                            '<b>Final Equity</b>', '<b>Avg P&L/Day</b>', '<b>Profit Factor</b>'],
                    fill_color='#667eea',
                    font=dict(color='white', size=16, family='Arial'),
                    align='center',
                    height=45,
                    line=dict(color='#667eea', width=0)
                ),
                cells=dict(
                    values=[
                        [f"{self.start_date} to {self.end_date}"],
                        [f"{pm['num_days']}"],
                        [f"{pm['total_trades']}"],
                        [f"{pm['total_exits']}"],
                        [f"{pm['win_rate']:.1f}%"],
                        [f"${pm['total_pnl']:+,.2f}"],
                        [f"{pm['total_return']:+.2f}%"],
                        [f"{pm['avg_mrd']:+.3f}%"],
                        [f"${pm['final_equity']:,.2f}"],
                        [f"${pm['avg_pnl_per_day']:+,.2f}"],
                        [f"{pm['profit_factor']:.2f}"]
                    ],
                    fill_color='white',
                    font=dict(size=15, family='Arial'),
                    align='center',
                    height=40
                )
            ),
            row=row, col=1
        )

    def _add_symbol_summary_table(self, fig, row):
        """Add per-symbol performance summary table"""
        # Sort symbols by total P&L descending
        sorted_symbols = sorted(
            self.all_symbols,
            key=lambda s: self.symbol_metrics[s]['total_pnl'],
            reverse=True
        )

        symbols = []
        total_trades_list = []
        exits_list = []
        win_rates = []
        total_pnls = []
        avg_wins = []
        avg_losses = []

        for symbol in sorted_symbols:
            metrics = self.symbol_metrics[symbol]
            symbols.append(f"<b>{symbol}</b>")
            total_trades_list.append(str(metrics['total_trades']))
            exits_list.append(str(metrics['exits']))
            win_rates.append(f"{metrics['win_rate']:.1f}%")
            total_pnls.append(f"${metrics['total_pnl']:+,.2f}")
            avg_wins.append(f"${metrics['avg_win']:+,.2f}" if metrics['avg_win'] != 0 else "$0.00")
            avg_losses.append(f"${metrics['avg_loss']:+,.2f}" if metrics['avg_loss'] != 0 else "$0.00")

        fig.add_trace(
            go.Table(
                header=dict(
                    values=['<b>Symbol</b>', '<b>Total Trades</b>', '<b>Exits</b>', '<b>Win Rate</b>',
                            '<b>Total P&L</b>', '<b>Avg Win</b>', '<b>Avg Loss</b>'],
                    fill_color='#667eea',
                    font=dict(color='white', size=18, family='Arial'),
                    align='center',
                    height=50,
                    line=dict(color='#667eea', width=0)
                ),
                cells=dict(
                    values=[symbols, total_trades_list, exits_list, win_rates, total_pnls, avg_wins, avg_losses],
                    fill_color='white',
                    font=dict(size=16, family='Arial'),
                    align=['center', 'center', 'center', 'center', 'right', 'right', 'right'],
                    height=38
                )
            ),
            row=row, col=1
        )

    def _add_daily_performance_table(self, fig, row):
        """Add daily performance breakdown table"""
        dates = []
        pnls = []
        returns = []
        equities = []
        num_trades = []

        prev_equity = self.start_equity
        for daily in self.daily_metrics:
            dates.append(daily['date'])
            pnls.append(f"${daily['pnl']:+,.2f}")
            daily_return = (daily['pnl'] / prev_equity) * 100
            returns.append(f"{daily_return:+.2f}%")
            equities.append(f"${daily['equity']:,.2f}")
            num_trades.append(str(daily['num_trades']))
            prev_equity = daily['equity']

        fig.add_trace(
            go.Table(
                header=dict(
                    values=['<b>Date</b>', '<b>P&L</b>', '<b>Return</b>', '<b>Equity</b>', '<b>Trades</b>'],
                    fill_color='#764ba2',
                    font=dict(color='white', size=18, family='Arial'),
                    align='center',
                    height=50,
                    line=dict(color='#764ba2', width=0)
                ),
                cells=dict(
                    values=[dates, pnls, returns, equities, num_trades],
                    fill_color='white',
                    font=dict(size=16, family='Arial'),
                    align=['center', 'right', 'right', 'right', 'center'],
                    height=38
                )
            ),
            row=row, col=1
        )

    def _add_symbol_chart_and_table(self, fig, symbol, chart_row):
        """Add price chart and trade table for a symbol"""
        symbol_trades = self.trades_by_symbol.get(symbol, [])

        # Load price data across all dates
        trading_dates = sorted(self.daily_data.keys())
        times, prices = self._load_price_data(symbol, trading_dates)

        # Color mapping (12 distinct colors - removed gold miners NUGT/DUST)
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
        color = colors.get(symbol, '#6366f1')

        # Add price line
        if times and prices:
            fig.add_trace(
                go.Scatter(
                    x=times,
                    y=prices,
                    mode='lines',
                    name=f'{symbol} Price',
                    line=dict(color=color, width=2),
                    hovertemplate='%{x}<br>Price: $%{y:.2f}<extra></extra>',
                    showlegend=False
                ),
                row=chart_row, col=1
            )

        # Add entry markers
        entries = [t for t in symbol_trades if t.get('action') == 'ENTRY']
        if entries:
            entry_times = [datetime.fromtimestamp(t['timestamp_ms'] / 1000) for t in entries]
            entry_prices = [t['exec_price'] for t in entries]
            entry_texts = []
            for t in entries:
                text = f"<b>ENTRY</b><br>{t['date']}<br>Price: ${t['exec_price']:.2f}<br>Shares: {t['shares']}<br>Value: ${t['value']:.2f}"
                entry_texts.append(text)

            fig.add_trace(
                go.Scatter(
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
                ),
                row=chart_row, col=1
            )

        # Add exit markers
        exits = [t for t in symbol_trades if t.get('action') == 'EXIT']
        if exits:
            exit_times = [datetime.fromtimestamp(t['timestamp_ms'] / 1000) for t in exits]
            exit_prices = [t['exec_price'] for t in exits]
            exit_texts = []
            for t in exits:
                pnl = t.get('pnl', 0)
                pnl_pct = t.get('pnl_pct', 0)
                text = f"<b>EXIT</b><br>{t['date']}<br>Price: ${t['exec_price']:.2f}<br>Shares: {t['shares']}<br>P&L: ${pnl:+,.2f} ({pnl_pct*100:+.2f}%)"
                exit_texts.append(text)

            fig.add_trace(
                go.Scatter(
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
                ),
                row=chart_row, col=1
            )

        # Update chart axes with rangebreaks to hide non-trading periods
        fig.update_xaxes(
            title_text='Date/Time',
            row=chart_row,
            col=1,
            tickfont=dict(size=14),
            rangebreaks=[
                dict(bounds=[16, 9.5], pattern="hour"),  # Hide 4:00 PM to 9:30 AM (non-RTH)
                dict(bounds=["sat", "mon"]),  # Hide weekends
            ]
        )
        fig.update_yaxes(title_text='Price ($)', row=chart_row, col=1, tickfont=dict(size=14))

        # Add trade statement table
        table_row = chart_row + 1
        self._add_trade_statement_table(fig, symbol, symbol_trades, table_row)

    def _add_trade_statement_table(self, fig, symbol, symbol_trades, row):
        """Add trade statement table with same format as single-day dashboard"""

        if not symbol_trades:
            # Empty table with message
            fig.add_trace(
                go.Table(
                    header=dict(
                        values=['<b>Timestamp</b>', '<b>Action</b>', '<b>Price</b>', '<b>Shares</b>',
                                '<b>Value</b>', '<b>P&L</b>', '<b>P&L %</b>', '<b>Bars Held</b>', '<b>Reason</b>'],
                        fill_color='#764ba2',
                        font=dict(color='white', size=16, family='Arial'),
                        align='center',
                        height=48,
                        line=dict(color='#764ba2', width=0)
                    ),
                    cells=dict(
                        values=[['N/A'], ['NO TRADES'], ['N/A'], ['0'], ['$0.00'], ['-'], ['-'], ['-'],
                                ['Symbol not selected by rotation strategy']],
                        fill_color='white',
                        font=dict(size=15, family='Arial'),
                        align=['left', 'center', 'right', 'right', 'right', 'right', 'right', 'center', 'left'],
                        height=35
                    )
                ),
                row=row, col=1
            )
            return

        # Sort trades by timestamp
        sorted_trades = sorted(symbol_trades, key=lambda t: t['timestamp_ms'])

        # Format trade data
        timestamps = []
        actions = []
        prices = []
        shares = []
        values = []
        pnls = []
        pnl_pcts = []
        bars_held = []
        reasons = []
        row_colors = []

        for trade in sorted_trades:
            timestamp = datetime.fromtimestamp(trade['timestamp_ms'] / 1000).strftime('%Y-%m-%d %H:%M')
            timestamps.append(timestamp)

            action = trade.get('action', 'N/A')
            actions.append(action)

            prices.append(f"${trade.get('exec_price', 0):.2f}")
            shares.append(str(trade.get('shares', 0)))
            values.append(f"${trade.get('value', 0):.2f}")

            if action == 'EXIT':
                pnl = trade.get('pnl', 0)
                pnl_pct = trade.get('pnl_pct', 0)
                pnls.append(f"${pnl:+,.2f}")
                pnl_pcts.append(f"{pnl_pct*100:+.2f}%")
                bars_held.append(str(trade.get('bars_held', 0)))
                row_colors.append('#ffebee')  # Light red
            else:
                pnls.append('-')
                pnl_pcts.append('-')
                bars_held.append('-')
                row_colors.append('#e8f5e9')  # Light green

            reason = trade.get('reason', '')[:50]  # Truncate long reasons
            reasons.append(reason)

        fig.add_trace(
            go.Table(
                header=dict(
                    values=['<b>Timestamp</b>', '<b>Action</b>', '<b>Price</b>', '<b>Shares</b>',
                            '<b>Value</b>', '<b>P&L</b>', '<b>P&L %</b>', '<b>Bars Held</b>', '<b>Reason</b>'],
                    fill_color='#764ba2',
                    font=dict(color='white', size=16, family='Arial'),
                    align='center',
                    height=48,
                    line=dict(color='#764ba2', width=0)
                ),
                cells=dict(
                    values=[timestamps, actions, prices, shares, values, pnls, pnl_pcts, bars_held, reasons],
                    fill_color=[row_colors],
                    font=dict(size=15, family='Arial'),
                    align=['left', 'center', 'right', 'right', 'right', 'right', 'right', 'center', 'left'],
                    height=35
                )
            ),
            row=row, col=1
        )


def main():
    parser = argparse.ArgumentParser(description='Generate aggregate rotation trading summary dashboard')
    parser.add_argument('--batch-dir', required=True, help='Batch test directory (e.g., logs/october_2025)')
    parser.add_argument('--output', required=True, help='Output HTML file path')
    parser.add_argument('--start-date', required=True, help='Start date (YYYY-MM-DD)')
    parser.add_argument('--end-date', required=True, help='End date (YYYY-MM-DD)')
    parser.add_argument('--capital', type=float, default=100000.0, help='Starting capital (default: 100000)')
    parser.add_argument('--data-dir', default='data/equities', help='Historical data directory')

    args = parser.parse_args()

    # Create dashboard
    dashboard = AggregateDashboard(
        batch_dir=args.batch_dir,
        start_date=args.start_date,
        end_date=args.end_date,
        start_equity=args.capital,
        data_dir=args.data_dir
    )

    # Load and process data
    dashboard.load_all_data()
    dashboard.calculate_metrics()

    # Generate HTML
    dashboard.generate_dashboard(args.output)

    print("\n✅ Aggregate summary dashboard complete!")


if __name__ == '__main__':
    main()
