# Track-Record Based Volatility Adjustment

## Status: ✅ ALREADY IMPLEMENTED IN BASELINE v2.0

Location: `src/backend/rotation_trading_backend.cpp` (lines 783-871)

## How It Works

Tracks last 2 trades per symbol and adjusts position sizing:

### Win Scenarios
- **2 wins (>3% avg)**: 1.5x size (50% increase!)  
- **2 wins (>1% avg)**: 1.3x size (30% increase)
- **2 wins (small)**: 1.15x size (15% increase)
- **1 win (>3%)**: 1.4x size (40% increase)
- **1 win (>1.5%)**: 1.25x size (25% increase)
- **1 win (small)**: 1.15x size (15% increase)

### Loss Scenarios  
- **2 losses**: 0.3x-0.9x size (30-70% reduction via inverse vol)
- **1 loss**: 0.6x-1.0x size (inverse vol)
- **Mixed (1w/1l)**: 0.95x size (5% reduction)

### No History
- **Standard inverse volatility**: 0.7x-1.3x size

## Key Insight

The system is **AGGRESSIVE when winning** and **DEFENSIVE when losing** - exactly as requested! This amplifies gains during winning streaks and protects capital during losing streaks.

## Verification

Check logs for `[ADAPTIVE VOL]` messages showing adjustment weights and reasons.
