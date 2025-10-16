# Email Notification System Guide

## Overview

The OnlineTrader system automatically sends comprehensive email reports after each trading session with:

1. **Embedded Dashboard Image** - Visual preview directly in email
2. **Trading Metrics Summary** - Key performance metrics
3. **Interactive HTML Dashboard** - Full detailed report as attachment

---

## What You'll Receive

### Email Structure

**Subject**: `OnlineTrader Report - 2025-10-07 (P&L: +0.00%)`

**Email Body Contains**:

1. **Header Section**
   - Session date
   - Professional gradient header

2. **Trading Summary** (4 Key Metrics)
   - Total Trades
   - Win Rate (%)
   - Final Equity ($)
   - Session P&L ($ and %)

3. **Dashboard Preview Image** (NEW!)
   - Embedded visual dashboard showing:
     - Final Equity indicator
     - Total Return indicator
     - Equity Curve & Drawdown chart
     - Trades by Symbol (bar chart)
     - Position Distribution (pie chart)
   - High-resolution 1600x1200 PNG
   - Visible directly in email (no download needed)

4. **Attachment Section**
   - Full interactive HTML dashboard
   - Download and open in browser for complete analysis

### Attachment Contents

The attached HTML dashboard (`session_YYYYMMDD_HHMMSS.html`) includes:

- **Performance Summary**
  - MRB (Mean Return per Block)
  - Total Return
  - Max Drawdown
  - Win Rate
  - Sharpe Ratio
  - Total Trades
  - Daily Trades

- **Candlestick Chart**
  - Price action with SPY candles
  - Buy/sell trade markers
  - Signal overlays
  - Portfolio value curve

- **Detailed Trade Table**
  - Every trade with timestamp
  - Symbol, side, quantity, price
  - P&L per trade
  - Running equity
  - Professional JP Morgan-style formatting

---

## Email Recipients

**Primary**: yeogirl@gmail.com

To add additional recipients, modify the email script or use CC/BCC in Gmail settings.

---

## How It Works

### Automatic Sending (Live Mode Only)

When running in **live trading mode**:

```bash
./tools/launch_live_trading.sh release
```

Email is automatically sent at end of session:
- When you press Ctrl+C
- At EOD liquidation (4:00 PM ET)
- On graceful shutdown

### Manual Sending (Mock Mode)

For mock trading sessions, send email manually:

```bash
source config.env
python3 tools/send_dashboard_email.py \
    --dashboard data/dashboards/session_20251009_163724.html \
    --trades logs/mock_trading/trades_20251009_163724.jsonl \
    --recipient yeogirl@gmail.com \
    --session-date 2025-10-07
```

The script automatically detects and loads:
- `signals_*.jsonl`
- `positions_*.jsonl`
- `decisions_*.jsonl`

Based on the trades file timestamp.

---

## Email Generation Process

### Step 1: Trading Session Completes

```
Session ends → C++ destructor called → generate_dashboard()
```

### Step 2: Dashboard HTML Generated

```
Python professional_trading_dashboard.py
  → Creates interactive HTML with Plotly charts
  → Saves to data/dashboards/session_*.html
```

### Step 3: Dashboard Screenshot Captured

```
Python screenshot_dashboard.py
  → Opens dashboard HTML in headless browser (Playwright)
  → Waits for charts to render
  → Captures full-page screenshot (1600x3000)
  → Saves to /tmp/dashboard_*.png
```

### Step 4: Email Sent

```
Python send_dashboard_email.py
  → Embeds dashboard image in email body (Content-ID: dashboard_image)
  → Attaches interactive HTML dashboard
  → Sends via Gmail SMTP (TLS encrypted)
  → Cleans up temporary image
```

---

## Configuration

### Gmail Credentials

Stored in `config.env`:

```bash
export GMAIL_USER="yeogirl@gmail.com"
export GMAIL_APP_PASSWORD="pxam kfad bpuy akmu"
```

**Setup Instructions**: See `GMAIL_SETUP_GUIDE.md`

### Email Script Options

