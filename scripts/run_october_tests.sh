#!/bin/bash
#
# October 2025 Rotation Trading Test Suite Runner
# ===============================================
#
# Convenience wrapper for running October rotation tests
#
# Usage:
#   ./scripts/run_october_tests.sh                    # Run full October
#   ./scripts/run_october_tests.sh 2025-10-01 2025-10-05  # Run specific range
#

set -e

cd "$(dirname "$0")/.."

START_DATE="${1:-}"
END_DATE="${2:-}"

if [ -z "$START_DATE" ]; then
    echo "🎯 Running October 2025 Rotation Trading Test Suite (Full Month)"
    python3 tools/run_october_rotation_tests.py
else
    echo "🎯 Running October 2025 Rotation Trading Test Suite ($START_DATE to $END_DATE)"
    python3 tools/run_october_rotation_tests.py --start-date "$START_DATE" --end-date "$END_DATE"
fi
