#!/bin/bash
#
# Setup Daily Wake Schedule for Trading
# ======================================
#
# This script configures your Mac to wake Monday-Friday at 9:00 AM ET
# to ensure the launchd job can run at 9:15 AM ET.
#
# Usage:
#   sudo ./tools/setup_daily_wake.sh
#
# Requirements:
#   - Must run as root (uses sudo pmset)
#   - Mac must be connected to power
#

set -e

echo "========================================================================"
echo "OnlineTrader Daily Wake Schedule Setup"
echo "========================================================================"
echo

# Check if running as root
if [ "$EUID" -ne 0 ]; then
    echo "❌ ERROR: This script must be run with sudo"
    echo "Usage: sudo ./tools/setup_daily_wake.sh"
    exit 1
fi

echo "This will schedule your Mac to wake Monday-Friday at 9:00 AM ET"
echo "to ensure trading starts at 9:15 AM ET."
echo
echo "Current scheduled power events:"
pmset -g sched
echo
echo "Current power settings:"
pmset -g | grep -E "sleep|powernap|womp"
echo

read -p "Continue with setup? (y/n) " -n 1 -r
echo
if [[ ! $REPLY =~ ^[Yy]$ ]]; then
    echo "Setup cancelled."
    exit 0
fi

echo
echo "Configuring wake schedule..."

# Clear any existing power schedules (optional - be careful!)
echo "Clearing existing schedules..."
pmset repeat cancel 2>/dev/null || true

# Schedule wake Monday-Friday at 9:00 AM
# Note: pmset uses local time, not ET specifically, but since the Mac is set to ET, this works
echo "Setting wake schedule: Monday-Friday at 9:00 AM..."

pmset repeat wake MTWRF 09:00:00

if [ $? -eq 0 ]; then
    echo "✅ Wake schedule configured successfully!"
else
    echo "❌ ERROR: Failed to set wake schedule"
    exit 1
fi

echo
echo "Verifying configuration..."
pmset -g sched

echo
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "Wake Schedule Configured!"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo
echo "📋 Schedule Details:"
echo "  Days: Monday-Friday (MTWRF)"
echo "  Time: 9:00 AM (local time / ET)"
echo "  Action: Wake from sleep"
echo
echo "📋 Next Steps:"
echo "  1. Install launchd job: ./tools/install_launchd.sh"
echo "  2. Verify launchd runs at 9:15 AM ET after Mac wakes"
echo "  3. Check logs: tail -f logs/launchd_stdout.log"
echo
echo "⚙️  Additional Recommendations:"
echo "  - Keep Mac connected to power"
echo "  - Enable 'Wake for network access' (optional)"
echo "    Command: sudo pmset -a womp 1"
echo "  - Test by putting Mac to sleep before 9:00 AM on a weekday"
echo
echo "🔍 Management Commands:"
echo "  View schedule:    pmset -g sched"
echo "  Remove schedule:  sudo pmset repeat cancel"
echo "  View power:       pmset -g"
echo
echo "========================================================================"
echo "Setup Complete!"
echo "========================================================================"
