#!/usr/bin/env python3
import argparse
import json
import os
import sys
from datetime import datetime, timezone
from typing import List, Dict, Any, Tuple


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Visualize a Sentio trade book JSONL as equity curve and trade timeline (interactive HTML)"
    )
    parser.add_argument("--tradebook", required=True, help="Path to trade book JSONL (…_trades.jsonl)")
    parser.add_argument("--out", default="tradebook_visualization.html", help="Output HTML file")
    parser.add_argument("--data", default="data/equities/QQQ_RTH_NH.csv", help="Market data CSV to overlay (QQQ)")
    parser.add_argument("--start-equity", type=float, default=100000.0, help="Starting equity if equity_after is absent")
    parser.add_argument("--tz", default="UTC", help="Timezone label for display (no conversion performed)")
    return parser.parse_args()


def load_tradebook(path: str) -> List[Dict[str, Any]]:
    trades: List[Dict[str, Any]] = []
    with open(path, "r", encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            try:
                trades.append(json.loads(line))
            except json.JSONDecodeError:
                # Skip malformed lines gracefully
                continue
    return trades


def to_epoch_ms(dt_str: str) -> int:
    # Best-effort parse of common formats like "MM/DD HH:MM:SS" or ISO strings
    # If parsing fails, return 0 to maintain order
    fmts = [
        "%Y-%m-%dT%H:%M:%S.%fZ",
        "%Y-%m-%dT%H:%M:%SZ",
        "%Y-%m-%d %H:%M:%S",
        "%m/%d %H:%M:%S",
        "%m/%d %H:%M",
    ]
    for fmt in fmts:
        try:
            dt = datetime.strptime(dt_str, fmt)
            if dt.tzinfo is None:
                dt = dt.replace(tzinfo=timezone.utc)
            return int(dt.timestamp() * 1000)
        except Exception:
            pass
    return 0


def extract_series(trades: List[Dict[str, Any]], start_equity: float) -> Tuple[List[int], List[float], List[Dict[str, Any]]]:
    times: List[int] = []
    equities: List[float] = []
    trade_points: List[Dict[str, Any]] = []

    equity = start_equity
    have_equity_after = any("equity_after" in t for t in trades)

    for t in trades:
        # Timestamp resolution
        if "timestamp_ms" in t:
            ts = int(t["timestamp_ms"])  # already epoch ms
        elif "timestamp" in t:
            ts = int(t["timestamp"]) if str(t["timestamp"]).isdigit() else to_epoch_ms(str(t["timestamp"]))
        elif "datetime" in t:
            ts = to_epoch_ms(str(t["datetime"]))
        else:
            ts = 0

        # Equity series
        if have_equity_after and "equity_after" in t:
            try:
                equity = float(t["equity_after"])
            except Exception:
                pass
        else:
            # Fallback accumulate realized pnl if present
            pnl = 0.0
            if "realized_pnl" in t:
                try:
                    pnl = float(t["realized_pnl"])
                except Exception:
                    pnl = 0.0
            equity += pnl

        times.append(ts)
        equities.append(equity)

        # Trade markers (include instrument details)
        instrument = str(t.get("symbol", t.get("instrument", "")))
        def _to_float(val):
            try:
                return float(val)
            except Exception:
                return None

        trade_points.append({
            "timestamp_ms": ts,
            "y": equity,
            "action": str(t.get("action", "")),
            "instrument": instrument,
            "price": _to_float(t.get("price", None)),
            "quantity": _to_float(t.get("quantity", None)),
            "realized_pnl": _to_float(t.get("realized_pnl", 0.0)) or 0.0,
        })

    return times, equities, trade_points


def render_plotly_html(times: List[int], equities: List[float], trades: List[Dict[str, Any]], tz_label: str, ohlc: List[Dict[str, Any]] | None = None) -> str:
    # We avoid requiring plotly python; emit a self-contained HTML using plotly.js from CDN
    # Build JS arrays
    x_vals = times
    y_vals = equities

    # Separate buy vs sell markers
    buys = [tp for tp in trades if tp["action"].lower().startswith("buy")]
    sells = [tp for tp in trades if tp["action"].lower().startswith("sell")]

    def js_array(objs: List[Any]) -> str:
        return json.dumps(objs, separators=(",", ":"))

    # Determine visible range from trade timestamps
    valid_times = [t for t in x_vals if t and t > 0]
    if valid_times:
        tmin = min(valid_times)
        tmax = max(valid_times)
        pad = max(60_000, int((tmax - tmin) * 0.02))  # >=1min padding
        xr_start = tmin - pad
        xr_end = tmax + pad
    else:
        xr_start, xr_end = 0, 0

    # Trim OHLC to visible range
    if ohlc:
        if xr_start and xr_end:
            ohlc = [b for b in ohlc if xr_start <= int(b.get("t", 0)) <= xr_end]
    ohlc = ohlc or []
    # Compute y-axis ranges for price and equity for full-screen mapping
    def _minmax(arr: List[float], pad_ratio: float = 0.02):
        if not arr:
            return 0.0, 1.0
        vmin = min(arr)
        vmax = max(arr)
        if vmin == vmax:
            vmin -= 1.0
            vmax += 1.0
        pad = max(1e-6, (vmax - vmin) * pad_ratio)
        return vmin - pad, vmax + pad

    price_series = [float(b.get("c", 0.0)) for b in ohlc]
    px_min, px_max = _minmax(price_series, 0.03)
    eq_min, eq_max = _minmax(equities, 0.03)

    # Map trade markers to price axis using close price at the nearest bar
    ohlc_times = [int(b.get("t", 0)) for b in ohlc]
    ohlc_closes = price_series
    def _lookup_px(ts: int) -> float:
        if not ohlc_times:
            return 0.0
        # binary search for nearest index
        import bisect
        i = bisect.bisect_left(ohlc_times, ts)
        if i == 0:
            return ohlc_closes[0]
        if i >= len(ohlc_times):
            return ohlc_closes[-1]
        # pick closer of i-1 and i
        before_t, after_t = ohlc_times[i-1], ohlc_times[i]
        return ohlc_closes[i-1] if abs(ts - before_t) <= abs(after_t - ts) else ohlc_closes[i]

    buy_px = [
        {"t": tp["timestamp_ms"], "p": _lookup_px(tp["timestamp_ms"])}
        for tp in trades if tp["action"].lower().startswith("buy")
    ]
    sell_px = [
        {"t": tp["timestamp_ms"], "p": _lookup_px(tp["timestamp_ms"])}
        for tp in trades if tp["action"].lower().startswith("sell")
    ]

    ohlc_js = json.dumps(ohlc, separators=(",", ":"))
    buy_px_js = json.dumps(buy_px, separators=(",", ":"))
    sell_px_js = json.dumps(sell_px, separators=(",", ":"))
    html = f"""
<!doctype html>
<html>
  <head>
    <meta charset=\"utf-8\" />
    <meta name=\"viewport\" content=\"width=device-width, initial-scale=1\" />
    <title>Tradebook Visualization</title>
    <script src=\"https://cdn.plot.ly/plotly-2.35.2.min.js\"></script>
    <style>
      body {{ font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, Helvetica, Arial, sans-serif; margin: 0; padding: 0; }}
      #plot {{ width: 100%; height: 96vh; }}
    </style>
  </head>
  <body>
    <div id=\"plot\"></div>
    <script>
      const x = {js_array(x_vals)};  // epoch ms
      const y = {js_array(y_vals)};
      const buys = {js_array(buys)};
      const sells = {js_array(sells)};

      const ohlc = {ohlc_js};
      const buyPx = {buy_px_js};
      const sellPx = {sell_px_js};

      const equityTrace = {{
        x: x.map(ts => new Date(ts)),
        y: y,
        mode: 'lines',
        name: 'Equity',
        line: {{color: '#1f77b4'}},
        yaxis: 'y2',
        hovertemplate: '%{{x|%Y-%m-%d %H:%M:%S}} ({tz_label})<br>Equity: $%{{y:.2f}}<extra></extra>'
      }};

      const priceTrace = {{
        type: 'candlestick',
        x: ohlc.map(b => new Date(b.t)),
        open: ohlc.map(b => b.o),
        high: ohlc.map(b => b.h),
        low: ohlc.map(b => b.l),
        close: ohlc.map(b => b.c),
        name: 'QQQ',
        increasing: {{ line: {{ color: '#2ca02c' }} }},
        decreasing: {{ line: {{ color: '#d62728' }} }},
        yaxis: 'y1',
        opacity: 0.4,
        hoverinfo: 'skip'
      }};

      const buyTrace = {{
        x: buys.map(p => new Date(p.timestamp_ms)),
        y: buys.map(p => p.y),
        mode: 'markers',
        name: 'BUY',
        marker: {{ color: '#2ca02c', size: 8, symbol: 'triangle-up' }},
        customdata: buys.map(p => [p.instrument, p.price, p.quantity, p.realized_pnl]),
        hovertemplate: 'BUY %{{x|%Y-%m-%d %H:%M:%S}} ({tz_label})' +
                       '<br>Instrument: %{{customdata[0]}}' +
                       '<br>Price: $%{{customdata[1]:.2f}}  Qty: %{{customdata[2]:.4f}}' +
                       '<br>Realized PnL: $%{{customdata[3]:.2f}}' +
                       '<br>Equity: $%{{y:.2f}}<extra></extra>'
      }};

      const sellTrace = {{
        x: sells.map(p => new Date(p.timestamp_ms)),
        y: sells.map(p => p.y),
        mode: 'markers',
        name: 'SELL',
        marker: {{ color: '#d62728', size: 8, symbol: 'triangle-down' }},
        customdata: sells.map(p => [p.instrument, p.price, p.quantity, p.realized_pnl]),
        hovertemplate: 'SELL %{{x|%Y-%m-%d %H:%M:%S}} ({tz_label})' +
                       '<br>Instrument: %{{customdata[0]}}' +
                       '<br>Price: $%{{customdata[1]:.2f}}  Qty: %{{customdata[2]:.4f}}' +
                       '<br>Realized PnL: $%{{customdata[3]:.2f}}' +
                       '<br>Equity: $%{{y:.2f}}<extra></extra>'
      }};

      // Price-axis buy/sell markers (at QQQ price level)
      const buyPxTrace = {{
        x: buyPx.map(b => new Date(b.t)),
        y: buyPx.map(b => b.p),
        mode: 'markers',
        name: 'BUY@Price',
        yaxis: 'y1',
        marker: {{ color: '#2ca02c', size: 9, symbol: 'triangle-up' }},
        hovertemplate: 'BUY %{{x|%Y-%m-%d %H:%M:%S}} ({tz_label})<br>QQQ: $%{{y:.2f}}<extra></extra>'
      }};
      const sellPxTrace = {{
        x: sellPx.map(b => new Date(b.t)),
        y: sellPx.map(b => b.p),
        mode: 'markers',
        name: 'SELL@Price',
        yaxis: 'y1',
        marker: {{ color: '#d62728', size: 9, symbol: 'triangle-down' }},
        hovertemplate: 'SELL %{{x|%Y-%m-%d %H:%M:%S}} ({tz_label})<br>QQQ: $%{{y:.2f}}<extra></extra>'
      }};

      const annotations = [];
      if (buys.length + sells.length === 0) {{
        annotations.push({{
          xref: 'paper', yref: 'paper', x: 0.5, y: 0.5,
          text: 'No executed trades in this run', showarrow: false,
          font: {{ size: 16, color: '#888' }}
        }});
      }}

      const xrStart = {xr_start};
      const xrEnd = {xr_end};
      const xaxisCfg = {{ title: 'Time', type: 'date', rangeslider: {{ visible: false }} }};
      if (xrStart && xrEnd && xrEnd > xrStart) {{
        xaxisCfg.range = [new Date(xrStart), new Date(xrEnd)];
      }}

      const layout = {{
        title: 'Equity Curve and Trade Timeline',
        xaxis: xaxisCfg,
        yaxis: {{ title: 'QQQ Price', side: 'right', domain: [0.0, 1.0], fixedrange: false, range: [{px_min}, {px_max}] }},
        yaxis2: {{ title: 'Equity ($)', overlaying: 'y', side: 'left', fixedrange: false, range: [{eq_min}, {eq_max}] }},
        hovermode: 'x unified',
        legend: {{ orientation: 'h', y: -0.1 }},
        margin: {{ t: 48, r: 16, b: 64, l: 64 }},
        annotations
      }};

      const series = [priceTrace, buyPxTrace, sellPxTrace, equityTrace, buyTrace, sellTrace];
      Plotly.newPlot('plot', series, layout, {{ responsive: true }});
    </script>
  </body>
  </html>
    """
    return html


def main() -> int:
    args = parse_args()
    if not os.path.exists(args.tradebook):
        print(f"❌ Trade book not found: {args.tradebook}", file=sys.stderr)
        return 1

    trades = load_tradebook(args.tradebook)
    if not trades:
        print("❌ No trades found in trade book (or file empty).", file=sys.stderr)
        return 1

    times, equities, points = extract_series(trades, args.start_equity)

    # Optional: load OHLC from CSV for background overlay
    ohlc: List[Dict[str, Any]] = []
    if os.path.exists(args.data):
        try:
            import csv
            with open(args.data, "r", encoding="utf-8") as f:
                reader = csv.DictReader(f)
                for row in reader:
                    try:
                        # ts_utc or ts_nyt_epoch available
                        if "ts_nyt_epoch" in row and row["ts_nyt_epoch"]:
                            t_ms = int(float(row["ts_nyt_epoch"])) * 1000
                        else:
                            t_ms = to_epoch_ms(row.get("ts_utc", ""))
                        ohlc.append({
                            "t": t_ms,
                            "o": float(row["open"]),
                            "h": float(row["high"]),
                            "l": float(row["low"]),
                            "c": float(row["close"]),
                        })
                    except Exception:
                        # Skip malformed rows
                        continue
        except Exception as e:
            print(f"⚠️ Failed to load data CSV overlay: {e}", file=sys.stderr)

    html = render_plotly_html(times, equities, points, args.tz, ohlc)

    with open(args.out, "w", encoding="utf-8") as f:
        f.write(html)
    print(f"✅ Visualization written to: {args.out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())



import json
import os
import sys
from datetime import datetime, timezone
from typing import List, Dict, Any, Tuple


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Visualize a Sentio trade book JSONL as equity curve and trade timeline (interactive HTML)"
    )
    parser.add_argument("--tradebook", required=True, help="Path to trade book JSONL (…_trades.jsonl)")
    parser.add_argument("--out", default="tradebook_visualization.html", help="Output HTML file")
    parser.add_argument("--data", default="data/equities/QQQ_RTH_NH.csv", help="Market data CSV to overlay (QQQ)")
    parser.add_argument("--start-equity", type=float, default=100000.0, help="Starting equity if equity_after is absent")
    parser.add_argument("--tz", default="UTC", help="Timezone label for display (no conversion performed)")
    return parser.parse_args()


def load_tradebook(path: str) -> List[Dict[str, Any]]:
    trades: List[Dict[str, Any]] = []
    with open(path, "r", encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            try:
                trades.append(json.loads(line))
            except json.JSONDecodeError:
                # Skip malformed lines gracefully
                continue
    return trades


def to_epoch_ms(dt_str: str) -> int:
    # Best-effort parse of common formats like "MM/DD HH:MM:SS" or ISO strings
    # If parsing fails, return 0 to maintain order
    fmts = [
        "%Y-%m-%dT%H:%M:%S.%fZ",
        "%Y-%m-%dT%H:%M:%SZ",
        "%Y-%m-%d %H:%M:%S",
        "%m/%d %H:%M:%S",
        "%m/%d %H:%M",
    ]
    for fmt in fmts:
        try:
            dt = datetime.strptime(dt_str, fmt)
            if dt.tzinfo is None:
                dt = dt.replace(tzinfo=timezone.utc)
            return int(dt.timestamp() * 1000)
        except Exception:
            pass
    return 0


def extract_series(trades: List[Dict[str, Any]], start_equity: float) -> Tuple[List[int], List[float], List[Dict[str, Any]]]:
    times: List[int] = []
    equities: List[float] = []
    trade_points: List[Dict[str, Any]] = []

    equity = start_equity
    have_equity_after = any("equity_after" in t for t in trades)

    for t in trades:
        # Timestamp resolution
        if "timestamp_ms" in t:
            ts = int(t["timestamp_ms"])  # already epoch ms
        elif "timestamp" in t:
            ts = int(t["timestamp"]) if str(t["timestamp"]).isdigit() else to_epoch_ms(str(t["timestamp"]))
        elif "datetime" in t:
            ts = to_epoch_ms(str(t["datetime"]))
        else:
            ts = 0

        # Equity series
        if have_equity_after and "equity_after" in t:
            try:
                equity = float(t["equity_after"])
            except Exception:
                pass
        else:
            # Fallback accumulate realized pnl if present
            pnl = 0.0
            if "realized_pnl" in t:
                try:
                    pnl = float(t["realized_pnl"])
                except Exception:
                    pnl = 0.0
            equity += pnl

        times.append(ts)
        equities.append(equity)

        # Trade markers (include instrument details)
        instrument = str(t.get("symbol", t.get("instrument", "")))
        def _to_float(val):
            try:
                return float(val)
            except Exception:
                return None

        trade_points.append({
            "timestamp_ms": ts,
            "y": equity,
            "action": str(t.get("action", "")),
            "instrument": instrument,
            "price": _to_float(t.get("price", None)),
            "quantity": _to_float(t.get("quantity", None)),
            "realized_pnl": _to_float(t.get("realized_pnl", 0.0)) or 0.0,
        })

    return times, equities, trade_points


def render_plotly_html(times: List[int], equities: List[float], trades: List[Dict[str, Any]], tz_label: str, ohlc: List[Dict[str, Any]] | None = None) -> str:
    # We avoid requiring plotly python; emit a self-contained HTML using plotly.js from CDN
    # Build JS arrays
    x_vals = times
    y_vals = equities

    # Separate buy vs sell markers
    buys = [tp for tp in trades if tp["action"].lower().startswith("buy")]
    sells = [tp for tp in trades if tp["action"].lower().startswith("sell")]

    def js_array(objs: List[Any]) -> str:
        return json.dumps(objs, separators=(",", ":"))

    # Determine visible range from trade timestamps
    valid_times = [t for t in x_vals if t and t > 0]
    if valid_times:
        tmin = min(valid_times)
        tmax = max(valid_times)
        pad = max(60_000, int((tmax - tmin) * 0.02))  # >=1min padding
        xr_start = tmin - pad
        xr_end = tmax + pad
    else:
        xr_start, xr_end = 0, 0

    # Trim OHLC to visible range
    if ohlc:
        if xr_start and xr_end:
            ohlc = [b for b in ohlc if xr_start <= int(b.get("t", 0)) <= xr_end]
    ohlc = ohlc or []
    # Compute y-axis ranges for price and equity for full-screen mapping
    def _minmax(arr: List[float], pad_ratio: float = 0.02):
        if not arr:
            return 0.0, 1.0
        vmin = min(arr)
        vmax = max(arr)
        if vmin == vmax:
            vmin -= 1.0
            vmax += 1.0
        pad = max(1e-6, (vmax - vmin) * pad_ratio)
        return vmin - pad, vmax + pad

    price_series = [float(b.get("c", 0.0)) for b in ohlc]
    px_min, px_max = _minmax(price_series, 0.03)
    eq_min, eq_max = _minmax(equities, 0.03)

    # Map trade markers to price axis using close price at the nearest bar
    ohlc_times = [int(b.get("t", 0)) for b in ohlc]
    ohlc_closes = price_series
    def _lookup_px(ts: int) -> float:
        if not ohlc_times:
            return 0.0
        # binary search for nearest index
        import bisect
        i = bisect.bisect_left(ohlc_times, ts)
        if i == 0:
            return ohlc_closes[0]
        if i >= len(ohlc_times):
            return ohlc_closes[-1]
        # pick closer of i-1 and i
        before_t, after_t = ohlc_times[i-1], ohlc_times[i]
        return ohlc_closes[i-1] if abs(ts - before_t) <= abs(after_t - ts) else ohlc_closes[i]

    buy_px = [
        {"t": tp["timestamp_ms"], "p": _lookup_px(tp["timestamp_ms"])}
        for tp in trades if tp["action"].lower().startswith("buy")
    ]
    sell_px = [
        {"t": tp["timestamp_ms"], "p": _lookup_px(tp["timestamp_ms"])}
        for tp in trades if tp["action"].lower().startswith("sell")
    ]

    ohlc_js = json.dumps(ohlc, separators=(",", ":"))
    buy_px_js = json.dumps(buy_px, separators=(",", ":"))
    sell_px_js = json.dumps(sell_px, separators=(",", ":"))
    html = f"""
<!doctype html>
<html>
  <head>
    <meta charset=\"utf-8\" />
    <meta name=\"viewport\" content=\"width=device-width, initial-scale=1\" />
    <title>Tradebook Visualization</title>
    <script src=\"https://cdn.plot.ly/plotly-2.35.2.min.js\"></script>
    <style>
      body {{ font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, Helvetica, Arial, sans-serif; margin: 0; padding: 0; }}
      #plot {{ width: 100%; height: 96vh; }}
    </style>
  </head>
  <body>
    <div id=\"plot\"></div>
    <script>
      const x = {js_array(x_vals)};  // epoch ms
      const y = {js_array(y_vals)};
      const buys = {js_array(buys)};
      const sells = {js_array(sells)};

      const ohlc = {ohlc_js};
      const buyPx = {buy_px_js};
      const sellPx = {sell_px_js};

      const equityTrace = {{
        x: x.map(ts => new Date(ts)),
        y: y,
        mode: 'lines',
        name: 'Equity',
        line: {{color: '#1f77b4'}},
        yaxis: 'y2',
        hovertemplate: '%{{x|%Y-%m-%d %H:%M:%S}} ({tz_label})<br>Equity: $%{{y:.2f}}<extra></extra>'
      }};

      const priceTrace = {{
        type: 'candlestick',
        x: ohlc.map(b => new Date(b.t)),
        open: ohlc.map(b => b.o),
        high: ohlc.map(b => b.h),
        low: ohlc.map(b => b.l),
        close: ohlc.map(b => b.c),
        name: 'QQQ',
        increasing: {{ line: {{ color: '#2ca02c' }} }},
        decreasing: {{ line: {{ color: '#d62728' }} }},
        yaxis: 'y1',
        opacity: 0.4,
        hoverinfo: 'skip'
      }};

      const buyTrace = {{
        x: buys.map(p => new Date(p.timestamp_ms)),
        y: buys.map(p => p.y),
        mode: 'markers',
        name: 'BUY',
        marker: {{ color: '#2ca02c', size: 8, symbol: 'triangle-up' }},
        customdata: buys.map(p => [p.instrument, p.price, p.quantity, p.realized_pnl]),
        hovertemplate: 'BUY %{{x|%Y-%m-%d %H:%M:%S}} ({tz_label})' +
                       '<br>Instrument: %{{customdata[0]}}' +
                       '<br>Price: $%{{customdata[1]:.2f}}  Qty: %{{customdata[2]:.4f}}' +
                       '<br>Realized PnL: $%{{customdata[3]:.2f}}' +
                       '<br>Equity: $%{{y:.2f}}<extra></extra>'
      }};

      const sellTrace = {{
        x: sells.map(p => new Date(p.timestamp_ms)),
        y: sells.map(p => p.y),
        mode: 'markers',
        name: 'SELL',
        marker: {{ color: '#d62728', size: 8, symbol: 'triangle-down' }},
        customdata: sells.map(p => [p.instrument, p.price, p.quantity, p.realized_pnl]),
        hovertemplate: 'SELL %{{x|%Y-%m-%d %H:%M:%S}} ({tz_label})' +
                       '<br>Instrument: %{{customdata[0]}}' +
                       '<br>Price: $%{{customdata[1]:.2f}}  Qty: %{{customdata[2]:.4f}}' +
                       '<br>Realized PnL: $%{{customdata[3]:.2f}}' +
                       '<br>Equity: $%{{y:.2f}}<extra></extra>'
      }};

      // Price-axis buy/sell markers (at QQQ price level)
      const buyPxTrace = {{
        x: buyPx.map(b => new Date(b.t)),
        y: buyPx.map(b => b.p),
        mode: 'markers',
        name: 'BUY@Price',
        yaxis: 'y1',
        marker: {{ color: '#2ca02c', size: 9, symbol: 'triangle-up' }},
        hovertemplate: 'BUY %{{x|%Y-%m-%d %H:%M:%S}} ({tz_label})<br>QQQ: $%{{y:.2f}}<extra></extra>'
      }};
      const sellPxTrace = {{
        x: sellPx.map(b => new Date(b.t)),
        y: sellPx.map(b => b.p),
        mode: 'markers',
        name: 'SELL@Price',
        yaxis: 'y1',
        marker: {{ color: '#d62728', size: 9, symbol: 'triangle-down' }},
        hovertemplate: 'SELL %{{x|%Y-%m-%d %H:%M:%S}} ({tz_label})<br>QQQ: $%{{y:.2f}}<extra></extra>'
      }};

      const annotations = [];
      if (buys.length + sells.length === 0) {{
        annotations.push({{
          xref: 'paper', yref: 'paper', x: 0.5, y: 0.5,
          text: 'No executed trades in this run', showarrow: false,
          font: {{ size: 16, color: '#888' }}
        }});
      }}

      const xrStart = {xr_start};
      const xrEnd = {xr_end};
      const xaxisCfg = {{ title: 'Time', type: 'date', rangeslider: {{ visible: false }} }};
      if (xrStart && xrEnd && xrEnd > xrStart) {{
        xaxisCfg.range = [new Date(xrStart), new Date(xrEnd)];
      }}

      const layout = {{
        title: 'Equity Curve and Trade Timeline',
        xaxis: xaxisCfg,
        yaxis: {{ title: 'QQQ Price', side: 'right', domain: [0.0, 1.0], fixedrange: false, range: [{px_min}, {px_max}] }},
        yaxis2: {{ title: 'Equity ($)', overlaying: 'y', side: 'left', fixedrange: false, range: [{eq_min}, {eq_max}] }},
        hovermode: 'x unified',
        legend: {{ orientation: 'h', y: -0.1 }},
        margin: {{ t: 48, r: 16, b: 64, l: 64 }},
        annotations
      }};

      const series = [priceTrace, buyPxTrace, sellPxTrace, equityTrace, buyTrace, sellTrace];
      Plotly.newPlot('plot', series, layout, {{ responsive: true }});
    </script>
  </body>
  </html>
    """
    return html


def main() -> int:
    args = parse_args()
    if not os.path.exists(args.tradebook):
        print(f"❌ Trade book not found: {args.tradebook}", file=sys.stderr)
        return 1

    trades = load_tradebook(args.tradebook)
    if not trades:
        print("❌ No trades found in trade book (or file empty).", file=sys.stderr)
        return 1

    times, equities, points = extract_series(trades, args.start_equity)

    # Optional: load OHLC from CSV for background overlay
    ohlc: List[Dict[str, Any]] = []
    if os.path.exists(args.data):
        try:
            import csv
            with open(args.data, "r", encoding="utf-8") as f:
                reader = csv.DictReader(f)
                for row in reader:
                    try:
                        # ts_utc or ts_nyt_epoch available
                        if "ts_nyt_epoch" in row and row["ts_nyt_epoch"]:
                            t_ms = int(float(row["ts_nyt_epoch"])) * 1000
                        else:
                            t_ms = to_epoch_ms(row.get("ts_utc", ""))
                        ohlc.append({
                            "t": t_ms,
                            "o": float(row["open"]),
                            "h": float(row["high"]),
                            "l": float(row["low"]),
                            "c": float(row["close"]),
                        })
                    except Exception:
                        # Skip malformed rows
                        continue
        except Exception as e:
            print(f"⚠️ Failed to load data CSV overlay: {e}", file=sys.stderr)

    html = render_plotly_html(times, equities, points, args.tz, ohlc)

    with open(args.out, "w", encoding="utf-8") as f:
        f.write(html)
    print(f"✅ Visualization written to: {args.out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())


