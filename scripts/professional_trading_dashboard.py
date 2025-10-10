#!/usr/bin/env python3
"""
Professional Trading Visualization Dashboard
============================================

A comprehensive trading visualization tool that creates professional-grade charts
and analysis for trade books. Features include:

- Interactive candlestick charts with trade overlays
- Equity curve with drawdown analysis
- Trade-by-trade P&L visualization
- Volume analysis and trade timing
- Performance metrics dashboard
- Risk metrics and statistics
- Professional styling and layout

Requirements:
- plotly
- pandas
- numpy
- mplfinance (optional, for additional chart types)

Usage:
    python professional_trading_dashboard.py --tradebook trades.jsonl --data SPY_RTH_NH.csv
"""

import argparse
import json
import os
import sys
from datetime import datetime, timezone
from typing import List, Dict, Any, Tuple, Optional
import pandas as pd
import numpy as np
import pytz

try:
    import plotly.graph_objects as go
    from plotly.subplots import make_subplots
    import plotly.express as px
    from plotly.offline import plot
except ImportError:
    print("❌ Plotly not installed. Install with: pip install plotly")
    sys.exit(1)

try:
    import mplfinance as mpf
except ImportError:
    mpf = None
    print("⚠️ mplfinance not installed. Install with: pip install mplfinance for additional chart types")


