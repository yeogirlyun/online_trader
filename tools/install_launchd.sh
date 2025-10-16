#!/bin/bash
#
# Install launchd Job for OnlineTrader Auto-Trading
# ==================================================
#
# This script installs a launchd job that:
# - Runs Monday-Friday at 9:15 AM ET
# - Can wake Mac from sleep
# - Performs warmup (20 blocks + today's bars)
# - Launches live trading at 9:30 AM ET
# - Sends email with dashboard at end of day
#
# Usage:
#   ./tools/install_launchd.sh
#

set -e

PROJECT_DIR="/Volumes/ExternalSSD/Dev/C++/online_trader"
PLIST_SOURCE="$PROJECT_DIR/tools/com.onlinetrader.autostart.plist"
PLIST_DEST="$HOME/Library/LaunchAgents/com.onlinetrader.autostart.plist"
LAUNCH_AGENTS_DIR="$HOME/Library/LaunchAgents"

echo "========================================================================"
echo "OnlineTrader launchd Installation"
echo "========================================================================"
echo

# Validate plist source exists
if [ ! -f "$PLIST_SOURCE" ]; then
    echo "❌ ERROR: Plist file not found: $PLIST_SOURCE"
    exit 1
fi

echo "✓ Plist file validated: $PLIST_SOURCE"
echo

# Create LaunchAgents directory if it doesn't exist
mkdir -p "$LAUNCH_AGENTS_DIR"
echo "✓ LaunchAgents directory: $LAUNCH_AGENTS_DIR"
echo

# Display what will be installed
echo "This will install a launchd job to run Monday-Friday at 9:15 AM ET."
echo
echo "The launchd job will:"
echo "  1. Wake Mac from sleep if needed (Power Nap feature)"
echo "  2. Check if it's a trading day (Monday-Friday)"
echo "  3. Perform comprehensive warmup (20 blocks + today's bars)"
echo "  4. Launch live trading at 9:30 AM ET"
echo "  5. Send email with dashboard to yeogirl@gmail.com at end of day"
echo
echo "Advantages over cron:"
echo "  ✓ Can wake Mac from sleep"
echo "  ✓ Better logging"
echo "  ✓ More reliable on macOS"
echo "  ✓ Auto-restarts after reboot"
echo
echo "Logs will be saved to:"
echo "  - $PROJECT_DIR/logs/launchd_stdout.log"
echo "  - $PROJECT_DIR/logs/launchd_stderr.log"
echo "  - $PROJECT_DIR/logs/cron_YYYYMMDD.log (from cron_launcher.sh)"
echo
echo "Note: You must have GMAIL_APP_PASSWORD set in config.env for email notifications."
echo "      Generate at: https://myaccount.google.com/apppasswords"
echo

read -p "Continue with installation? (y/n) " -n 1 -r
echo
if [[ ! $REPLY =~ ^[Yy]$ ]]; then
    echo "Installation cancelled."
    exit 0
fi

echo
echo "Installing launchd job..."

# Check if already installed
if [ -f "$PLIST_DEST" ]; then
    echo "⚠️  Warning: Existing launchd job found"
    echo
    echo "Current job: $PLIST_DEST"
    launchctl list | grep onlinetrader || echo "  (not currently loaded)"
    echo
    read -p "Replace existing job? (y/n) " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        echo "Installation cancelled. To manually manage:"
        echo "  Unload: launchctl unload $PLIST_DEST"
        echo "  Remove: rm $PLIST_DEST"
        exit 0
    fi

    # Unload existing job
    echo "Unloading existing job..."
    launchctl unload "$PLIST_DEST" 2>/dev/null || true
    sleep 1
fi

# Copy plist file
echo "Copying plist file..."
cp "$PLIST_SOURCE" "$PLIST_DEST"
echo "✓ Plist copied to: $PLIST_DEST"

# Set permissions
chmod 644 "$PLIST_DEST"
echo "✓ Permissions set (644)"

# Load the job
echo "Loading launchd job..."
launchctl load "$PLIST_DEST"

# Verify it loaded
sleep 1
if launchctl list | grep -q "com.onlinetrader.autostart"; then
    echo "✅ launchd job loaded successfully!"
else
    echo "⚠️  Warning: Job may not have loaded properly"
    echo "Check with: launchctl list | grep onlinetrader"
fi

echo
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "Installation Complete!"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo
echo "📋 Job Details:"
echo "  Label: com.onlinetrader.autostart"
echo "  Schedule: Monday-Friday at 9:15 AM ET"
echo "  Plist: $PLIST_DEST"
echo
echo "📋 Next Steps:"
echo "  1. Verify config.env has all API keys (Alpaca, Polygon, Gmail)"
echo "  2. Test manually: bash $PROJECT_DIR/tools/cron_launcher.sh"
echo "  3. Check job status: launchctl list | grep onlinetrader"
echo "  4. View logs: tail -f logs/launchd_stdout.log"
echo
echo "🔍 Management Commands:"
echo "  Check status:     launchctl list | grep onlinetrader"
echo "  Unload (disable): launchctl unload $PLIST_DEST"
echo "  Reload (enable):  launchctl load $PLIST_DEST"
echo "  Remove:           launchctl unload $PLIST_DEST && rm $PLIST_DEST"
echo "  View logs:        tail -f $PROJECT_DIR/logs/launchd_*.log"
echo
echo "⚙️  Optional: Enable Power Nap to wake from sleep"
echo "  System Preferences → Battery → Power Adapter"
echo "  ☑ Enable Power Nap while plugged into power adapter"
echo
echo "📧 Email notifications will be sent to: yeogirl@gmail.com"
echo "   (Set GMAIL_APP_PASSWORD in config.env)"
echo
echo "========================================================================"
echo "Installation Complete!"
echo "========================================================================"
