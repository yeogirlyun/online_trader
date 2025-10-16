# Gmail Setup for Email Notifications

**Quick Reference for OnlineTrader Email Notifications**

---

## Prerequisites

- Gmail account with 2-Step Verification enabled
- Access to https://myaccount.google.com/apppasswords

---

## Setup Steps

### 1. Generate Gmail App Password

1. **Visit**: https://myaccount.google.com/apppasswords

   Or navigate:
   - Google Account → Security → 2-Step Verification → App passwords

2. **Create App Password**:
   - Select app: "Mail"
   - Select device: "Mac" or "Other (OnlineTrader)"
   - Click "Generate"

3. **Copy the 16-character password**:
   ```
   Example: abcd efgh ijkl mnop
   ```

   ⚠️ **Important**: Save this password - you can't view it again!

### 2. Add to config.env

**Option A: Command Line** (Recommended)
```bash
# Navigate to project directory
cd /Volumes/ExternalSSD/Dev/C++/online_trader

# Add Gmail user
echo 'GMAIL_USER=yeogirl@gmail.com' >> config.env

# Add app password (replace with your actual password)
echo 'GMAIL_APP_PASSWORD=abcd efgh ijkl mnop' >> config.env
```

**Option B: Manual Edit**
```bash
# Open config.env in text editor
open -e config.env

# Add these lines at the end:
# Gmail credentials for email notifications
GMAIL_USER=yeogirl@gmail.com
GMAIL_APP_PASSWORD=your-16-char-password-here

# Save and close
```

### 3. Verify Setup

```bash
# Load credentials
source config.env

# Check they're loaded
echo "GMAIL_USER: $GMAIL_USER"
echo "Password set: ${GMAIL_APP_PASSWORD:+YES}"

# Verify config.env contains them
grep GMAIL config.env
```

### 4. Test Email

```bash
# Load environment
source config.env

# Send test email with latest dashboard
python3 tools/send_dashboard_email.py \
    --dashboard data/dashboards/session_$(ls -t data/dashboards/ | head -1) \
    --trades logs/mock_trading/trades_$(ls -t logs/mock_trading/trades_* | head -1 | xargs basename) \
    --recipient yeogirl@gmail.com
```

**Expected Output**:
```
📧 Preparing email notification...
📊 Session Summary:
   Trades: XX
   P&L: $X.XX (X.XX%)
   Final Equity: $XXX,XXX.XX

📤 Sending email...
✅ Email sent successfully to yeogirl@gmail.com
```

---

## Troubleshooting

### "App passwords option not available"

**Enable 2-Step Verification first**:
1. Visit: https://myaccount.google.com/security
2. Click "2-Step Verification" → Turn on
3. Complete phone verification
4. Return to App Passwords page

### "Authentication failed" error

**Check credentials**:
```bash
# Verify password in config.env
cat config.env | grep GMAIL_APP_PASSWORD

# Ensure no extra spaces or quotes
# Correct:   GMAIL_APP_PASSWORD=abcdefghijklmnop
# Incorrect: GMAIL_APP_PASSWORD="abcd efgh ijkl mnop"
# Incorrect: GMAIL_APP_PASSWORD = abcdefghijklmnop
```

**Fix if needed**:
```bash
# Remove old entry
sed -i '' '/GMAIL_APP_PASSWORD/d' config.env

# Add correct entry (no quotes, spaces optional)
echo 'GMAIL_APP_PASSWORD=abcdefghijklmnop' >> config.env
```

### Email not sending from automated sessions

**Ensure environment is loaded**:

For **cron jobs**, credentials must be in config.env because cron doesn't load shell environment.

Verify cron launcher loads config.env:
```bash
grep "source config.env" tools/cron_launcher.sh
```

For **manual launches**, source before running:
```bash
source config.env
./tools/launch_live_trading.sh release
```

---

## Security Notes

### Protecting Your App Password

**✅ Good Practices**:
- Store in `config.env` (already in `.gitignore`)
- Never commit to git
- Regenerate if exposed
- Use app-specific passwords (not main password)

**❌ Don't**:
- Share the password
- Commit to repository
- Store in scripts
- Use your main Gmail password

### Revoke Access

If you need to revoke access:
1. Visit: https://myaccount.google.com/apppasswords
2. Find "OnlineTrader" (or "Mail") entry
3. Click "Remove"

Then generate a new one and update `config.env`.

---

## Email Format

### What You'll Receive

**Subject**:
```
OnlineTrader Report - 2025-10-09 (P&L: +0.25%)
```

**Body** (HTML formatted):
```
📊 OnlineTrader Session Report
   2025-10-09

Trading Summary
───────────────────────────────
Total Trades:      62
Win Rate:          52.3%
Final Equity:      $100,250.00
Session P&L:       +$250.00 (+0.25%)

📎 Attachment: session_20251009_163724.html
   Open in browser for interactive charts
```

**Attachment**: Full interactive dashboard HTML file

### Email Schedule

**Live Trading Mode**:
- Sent automatically at end of each trading session (4:00 PM ET)
- Or when you stop trading with Ctrl+C

**Mock Trading Mode**:
- NOT sent automatically
- Must send manually with email script

---

## Quick Commands

```bash
# Setup
echo 'GMAIL_USER=yeogirl@gmail.com' >> config.env
echo 'GMAIL_APP_PASSWORD=your-password' >> config.env

# Test
source config.env
python3 tools/send_dashboard_email.py \
    --dashboard data/dashboards/session_20251009_163724.html \
    --trades logs/mock_trading/trades_20251009_163724.jsonl \
    --recipient yeogirl@gmail.com

# Verify
grep GMAIL config.env
```

---

## Documentation

- **Main Guide**: `AUTOMATED_TRADING_SETUP.md`
- **Email Script**: `tools/send_dashboard_email.py`
- **Cron Setup**: `tools/install_cron.sh`

---

**Status**: Ready to configure
**Last Updated**: 2025-10-09