class TradingDashboard:
    """Professional trading visualization dashboard"""

    def __init__(self, tradebook_path: str, data_path: str, signals_path: str = None, start_equity: float = 100000.0):
        self.tradebook_path = tradebook_path
        self.data_path = data_path
        self.signals_path = signals_path
        self.start_equity = start_equity
        self.trades = []
        self.signals = {}  # Map bar_id -> signal
        self.market_data = None
        self.equity_curve = None
        self.performance_metrics = {}
        
    def load_data(self):
        """Load tradebook, signals, and market data"""
        print("📊 Loading tradebook...")
        self.trades = self._load_tradebook()

        if self.signals_path:
            print("🎯 Loading signals...")
            self.signals = self._load_signals()

        print("📈 Loading market data...")
        self.market_data = self._load_market_data()

        print("📊 Calculating equity curve...")
        self.equity_curve = self._calculate_equity_curve()

        print("📊 Calculating performance metrics...")
        self.performance_metrics = self._calculate_performance_metrics()
        
    def _load_tradebook(self) -> List[Dict[str, Any]]:
        """Load tradebook from JSONL file"""
        trades = []
        with open(self.tradebook_path, "r", encoding="utf-8") as f:
            for line in f:
                line = line.strip()
                if not line:
                    continue
                try:
                    trades.append(json.loads(line))
                except json.JSONDecodeError:
                    continue
        return trades

    def _load_signals(self) -> Dict[int, Dict[str, Any]]:
        """Load signals from JSONL file, indexed by bar_id"""
        signals = {}
        with open(self.signals_path, "r", encoding="utf-8") as f:
            for line in f:
                line = line.strip()
                if not line:
                    continue
                try:
                    signal = json.loads(line)
                    bar_id = signal.get('bar_id')
                    if bar_id:
                        signals[bar_id] = signal
                except json.JSONDecodeError:
                    continue
        print(f"   Loaded {len(signals)} signals")
        return signals
    
    def _load_market_data(self) -> pd.DataFrame:
        """Load market data from CSV"""
        if not os.path.exists(self.data_path):
            print(f"⚠️ Market data file not found: {self.data_path}")
            return None

        df = pd.read_csv(self.data_path)

        # Convert timestamp to datetime in ET timezone, then make tz-naive
        if 'ts_utc' in df.columns:
            # Parse as UTC-aware, then convert to ET, then remove timezone
            df['datetime'] = pd.to_datetime(df['ts_utc'], utc=True).dt.tz_convert('America/New_York').dt.tz_localize(None)
        elif 'ts_nyt_epoch' in df.columns:
            # Epoch is already in ET, so parse as UTC then treat as ET
            df['datetime'] = pd.to_datetime(df['ts_nyt_epoch'], unit='s', utc=True).dt.tz_convert('America/New_York').dt.tz_localize(None)
        else:
            print("❌ No timestamp column found in market data")
            return None
            
        # Ensure OHLC columns are numeric
        for col in ['open', 'high', 'low', 'close', 'volume']:
            if col in df.columns:
                df[col] = pd.to_numeric(df[col], errors='coerce')
                
        return df.dropna()
    
    def _calculate_equity_curve(self) -> pd.DataFrame:
        """Calculate equity curve from trades"""
        if not self.trades:
            return None

        # Create equity curve data
        equity_data = []
        current_equity = self.start_equity

        for trade in self.trades:
            # Extract trade information - handle both C++ string format and Python ms format
            if 'timestamp' in trade and isinstance(trade['timestamp'], str):
                # C++ format: "2025-10-07 09:30:00 America/New_York"
                ts_str = trade['timestamp'].replace(' America/New_York', '')
                timestamp_dt = pd.to_datetime(ts_str)
            elif 'timestamp_ms' in trade:
                # Python format: milliseconds
                timestamp_dt = pd.to_datetime(trade['timestamp_ms'], unit='ms') - pd.Timedelta(hours=4)
            else:
                timestamp_dt = pd.NaT

            equity_after = trade.get('portfolio_value', trade.get('equity_after', current_equity))
            cash_balance = trade.get('cash_balance', trade.get('cash', equity_after))
            pnl = equity_after - current_equity

            equity_data.append({
                'timestamp': timestamp_dt,
                'equity': equity_after,
                'portfolio_value': equity_after,
                'cash': cash_balance,
                'pnl': pnl,
                'trade_type': trade.get('action', trade.get('side', 'unknown')),
                'symbol': trade.get('symbol', 'unknown'),
                'quantity': trade.get('quantity', trade.get('size', 0)),
                'price': trade.get('price', trade.get('fill_price', 0))
            })

            current_equity = equity_after

        return pd.DataFrame(equity_data)
    
    def _calculate_performance_metrics(self) -> Dict[str, Any]:
        """Calculate comprehensive performance metrics"""
        if self.equity_curve is None or self.equity_curve.empty:
            return {}

        equity = self.equity_curve['equity'].values
        returns = np.diff(equity) / equity[:-1]

        # Extract test period dates
        start_date = None
        end_date = None
        if self.trades:
            timestamps = [t.get('timestamp_ms', 0) for t in self.trades if t.get('timestamp_ms', 0) > 0]
            if timestamps:
                first_ts = min(timestamps)
                last_ts = max(timestamps)
                # Convert to ET timezone
                start_dt = datetime.fromtimestamp(first_ts / 1000, tz=timezone.utc).astimezone(pytz.timezone('America/New_York'))
                end_dt = datetime.fromtimestamp(last_ts / 1000, tz=timezone.utc).astimezone(pytz.timezone('America/New_York'))
                start_date = start_dt.strftime('%b %d, %Y')
                end_date = end_dt.strftime('%b %d, %Y')

        # Calculate number of blocks and trading days
        num_blocks = 0
        num_trading_days = 0
        if self.market_data is not None and not self.market_data.empty:
            # Count unique days in market data
            if 'datetime' in self.market_data.columns:
                dates = pd.to_datetime(self.market_data['datetime']).dt.date
                num_trading_days = dates.nunique()
                # Calculate blocks: 480 bars per block, count total bars
                total_bars = len(self.market_data)
                num_blocks = max(1, round(total_bars / 480))

        # Basic metrics
        total_return = (equity[-1] - equity[0]) / equity[0] * 100
        total_trades = len(self.trades)

        # Calculate winning/losing trades from equity changes
        winning_trades = 0
        losing_trades = 0
        for i in range(1, len(equity)):
            if equity[i] > equity[i-1]:
                winning_trades += 1
            elif equity[i] < equity[i-1]:
                losing_trades += 1

        # Risk metrics
        volatility = np.std(returns) * np.sqrt(252) * 100  # Annualized
        sharpe_ratio = np.mean(returns) / np.std(returns) * np.sqrt(252) if np.std(returns) > 0 else 0

        # Drawdown analysis
        peak = np.maximum.accumulate(equity)
        drawdown = (equity - peak) / peak * 100
        max_drawdown = np.min(drawdown)

        # Trade analysis - calculate PnL from equity changes
        equity_changes = np.diff(equity)
        avg_win = np.mean(equity_changes[equity_changes > 0]) if np.any(equity_changes > 0) else 0
        avg_loss = np.mean(equity_changes[equity_changes < 0]) if np.any(equity_changes < 0) else 0

        # Calculate MRB (Mean Return per Block)
        mrb = (total_return / num_blocks) if num_blocks > 0 else 0

        # Calculate daily trades
        num_daily_trades = (total_trades / num_trading_days) if num_trading_days > 0 else 0

        return {
            'total_return': total_return,
            'total_trades': total_trades,
            'winning_trades': winning_trades,
            'losing_trades': losing_trades,
            'win_rate': winning_trades / (winning_trades + losing_trades) * 100 if (winning_trades + losing_trades) > 0 else 0,
            'volatility': volatility,
            'sharpe_ratio': sharpe_ratio,
            'max_drawdown': max_drawdown,
            'avg_win': avg_win,
            'avg_loss': avg_loss,
            'profit_factor': abs(avg_win / avg_loss) if avg_loss != 0 else float('inf'),
            'equity_curve': equity,
            'drawdown': drawdown,
            'start_date': start_date,
            'end_date': end_date,
            'num_blocks': num_blocks,
            'mrb': mrb,
            'num_daily_trades': num_daily_trades
        }

    def _get_base_prices_for_trades(self, trades: List[Dict], market_data: pd.DataFrame) -> List[float]:
        """Get base ticker (SPY/QQQ) prices for trade timestamps for chart placement"""
        prices = []

        # Pre-convert market data datetime to ensure it's timezone-naive and sorted
        if not market_data.empty and 'datetime' in market_data.columns:
            market_times = pd.to_datetime(market_data['datetime'])
            if hasattr(market_times, 'dt') and market_times.dt.tz is not None:
                market_times = market_times.dt.tz_localize(None)

        for trade in trades:
            # Convert UTC timestamp to ET to match market data
            trade_time = pd.to_datetime(trade.get('timestamp_ms', 0), unit='ms') - pd.Timedelta(hours=4)

            # Find closest bar in market data
            if not market_data.empty and 'datetime' in market_data.columns:
                # Find the closest bar by time
                time_diffs = abs(market_times - trade_time)
                closest_idx = time_diffs.idxmin()

                # Use open price (matches when signal was generated and trade executed)
                base_price = float(market_data.loc[closest_idx, 'open'])
                prices.append(base_price)
            else:
                # Fallback to instrument price if no market data
                prices.append(trade.get('price', 0))

        return prices

    def create_candlestick_chart(self) -> go.Figure:
        """Create professional candlestick chart with trade overlays"""
        if self.market_data is None:
            print("❌ No market data available for candlestick chart")
            return None

        # Filter market data to trading period only
        if self.trades:
            # Parse trade timestamps - handle both string and millisecond formats
            trade_dates = []
            for t in self.trades:
                if 'timestamp' in t:
                    # String format from C++: "2025-10-07 09:30:00 America/New_York"
                    ts_str = t['timestamp'].replace(' America/New_York', '')
                    dt = pd.to_datetime(ts_str)
                    trade_dates.append(dt)
                elif 'timestamp_ms' in t:
                    # Millisecond timestamp
                    dt = pd.to_datetime(t['timestamp_ms'], unit='ms') - pd.Timedelta(hours=4)
                    trade_dates.append(dt)

            if trade_dates:
                first_dt = min(trade_dates)
                last_dt = max(trade_dates)
            else:
                # No valid timestamps, use all market data
                first_dt = self.market_data['datetime'].min()
                last_dt = self.market_data['datetime'].max()

            # Ensure market data datetime is also tz-naive
            if hasattr(self.market_data['datetime'], 'dt'):
                if self.market_data['datetime'].dt.tz is not None:
                    market_dt = self.market_data['datetime'].dt.tz_localize(None)
                else:
                    market_dt = self.market_data['datetime']
            else:
                market_dt = pd.to_datetime(self.market_data['datetime'])

            # Filter market data to ±1 day buffer around trading period
            buffer = pd.Timedelta(days=1)
            mask = (market_dt >= first_dt - buffer) & (market_dt <= last_dt + buffer)
            filtered_data = self.market_data[mask].copy()

            # Further filter to only show Regular Trading Hours (9:30 AM - 4:00 PM ET)
            if not filtered_data.empty and 'datetime' in filtered_data.columns:
                filtered_data['hour'] = pd.to_datetime(filtered_data['datetime']).dt.hour
                filtered_data['minute'] = pd.to_datetime(filtered_data['datetime']).dt.minute
                rth_mask = (
                    ((filtered_data['hour'] == 9) & (filtered_data['minute'] >= 30)) |
                    ((filtered_data['hour'] >= 10) & (filtered_data['hour'] < 16))
                )
                filtered_data = filtered_data[rth_mask].copy()
                filtered_data = filtered_data.drop(columns=['hour', 'minute'])

            print(f"📊 Filtered market data: {len(self.market_data)} → {len(filtered_data)} bars (RTH only)")
        else:
            filtered_data = self.market_data

        fig = make_subplots(
            rows=2, cols=1,
            shared_xaxes=True,
            vertical_spacing=0.08,
            subplot_titles=('Price Chart with Trades & Signals', 'Portfolio Value & P/L'),
            row_heights=[0.6, 0.4]
        )
        
        # Add SPY open and close prices as separate lines
        print(f"   Adding SPY price lines with {len(filtered_data)} bars")

        # Open price line (where trades execute)
        fig.add_trace(
            go.Scatter(
                x=filtered_data['datetime'].tolist(),
                y=filtered_data['open'].tolist(),
                mode='lines',
                name='SPY Open (trade price)',
                line=dict(color='#2E86DE', width=2),
                showlegend=True,
                connectgaps=False
            ),
            row=1, col=1
        )

        # Close price line for reference
        fig.add_trace(
            go.Scatter(
                x=filtered_data['datetime'].tolist(),
                y=filtered_data['close'].tolist(),
                mode='lines',
                name='SPY Close',
                line=dict(color='#999999', width=1, dash='dot'),
                showlegend=True,
                connectgaps=False,
                opacity=0.5
            ),
            row=1, col=1
        )
        
        # Add trade markers
        if self.trades:
            # Check both 'side' (C++) and 'action' (Python) fields
            buy_trades = [t for t in self.trades if t.get('side', t.get('action', '')).lower() == 'buy']
            sell_trades = [t for t in self.trades if t.get('side', t.get('action', '')).lower() == 'sell']

            # Buy trades (green triangles) with enhanced info
            if buy_trades:
                print(f"   Processing {len(buy_trades)} BUY trades for markers...")
                # Parse timestamps from C++ format
                buy_times = []
                for t in buy_trades:
                    if 'timestamp' in t:
                        ts_str = t['timestamp'].replace(' America/New_York', '')
                        buy_times.append(pd.to_datetime(ts_str))
                    elif 'timestamp_ms' in t:
                        buy_times.append(pd.to_datetime(t['timestamp_ms'], unit='ms') - pd.Timedelta(hours=4))
                print(f"   Parsed {len(buy_times)} BUY timestamps")

                # Get SPY price at trade time for Y-coordinate (so all trades appear on chart)
                buy_spy_prices = []
                buy_hover = []
                for t in buy_trades:
                    # Handle both C++ and Python field names
                    symbol = t.get('symbol', 'N/A')
                    price = t.get('filled_avg_price', t.get('price', 0))
                    quantity = t.get('filled_qty', t.get('quantity', 0))
                    trade_value = t.get('trade_value', price * quantity)
                    cash = t.get('cash_balance', 0)
                    portfolio = t.get('portfolio_value', 0)
                    trade_pnl = t.get('trade_pnl', 0.0)
                    reason = t.get('reason', 'N/A')
                    bar_idx = t.get('bar_index', 'N/A')

                    # Find SPY price at this trade's timestamp for chart positioning
                    trade_time = buy_times[len(buy_spy_prices)]  # Current trade's timestamp
                    closest_spy_price = filtered_data[filtered_data['datetime'] == trade_time]['close'].values
                    if len(closest_spy_price) > 0:
                        buy_spy_prices.append(closest_spy_price[0])
                    else:
                        # Fallback: find nearest time
                        time_diffs = abs(filtered_data['datetime'] - trade_time)
                        nearest_idx = time_diffs.idxmin()
                        buy_spy_prices.append(filtered_data.loc[nearest_idx, 'close'])

                    hover_text = (
                        f"<b>BUY {symbol}</b><br>" +
                        f"Bar: {bar_idx}<br>" +
                        f"Price: ${price:.2f}<br>" +
                        f"Qty: {quantity:.0f}<br>" +
                        f"Value: ${trade_value:,.2f}<br>" +
                        f"Cash: ${cash:,.2f}<br>" +
                        f"Portfolio: ${portfolio:,.2f}<br>" +
                        f"Trade P&L: ${trade_pnl:+.2f}<br>" +
                        f"Reason: {reason}"
                    )
                    buy_hover.append(hover_text)
                print(f"   Adding {len(buy_spy_prices)} BUY markers to chart")
                print(f"   BUY times range: {min(buy_times)} to {max(buy_times)}")
                print(f"   BUY prices range: ${min(buy_spy_prices):.2f} to ${max(buy_spy_prices):.2f}")
                fig.add_trace(
                    go.Scatter(
                        x=buy_times,
                        y=buy_spy_prices,
                        mode='markers',
                        marker=dict(symbol='triangle-up', size=20, color='#00ff00', line=dict(width=2, color='darkgreen')),
                        name='Buy Trades',
                        text=buy_hover,
                        hovertemplate='%{text}<extra></extra>'
                    ),
                    row=1, col=1
                )
            
            # Sell trades (red triangles) with enhanced info
            if sell_trades:
                print(f"   Processing {len(sell_trades)} SELL trades for markers...")
                # Parse timestamps from C++ format
                sell_times = []
                for t in sell_trades:
                    if 'timestamp' in t:
                        ts_str = t['timestamp'].replace(' America/New_York', '')
                        sell_times.append(pd.to_datetime(ts_str))
                    elif 'timestamp_ms' in t:
                        sell_times.append(pd.to_datetime(t['timestamp_ms'], unit='ms') - pd.Timedelta(hours=4))
                print(f"   Parsed {len(sell_times)} SELL timestamps")

                # Get SPY price at trade time for Y-coordinate (so all trades appear on chart)
                sell_spy_prices = []
                sell_hover = []
                for t in sell_trades:
                    # Handle both C++ and Python field names
                    symbol = t.get('symbol', 'N/A')
                    price = t.get('filled_avg_price', t.get('price', 0))
                    quantity = t.get('filled_qty', t.get('quantity', 0))
                    trade_value = t.get('trade_value', price * quantity)
                    cash = t.get('cash_balance', 0)
                    portfolio = t.get('portfolio_value', 0)
                    trade_pnl = t.get('trade_pnl', 0.0)
                    reason = t.get('reason', 'N/A')
                    bar_idx = t.get('bar_index', 'N/A')

                    # Find SPY price at this trade's timestamp for chart positioning
                    trade_time = sell_times[len(sell_spy_prices)]
                    closest_spy_price = filtered_data[filtered_data['datetime'] == trade_time]['close'].values
                    if len(closest_spy_price) > 0:
                        sell_spy_prices.append(closest_spy_price[0])
                    else:
                        # Fallback: find nearest time if exact match not found
                        time_diffs = abs(filtered_data['datetime'] - trade_time)
                        nearest_idx = time_diffs.idxmin()
                        sell_spy_prices.append(filtered_data.loc[nearest_idx, 'close'])

                    hover_text = (
                        f"<b>SELL {symbol}</b><br>" +
                        f"Bar: {bar_idx}<br>" +
                        f"Price: ${price:.2f}<br>" +
                        f"Qty: {quantity:.0f}<br>" +
                        f"Value: ${trade_value:,.2f}<br>" +
                        f"Cash: ${cash:,.2f}<br>" +
                        f"Portfolio: ${portfolio:,.2f}<br>" +
                        f"Trade P&L: ${trade_pnl:+.2f}<br>" +
                        f"Reason: {reason}"
                    )
                    sell_hover.append(hover_text)
                print(f"   Adding {len(sell_spy_prices)} SELL markers to chart")
                print(f"   SELL times range: {min(sell_times)} to {max(sell_times)}")
                print(f"   SELL prices range: ${min(sell_spy_prices):.2f} to ${max(sell_spy_prices):.2f}")
                fig.add_trace(
                    go.Scatter(
                        x=sell_times,
                        y=sell_spy_prices,
                        mode='markers',
                        marker=dict(symbol='triangle-down', size=20, color='#ff0000', line=dict(width=2, color='darkred')),
                        name='Sell Trades',
                        text=sell_hover,
                        hovertemplate='%{text}<extra></extra>'
                    ),
                    row=1, col=1
                )

        # Portfolio value chart (row 2)
        if self.equity_curve is not None and not self.equity_curve.empty:
            print(f"   Adding portfolio value line with {len(self.equity_curve)} points")
            # Timestamps are already parsed correctly in _calculate_equity_curve
            equity_times = self.equity_curve['timestamp']
            print(f"   Equity curve time range (ET): {equity_times.min()} to {equity_times.max()}")
            print(f"   Equity value range: ${self.equity_curve['equity'].min():,.2f} to ${self.equity_curve['equity'].max():,.2f}")

            fig.add_trace(
                go.Scatter(
                    x=equity_times.tolist(),
                    y=self.equity_curve['equity'].tolist(),
                    mode='lines+markers',
                    name='Portfolio Value (at trades)',
                    line=dict(color='#EE5A6F', width=2, shape='hv'),  # 'hv' = step plot
                    marker=dict(size=6, color='#EE5A6F'),
                    connectgaps=False,
                    hovertemplate='<b>Portfolio</b><br>Time: %{x}<br>Value: $%{y:,.2f}<extra></extra>'
                ),
                row=2, col=1
            )

            # Set Y-axis range to show only the variation (not from zero)
            equity_values = self.equity_curve['equity'].values
            min_equity = np.min(equity_values)
            max_equity = np.max(equity_values)
            range_padding = (max_equity - min_equity) * 0.1  # 10% padding
            fig.update_yaxes(
                range=[min_equity - range_padding, max_equity + range_padding],
                row=2, col=1
            )

            # Add starting equity reference line
            fig.add_hline(
                y=self.start_equity,
                line_dash="dash",
                line_color="gray",
                opacity=0.5,
                row=2, col=1,
                annotation_text=f"Start: ${self.start_equity:,.0f}",
                annotation_position="right"
            )

        # Update layout - show all data without scrollbars
        fig.update_layout(
            title={
                'text': f'OnlineEnsemble Trading Analysis - {len(self.trades)} Trades (RTH Only)',
                'x': 0.5,
                'xanchor': 'center'
            },
            xaxis_rangeslider_visible=False,  # Disable horizontal scrollbar
            height=900,
            showlegend=True,
            template='plotly_white',
            hovermode='closest'  # Show closest point on hover
        )

        # Show full trading day (no range restriction)
        # All data visible without scrolling

        # Configure x-axes to hide non-trading hours (removes overnight gaps)
        fig.update_xaxes(
            rangebreaks=[
                dict(bounds=[16, 9.5], pattern="hour"),  # Hide 4pm-9:30am
            ]
        )

        # Update axes labels
        fig.update_yaxes(title_text="Price ($)", row=1, col=1)
        fig.update_yaxes(title_text="Portfolio Value ($)", row=2, col=1)
        fig.update_xaxes(title_text="Date/Time (ET)", row=2, col=1)

        # Format x-axis to show time labels in ET timezone
        fig.update_xaxes(
            tickformat='%H:%M',  # Show time as HH:MM
            dtick=1800000,  # Tick every 30 minutes (in milliseconds)
            tickangle=0,
            tickfont=dict(size=10)
        )

        # Set Y-axis range for price chart to focus on actual price range
        if not filtered_data.empty:
            price_min = filtered_data['low'].min()
            price_max = filtered_data['high'].max()
            price_range = price_max - price_min
            padding = price_range * 0.05  # 5% padding
            fig.update_yaxes(
                range=[price_min - padding, price_max + padding],
                row=1, col=1
            )

        return fig
    
    def create_equity_curve_chart(self) -> go.Figure:
        """Create equity curve with drawdown analysis"""
        if self.equity_curve is None:
            print("❌ No equity curve data available")
            return None
            
        fig = make_subplots(
            rows=2, cols=1,
            shared_xaxes=True,
            vertical_spacing=0.1,
            subplot_titles=('Equity Curve', 'Drawdown'),
            row_heights=[0.7, 0.3]
        )
        
        # Equity curve
        fig.add_trace(
            go.Scatter(
                x=self.equity_curve['timestamp'],
                y=self.equity_curve['equity'],
                mode='lines',
                name='Equity',
                line=dict(color='blue', width=2),
                hovertemplate='<b>Equity</b><br>Time: %{x}<br>Value: $%{y:,.2f}<extra></extra>'
            ),
            row=1, col=1
        )
        
        # Drawdown
        if 'drawdown' in self.performance_metrics:
            fig.add_trace(
                go.Scatter(
                    x=self.equity_curve['timestamp'],
                    y=self.performance_metrics['drawdown'],
                    mode='lines',
                    name='Drawdown',
                    line=dict(color='red', width=2),
                    fill='tonexty',
                    fillcolor='rgba(255,0,0,0.3)',
                    hovertemplate='<b>Drawdown</b><br>Time: %{x}<br>Drawdown: %{y:.2f}%<extra></extra>'
                ),
                row=2, col=1
            )
        
        # Update layout
        fig.update_layout(
            title='Equity Curve and Drawdown Analysis',
            height=600,
            showlegend=True,
            template='plotly_white'
        )
        
        return fig
    
    def create_pnl_chart(self) -> go.Figure:
        """Create trade-by-trade P&L chart"""
        if not self.trades:
            print("❌ No trades available for P&L chart")
            return None
            
        pnls = [t.get('pnl', t.get('profit_loss', 0)) for t in self.trades]
        trade_numbers = list(range(1, len(pnls) + 1))
        
        # Color bars based on profit/loss
        colors = ['green' if pnl > 0 else 'red' for pnl in pnls]
        
        fig = go.Figure()
        
        fig.add_trace(
            go.Bar(
                x=trade_numbers,
                y=pnls,
                marker_color=colors,
                name='P&L',
                hovertemplate='<b>Trade %{x}</b><br>P&L: $%{y:,.2f}<extra></extra>'
            )
        )
        
        # Add cumulative P&L line
        cumulative_pnl = np.cumsum(pnls)
        fig.add_trace(
            go.Scatter(
                x=trade_numbers,
                y=cumulative_pnl,
                mode='lines',
                name='Cumulative P&L',
                line=dict(color='blue', width=2),
                hovertemplate='<b>Cumulative P&L</b><br>Trade: %{x}<br>Total: $%{y:,.2f}<extra></extra>'
            )
        )
        
        fig.update_layout(
            title='Trade-by-Trade P&L Analysis',
            xaxis_title='Trade Number',
            yaxis_title='P&L ($)',
            height=500,
            template='plotly_white'
        )
        
        return fig
    
    def create_performance_dashboard(self) -> go.Figure:
        """Create comprehensive performance metrics dashboard"""
        if not self.performance_metrics:
            print("❌ No performance metrics available")
            return None
            
        metrics = self.performance_metrics
        
        # Create subplots for different metric categories
        fig = make_subplots(
            rows=2, cols=2,
            subplot_titles=('Returns', 'Risk Metrics', 'Trade Statistics', 'Performance Summary'),
            specs=[[{"type": "indicator"}, {"type": "indicator"}],
                   [{"type": "indicator"}, {"type": "indicator"}]]
        )
        
        # Returns
        fig.add_trace(
            go.Indicator(
                mode="number+delta",
                value=metrics['total_return'],
                number={'suffix': '%'},
                title={'text': "Total Return"},
                delta={'reference': 0}
            ),
            row=1, col=1
        )
        
        # Risk metrics
        fig.add_trace(
            go.Indicator(
                mode="number",
                value=metrics['max_drawdown'],
                number={'suffix': '%'},
                title={'text': "Max Drawdown"}
            ),
            row=1, col=2
        )
        
        # Trade statistics
        fig.add_trace(
            go.Indicator(
                mode="number",
                value=metrics['win_rate'],
                number={'suffix': '%'},
                title={'text': "Win Rate"}
            ),
            row=2, col=1
        )
        
        # Performance summary
        fig.add_trace(
            go.Indicator(
                mode="number",
                value=metrics['sharpe_ratio'],
                number={'valueformat': '.2f'},
                title={'text': "Sharpe Ratio"}
            ),
            row=2, col=2
        )
        
        fig.update_layout(
            title='Performance Metrics Dashboard',
            height=600,
            template='plotly_white'
        )
        
        return fig
    
    def _parse_timestamp(self, timestamp_str: str) -> datetime:
        """Parse timestamp string to datetime"""
        try:
            # Try different timestamp formats
            if timestamp_str.isdigit():
                return datetime.fromtimestamp(int(timestamp_str), tz=timezone.utc)
            else:
                return datetime.fromisoformat(timestamp_str.replace('Z', '+00:00'))
        except:
            return datetime.now()
    
    def generate_dashboard(self, output_file: str = "professional_trading_dashboard.html"):
        """Generate focused trading dashboard with candlestick and P/L only"""
        print("🚀 Generating professional trading dashboard...")

        # Create focused charts only
        charts = {}

        # Candlestick chart (main chart with trades)
        candlestick_fig = self.create_candlestick_chart()
        if candlestick_fig:
            charts['candlestick'] = candlestick_fig

        # Generate HTML dashboard
        html_content = self._generate_html_dashboard(charts)
        
        # Save to file
        with open(output_file, 'w', encoding='utf-8') as f:
            f.write(html_content)
        
        print(f"✅ Professional trading dashboard saved to: {output_file}")
        return output_file
    
    def _generate_html_dashboard(self, charts: Dict[str, go.Figure]) -> str:
        """Generate HTML dashboard with all charts"""
        html_parts = []
        
        # HTML header
        html_parts.append("""
<!DOCTYPE html>
<html>
<head>
    <title>Professional Trading Dashboard</title>
    <script src="https://cdn.plot.ly/plotly-latest.min.js"></script>
    <style>
        body { font-family: 'Segoe UI', Arial, sans-serif; margin: 0; padding: 0; background-color: #f5f5f5; }
        .dashboard { max-width: 100%; margin: 0 auto; }
        .chart-container { background: white; margin: 20px; padding: 20px; border-radius: 8px; box-shadow: 0 2px 4px rgba(0,0,0,0.1); }

        /* Top header bar - green background with key metrics */
        .header-metrics {
            background: linear-gradient(to bottom, #4CAF50 0%, #45a049 100%);
            padding: 20px;
            display: flex;
            justify-content: space-around;
            align-items: center;
            color: white;
            box-shadow: 0 2px 4px rgba(0,0,0,0.2);
        }
        .header-metric {
            text-align: center;
        }
        .header-metric-label {
            font-size: 11px;
            text-transform: uppercase;
            opacity: 0.9;
            margin-bottom: 5px;
        }
        .header-metric-value {
            font-size: 24px;
            font-weight: bold;
        }
        .positive { color: #4CAF50; }
        .negative { color: #f44336; }

        /* End of Day Summary box */
        .eod-summary {
            background: white;
            margin: 20px;
            padding: 20px;
            border-radius: 8px;
            border-left: 4px solid #2196F3;
            box-shadow: 0 2px 4px rgba(0,0,0,0.1);
        }
        .eod-summary h3 {
            margin-top: 0;
            color: #2c3e50;
            font-size: 18px;
            border-bottom: 1px solid #e0e0e0;
            padding-bottom: 10px;
        }
        .eod-row {
            display: flex;
            justify-content: space-between;
            padding: 8px 0;
            border-bottom: 1px solid #f0f0f0;
        }
        .eod-row:last-child {
            border-bottom: none;
            font-weight: bold;
        }
        .eod-label {
            color: #666;
        }
        .eod-value {
            font-family: 'Courier New', monospace;
            font-weight: 600;
        }

        h2 { color: #34495e; border-bottom: 2px solid #3498db; padding-bottom: 10px; margin: 20px; }

        /* JP Morgan style trade table */
        .trade-table {
            width: 100%;
            border-collapse: collapse;
            font-family: 'Segoe UI', Arial, sans-serif;
            font-size: 13px;
            margin-top: 10px;
        }
        .trade-table thead {
            background: linear-gradient(to bottom, #f8f9fa 0%, #e9ecef 100%);
            border-top: 2px solid #003d82;
            border-bottom: 2px solid #003d82;
        }
        .trade-table th {
            padding: 12px 10px;
            text-align: left;
            font-weight: 600;
            color: #003d82;
            border-right: 1px solid #dee2e6;
        }
        .trade-table th:last-child { border-right: none; }
        .trade-table tbody tr {
            border-bottom: 1px solid #e9ecef;
            transition: background-color 0.2s;
        }
        .trade-table tbody tr:hover {
            background-color: #f8f9fa;
        }
        .trade-table tbody tr:nth-child(even) {
            background-color: #fdfdfd;
        }
        .trade-table td {
            padding: 10px;
            color: #212529;
            border-right: 1px solid #f1f3f5;
        }
        .trade-table td:last-child { border-right: none; }
        .trade-table .time {
            font-size: 11px;
            color: #6c757d;
        }
        .trade-table .symbol {
            font-weight: 600;
            color: #003d82;
        }
        .trade-table .action-buy {
            color: #28a745;
            font-weight: 600;
        }
        .trade-table .action-sell {
            color: #dc3545;
            font-weight: 600;
        }
        .trade-table .number {
            text-align: right;
            font-family: 'Courier New', monospace;
        }
        .trade-table .portfolio-value {
            text-align: right;
            font-family: 'Courier New', monospace;
            font-weight: bold;
            color: #003d82;
        }
        .trade-table .reason {
            font-size: 11px;
            color: #6c757d;
        }
        .trade-table .profit {
            color: #28a745;
            font-weight: 600;
        }
        .trade-table .loss {
            color: #dc3545;
            font-weight: 600;
        }
    </style>
</head>
<body>
    <div class="dashboard">
        """)

        # Top header bar with key metrics
        if self.performance_metrics:
            final_value = self.start_equity * (1 + self.performance_metrics.get('total_return', 0) / 100)
            total_pnl = final_value - self.start_equity
            roi = self.performance_metrics.get('total_return', 0)
            win_rate = self.performance_metrics.get('win_rate', 0)
            max_dd = self.performance_metrics.get('max_drawdown', 0)

            header_html = f"""
        <div class="header-metrics">
            <div class="header-metric">
                <div class="header-metric-label">Starting Equity</div>
                <div class="header-metric-value">${self.start_equity:,.0f}</div>
            </div>
            <div class="header-metric">
                <div class="header-metric-label">Final Value</div>
                <div class="header-metric-value">${final_value:,.0f}</div>
            </div>
            <div class="header-metric">
                <div class="header-metric-label">Total P&L</div>
                <div class="header-metric-value">${total_pnl:+,.0f}</div>
            </div>
            <div class="header-metric">
                <div class="header-metric-label">ROI</div>
                <div class="header-metric-value">{roi:+.4f}%</div>
            </div>
            <div class="header-metric">
                <div class="header-metric-label">Win Rate</div>
                <div class="header-metric-value">{win_rate:.1f}%</div>
            </div>
            <div class="header-metric">
                <div class="header-metric-label">Max Drawdown</div>
                <div class="header-metric-value">{max_dd:.2f}%</div>
            </div>
        </div>
            """
            html_parts.append(header_html)

            # End of Day Summary box
            final_cash = self.equity_curve['cash'].iloc[-1] if len(self.equity_curve) > 0 else self.start_equity
            final_portfolio = self.equity_curve['portfolio_value'].iloc[-1] if len(self.equity_curve) > 0 else self.start_equity
            total_return_pct = ((final_portfolio - self.start_equity) / self.start_equity) * 100

            eod_html = f"""
        <div class="eod-summary">
            <h3>📋 End of Day Summary</h3>
            <div class="eod-row">
                <span class="eod-label">Final Cash:</span>
                <span class="eod-value">${final_cash:,.2f}</span>
            </div>
            <div class="eod-row">
                <span class="eod-label">Final Portfolio Value:</span>
                <span class="eod-value">${final_portfolio:,.2f}</span>
            </div>
            <div class="eod-row">
                <span class="eod-label">Total Return:</span>
                <span class="eod-value {'positive' if total_return_pct >= 0 else 'negative'}">${total_pnl:+,.2f} ({total_return_pct:+.4f}%)</span>
            </div>
        </div>
            """
            html_parts.append(eod_html)
        
        # Add charts
        for chart_name, fig in charts.items():
            html_parts.append(f"""
        <div class="chart-container">
            <h2>📊 {chart_name.title()} Chart</h2>
            <div id="{chart_name}-chart"></div>
        </div>
        """)

        # Add trade statement table (JP Morgan style)
        if self.trades:
            html_parts.append(f"""
        <div class="chart-container">
            <h2>📋 Trade Statement ({len(self.trades)} Trades)</h2>
            <table class="trade-table">
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
                <tbody>
            """)

            cumulative_pnl = 0.0
            for idx, trade in enumerate(self.trades, 1):
                # Format timestamp - handle both formats
                if 'timestamp' in trade:
                    # String timestamp from C++ (e.g., "2025-10-07 09:30:00 America/New_York")
                    ts_str = trade['timestamp']
                    # Parse the timestamp
                    try:
                        # Split off timezone if present
                        if ' America/New_York' in ts_str:
                            ts_str = ts_str.replace(' America/New_York', '')
                        dt_et = datetime.strptime(ts_str, '%Y-%m-%d %H:%M:%S')
                        date_str = dt_et.strftime('%b %d')
                        time_str = dt_et.strftime('%H:%M:%S')
                    except:
                        date_str = 'N/A'
                        time_str = 'N/A'
                elif 'timestamp_ms' in trade:
                    # Millisecond timestamp
                    ts_ms = trade['timestamp_ms']
                    dt = datetime.fromtimestamp(ts_ms / 1000, tz=timezone.utc)
                    dt_et = dt.astimezone(pytz.timezone('America/New_York'))
                    date_str = dt_et.strftime('%b %d')
                    time_str = dt_et.strftime('%H:%M:%S')
                else:
                    date_str = 'N/A'
                    time_str = 'N/A'

                # Format action with color - handle both 'side' (C++) and 'action' (Python)
                action = trade.get('side', trade.get('action', 'N/A')).upper()
                action_class = 'buy' if action == 'BUY' else 'sell'

                # Format values - handle both C++ and Python formats
                symbol = trade.get('symbol', 'N/A')
                quantity = trade.get('filled_qty', trade.get('quantity', 0))
                price = trade.get('filled_avg_price', trade.get('price', 0))
                trade_value = trade.get('trade_value', price * abs(quantity) if price and quantity else 0)
                cash_balance = trade.get('cash_balance', 0)
                portfolio_value = trade.get('portfolio_value', 0)
                reason = trade.get('reason', 'N/A')
                bar_index = trade.get('bar_index', idx - 1)

                # Calculate trade P&L
                trade_pnl = trade.get('trade_pnl', 0.0)
                cumulative_pnl += trade_pnl

                # Format P&L with color
                trade_pnl_class = 'profit' if trade_pnl >= 0 else 'loss'
                cum_pnl_class = 'profit' if cumulative_pnl >= 0 else 'loss'

                html_parts.append(f"""
                    <tr>
                        <td class="number">{idx}</td>
                        <td class="number">{bar_index}</td>
                        <td>{date_str}<br><span class="time">{time_str}</span></td>
                        <td class="symbol">{symbol}</td>
                        <td class="action-{action_class}">{action}</td>
                        <td class="number">{quantity:.0f}</td>
                        <td class="number">{price:.2f}</td>
                        <td class="number">{trade_value:,.2f}</td>
                        <td class="number">{cash_balance:,.2f}</td>
                        <td class="portfolio-value">{portfolio_value:,.2f}</td>
                        <td class="number {trade_pnl_class}">{trade_pnl:+.2f}</td>
                        <td class="number {cum_pnl_class}">{cumulative_pnl:+.2f}</td>
                        <td class="reason">{reason}</td>
                    </tr>
                """)

            html_parts.append("""
                </tbody>
            </table>
        </div>
            """)
        
        # Add JavaScript for charts - use simple, direct approach
        html_parts.append("""
        <script>
        """)

        for chart_name, fig in charts.items():
            # Use Plotly's built-in JSON encoder which handles numpy arrays
            from plotly.io import to_json
            fig_json_str = to_json(fig)

            html_parts.append(f"""
            // Render {chart_name} chart
            var figData_{chart_name} = {fig_json_str};
            Plotly.newPlot(
                '{chart_name}-chart',
                figData_{chart_name}.data,
                figData_{chart_name}.layout,
                {{responsive: true}}
            );
            """)

        html_parts.append("""
        </script>
    </div>
</body>
</html>
        """)
        
        return ''.join(html_parts)


def main():
    parser = argparse.ArgumentParser(
        description="Professional Trading Visualization Dashboard"
    )
    parser.add_argument("--tradebook", required=True, help="Path to trade book JSONL file")
    parser.add_argument("--signals", help="Path to signals JSONL file (optional, for probability info)")
    parser.add_argument("--data", default="data/equities/QQQ_RTH_NH.csv", help="Market data CSV file")
    parser.add_argument("--output", default="professional_trading_dashboard.html", help="Output HTML file")
    parser.add_argument("--start-equity", type=float, default=100000.0, help="Starting equity")

    args = parser.parse_args()
    
    # Validate inputs
    if not os.path.exists(args.tradebook):
        print(f"❌ Trade book not found: {args.tradebook}")
        return 1
    
    # Create dashboard
    dashboard = TradingDashboard(args.tradebook, args.data, args.signals, args.start_equity)
    
    try:
        dashboard.load_data()
        dashboard.generate_dashboard(args.output)
        print(f"🎉 Professional trading dashboard generated successfully!")
        print(f"📊 Open {args.output} in your browser to view the dashboard")
        return 0
    except Exception as e:
        print(f"❌ Error generating dashboard: {e}")
        return 1


if __name__ == "__main__":
    sys.exit(main())
