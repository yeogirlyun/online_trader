#!/bin/bash
#
# Uninstall launchd Job for OnlineTrader
# =======================================
#
# Usage:
#   ./tools/uninstall_launchd.sh
#

set -e

PLIST_DEST="$HOME/Library/LaunchAgents/com.onlinetrader.autostart.plist"

echo "========================================================================"
echo "OnlineTrader launchd Uninstallation"
echo "========================================================================"
echo

if [ ! -f "$PLIST_DEST" ]; then
    echo "❌ No launchd job found at: $PLIST_DEST"
    echo "Nothing to uninstall."
    exit 0
fi

echo "Found launchd job: $PLIST_DEST"
echo

read -p "Uninstall launchd job? (y/n) " -n 1 -r
echo
if [[ ! $REPLY =~ ^[Yy]$ ]]; then
    echo "Uninstallation cancelled."
    exit 0
fi

echo
echo "Uninstalling..."

# Unload the job
echo "Unloading launchd job..."
launchctl unload "$PLIST_DEST" 2>/dev/null || echo "  (job was not loaded)"

# Remove the plist file
echo "Removing plist file..."
rm "$PLIST_DEST"

echo
echo "✅ launchd job uninstalled successfully!"
echo
echo "The following logs remain (you can delete manually if desired):"
echo "  - logs/launchd_stdout.log"
echo "  - logs/launchd_stderr.log"
echo "  - logs/cron_*.log"
echo
echo "To reinstall: ./tools/install_launchd.sh"
echo
echo "========================================================================"
echo "Uninstallation Complete!"
echo "========================================================================"