```bash
python3 tools/send_dashboard_email.py --help

Options:
  --dashboard PATH     Path to dashboard HTML file (required)
  --trades PATH        Path to trades JSONL file (required)
  --signals PATH       Path to signals JSONL file (optional, auto-detected)
  --positions PATH     Path to positions JSONL file (optional, auto-detected)
  --decisions PATH     Path to decisions JSONL file (optional, auto-detected)
  --recipient EMAIL    Recipient email (default: yeogirl@gmail.com)
  --session-date DATE  Session date YYYY-MM-DD (optional, auto-detected)
  --no-image           Skip dashboard image generation
```

---

## Dashboard Image Details

### Visual Components

The dashboard screenshot is a **full capture of your approved dashboard HTML**, showing:

**Section 1: Performance Summary**
- MRB (Mean Return per Block)
- Total Return
- Max Drawdown
- Win Rate
- Sharpe Ratio
- Total Trades
- Daily Trades

**Section 2: Candlestick Chart**
- Price action with SPY candles
- Buy/sell trade markers on chart
- Signal overlays
- Portfolio value curve below
- Range slider for zooming

**Section 3: Trade Statement Table**
- Complete scrollable trade table
- All 62 trades with details:
  - Date/Time (ET timezone)
  - Symbol (SPY, SPXL, SH, SDS)
  - Action (BUY/SELL)
  - Quantity
  - Price
  - Trade Value
  - Cash Balance
  - Portfolio Value
  - Reason

### Image Specifications

- **Resolution**: 1600 x 3000 pixels (full page)
- **Method**: Playwright headless browser screenshot
- **Format**: PNG
- **Size**: ~500-600 KB (embedded in email)
- **Quality**: High DPI, exact replica of HTML dashboard
- **Fonts**: Matches dashboard styling
- **Background**: White with subtle shadows

---

## Troubleshooting

### Email Not Received

1. **Check Gmail credentials**:
   ```bash
   grep GMAIL config.env
   ```

2. **Check spam folder** in Gmail

3. **Verify email sent**:
   - Look for "✅ Email sent successfully" in output
   - Check for error messages

4. **Test email manually**:
   ```bash
   source config.env
   python3 tools/send_dashboard_email.py \
       --dashboard data/dashboards/session_20251009_163724.html \
       --trades logs/mock_trading/trades_20251009_163724.jsonl \
       --recipient yeogirl@gmail.com
   ```

### Dashboard Image Not Showing

1. **Verify Playwright installed**:
   ```bash
   pip show playwright
   ```

   If not installed:
   ```bash
   pip install playwright
   playwright install chromium
   ```

2. **Check screenshot generation**:
   - Look for "✅ Dashboard screenshot saved" in output
   - Check for "/tmp/dashboard_*.png" file creation

3. **Test screenshot manually**:
   ```bash
   python3 tools/screenshot_dashboard.py \
       --dashboard data/dashboards/session_20251009_163724.html \
       --output /tmp/test_screenshot.png

   open /tmp/test_screenshot.png
   ```

4. **Send email without image** (fallback):
   ```bash
   python3 tools/send_dashboard_email.py \
       --dashboard data/dashboards/session_*.html \
       --trades logs/mock_trading/trades_*.jsonl \
       --no-image
   ```

### Dashboard Metrics Incorrect

**Issue**: Trades count shows 0, but email summary shows 62

**Cause**: Two different data sources:
- Dashboard image uses `positions.jsonl` (shows realized trades)
- Email summary uses `trades.jsonl` (shows all order fills)

**Solution**: This is expected. The positions file tracks position changes (entries/exits), while trades file tracks all order fills (including scaling in/out).

For accurate trade metrics, refer to:
- **Email Summary**: Order fills count
- **Dashboard Image**: Position entries/exits
- **HTML Attachment**: Complete analysis

### Large Email Size

Dashboard images are ~500-800 KB. If email is too large:

```bash
# Send without embedded image (attachment only)
python3 tools/send_dashboard_email.py \
    --dashboard data/dashboards/session_*.html \
    --trades logs/mock_trading/trades_*.jsonl \
    --no-image
```

---

