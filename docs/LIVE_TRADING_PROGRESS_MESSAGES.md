# Live Trading Progress Messages Reference

## Startup Phase

### Connection
```
=== OnlineTrader v1.0 Live Paper Trading Started ===
Instruments: SPY (1x), SPXL (3x), SH (-1x), SDS (-2x)
Trading Hours: 9:30am - 4:00pm ET (Regular Hours Only)
Strategy: OnlineEnsemble EWRLS with Asymmetric Thresholds

Connecting to Alpaca Paper Trading...
✓ Connected - Account: PA33VD4Q6WLH
  Starting Capital: $200000.00

Connecting to Polygon proxy...
✓ Connected to Polygon
✓ Subscribed to SPY, SPXL, SH, SDS
```

## Warmup Phase

### Feature Engine Warmup
```
=== Starting Warmup Process ===
  Target: 3900 bars (10 blocks @ 390 bars/block)
  Available: 4301 bars

  Progress: 1000/4301 bars (936 training samples)

✓ Feature Engine Warmup Complete (64 bars)
  - All rolling windows initialized
  - Technical indicators ready
  - Starting predictor training...
```

### Predictor Warmup
```
  Progress: 2000/4301 bars (1936 training samples)
  Progress: 3000/4301 bars (2936 training samples)

✓ Strategy Warmup Complete (3900 bars)
  - EWRLS predictor fully trained
  - Multi-horizon predictions ready
  - Strategy ready for live trading
```

### Warmup Summary
```
=== Warmup Summary ===
✓ Total bars processed: 4301
✓ Feature engine ready: Bar 64
✓ Strategy ready: Bar 3900
✓ Predictor trained: 4236 samples
✓ Last warmup bar: 2025-10-07 16:00:00
✓ Strategy is_ready() = YES
```

## Live Trading Phase

### Market Status
```
🕐 Regular Trading Hours - processing for signals and trades
```

OR

```
⏰ After-hours - learning only, no trading
```

OR

```
⏸️  Holiday/Weekend - no trading (learning continues)
```

### Bar Reception
```
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
📊 BAR #4302 Received from Polygon
  Time: 2025-10-08 09:31:00 ET
  OHLC: O=$670.50 H=$670.75 L=$670.45 C=$670.60
  Volume: 125000
✓ Bar validation passed
```

### Strategy Update
```
⚙️  Feeding bar to strategy (updating indicators)...
✓ Predictor updated (learning from previous bar return: 0.015%)
```

### Signal Generation
```
🧠 Generating signal from strategy...
📈 SIGNAL GENERATED:
  Prediction: LONG
  Probability: 0.6245
  Confidence: 0.78
  Strategy Ready: YES
```

### Trading Decision
```
🎯 Evaluating trading decision...
```

### Trade Execution
```
🚀 *** EXECUTING TRADE ***
  Current State: CASH_ONLY
  Target State: QQQ_ONLY
  Reason: LONG signal with confidence 0.78

📤 Step 1: Closing current positions...
✓ All positions closed

💰 Step 2: Fetching account balance from Alpaca...
✓ Account Status:
  Cash: $200000.00
  Portfolio Value: $200000.00
  Buying Power: $400000.00

📥 Step 3: Opening new positions...
  🔵 Sending BUY order to Alpaca:
     Symbol: SPY
     Quantity: 298 shares
  ✓ Order Confirmed:
     Order ID: abc123-def456-ghi789
     Status: filled

✓ Transition Complete!
  New State: QQQ_ONLY
  Entry Equity: $200000.00
```

### No Trade (Hold)
```
🎯 Evaluating trading decision...
  Decision: HOLD
  Reason: Signal not strong enough (confidence 0.52 < 0.60)
```

### Balance Updates
Every trade execution shows:
```
💰 Account Status:
  Cash: $180.50
  Portfolio Value: $199999.75
  Buying Power: $360.00
```

## End of Day
```
🔔 END OF DAY - Liquidation window active
✓ EOD liquidation complete for 2025-10-08
```

## Error Messages
```
❌ Invalid bar dropped: Price gap exceeds 5%
❌ Failed to close positions - aborting transition
❌ Failed to get account info - aborting transition
❌ Failed to place order for SPY
```

## Progress Indicators

### Periodic Reconciliation (Every 60 minutes)
```
Reconciling positions with broker...
✓ Position reconciliation complete
```

### Feature Count
- **UnifiedFeatureEngine**: 58 features
  - Time: 8
  - Price/Volume: 6
  - Moving Averages: 8
  - Volatility: 11
  - Momentum: 13
  - Volume: 3
  - Donchian: 4
  - Patterns: 5

### Warmup Requirements
- **Feature Engine**: 64 bars minimum
- **Strategy**: 3900 bars (10 blocks @ 390 bars/block)
- **Predictor**: Trains on all bars after bar 64

---

**To Start Live Trading**:
```bash
cd /Volumes/ExternalSSD/Dev/C++/online_trader
source config.env
./build/sentio_cli live-trade
```

The system will automatically:
1. Connect to Alpaca Paper Trading
2. Connect to Polygon WebSocket
3. Load and process warmup data
4. Start receiving live 1-minute bars
5. Generate signals and execute trades during regular hours (9:30am-4:00pm ET)
6. Learn continuously from all bars (even after-hours)
7. Liquidate all positions at end of day (3:58pm-4:00pm ET)