## Email Examples

### Successful Trading Session

```
Subject: OnlineTrader Report - 2025-10-07 (P&L: +1.24%)

Body:
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
📊 OnlineTrader Session Report
   2025-10-07
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

Trading Summary
───────────────────────────────
Total Trades:      87
Win Rate:          54.2%
Final Equity:      $101,240.00
Session P&L:       +$1,240.00 (+1.24%)

[Embedded Dashboard Image showing charts]

📎 Attachment: session_20251009_143502.html
   Download for full interactive dashboard
```

### Losing Session

```
Subject: OnlineTrader Report - 2025-10-08 (P&L: -0.65%)

Body:
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
📊 OnlineTrader Session Report
   2025-10-08
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

Trading Summary
───────────────────────────────
Total Trades:      62
Win Rate:          42.1%
Final Equity:      $99,350.00
Session P&L:       -$650.00 (-0.65%)

[Embedded Dashboard Image showing charts]

📎 Attachment: session_20251008_160015.html
   Download for full interactive dashboard
```

---

## Security & Privacy

### Gmail App Password Security

- **Never commit** `config.env` to git (already in `.gitignore`)
- App password grants **full Gmail access**
- Regenerate if exposed: https://myaccount.google.com/apppasswords
- Use dedicated trading Gmail account (recommended)

### Email Content Security

- Emails contain **trading performance data**
- Keep recipient list confidential
- Consider encryption for sensitive data
- Gmail uses TLS encryption in transit

---

## Integration Points

### C++ Code Integration

Location: `src/cli/live_trade_command.cpp`

```cpp
void generate_dashboard() {
    // Generate HTML dashboard
    std::string python_cmd = "python3 tools/professional_trading_dashboard.py ...";
    system(python_cmd.c_str());

    // Send email (live mode only)
    if (!is_mock_mode_) {
        std::string email_cmd = "python3 tools/send_dashboard_email.py ...";
        system(email_cmd.c_str());
    }
}
```

### Python Scripts

1. **`tools/professional_trading_dashboard.py`**
   - Generates interactive HTML dashboard
   - Uses Plotly for charts
   - Creates trade tables

2. **`tools/generate_dashboard_image.py`**
   - Creates PNG dashboard preview
   - Loads all session data (trades, signals, positions, decisions)
   - Generates comprehensive visual summary

3. **`tools/send_dashboard_email.py`**
   - Calls `generate_dashboard_image.py`
   - Embeds image in email body
   - Attaches HTML dashboard
   - Sends via Gmail SMTP

---

## Future Enhancements

Potential improvements:

1. **Multi-day summary emails** (weekly reports)
2. **Performance comparison** (vs benchmarks)
3. **Alert emails** for drawdowns >2%
4. **SMS notifications** for critical events
5. **Slack/Discord integration**
6. **PDF report generation** (alternative to HTML)
7. **Cloud storage** for dashboards (S3, Drive)
8. **Email templates** (customizable branding)

---

## Quick Reference

### Send Test Email

```bash
source config.env
python3 tools/send_dashboard_email.py \
    --dashboard data/dashboards/session_20251009_163724.html \
    --trades logs/mock_trading/trades_20251009_163724.jsonl \
    --recipient yeogirl@gmail.com
```

### Generate Dashboard Only

```bash
python3 tools/professional_trading_dashboard.py \
    --tradebook logs/mock_trading/trades_20251009_163724.jsonl \
    --signals logs/mock_trading/signals_20251009_163724.jsonl \
    --data data/equities/SPY_RTH_NH.csv \
    --output data/dashboards/custom_dashboard.html \
    --start-equity 100000
```

### Generate Image Only

```bash
python3 tools/generate_dashboard_image.py \
    --trades logs/mock_trading/trades_20251009_163724.jsonl \
    --positions logs/mock_trading/positions_20251009_163724.jsonl \
    --decisions logs/mock_trading/decisions_20251009_163724.jsonl \
    --output /tmp/dashboard_preview.png
```

---

**Status**: Fully operational
**Last Updated**: 2025-10-09
**Version**: 2.1
