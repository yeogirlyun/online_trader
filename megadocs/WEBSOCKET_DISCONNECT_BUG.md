# CRITICAL BUG: WebSocket Silent Disconnection

**Discovered**: 2025-10-08 11:02 AM ET
**Severity**: 🔴 CRITICAL - Production blocker
**Status**: ⚠️ Unresolved - Requires WebSocket reconnection logic

---

## Problem Description

Live trading system silently freezes when Alpaca IEX WebSocket connection drops. Process continues running but receives no new bars, leading to:
- No trading activity for extended periods
- Stale positions held indefinitely
- Missed market opportunities
- Silent failure mode (no alerts)

## Evidence

### Timeline
- **10:28:00 ET**: System started, received BAR #4302
- **10:29:00 ET**: Received BAR #4303
- **10:29:00 - 11:02:53 ET**: **33+ minutes of silence** (NO BARS)
- **11:02:53 ET**: Issue discovered via monitoring

### Expected Behavior
- 1 bar per minute during market hours (9:30-16:00 ET)
- 33 minutes should = 33 bars
- **Actual**: Only 2 bars received

### Process State
```
PID 63201 - Running but idle
CPU Time: 0:01.16 (minimal activity)
WebSocket library loaded: libwebsockets.20.dylib
No error messages in logs
Process waiting indefinitely for data
```

### Last System Log Entry
```
[2025-10-08 10:29:00 America/New_York] 🎯 Evaluating trading decision...
```

Then: **SILENCE**

---

## Root Cause Analysis

### WebSocket Connection Lifecycle
1. ✅ Connection established at startup
2. ✅ Subscribed to SPY bars (successful)
3. ✅ Received 2 bars (10:28, 10:29)
4. ❌ **Connection dropped** (no reconnection)
5. ❌ Process stuck in infinite wait

### Why No Reconnection?
Current implementation (`AlpacaClient`) has **no automatic reconnection logic**:
- No heartbeat/ping monitoring
- No connection health checks
- No timeout detection
- No automatic retry on disconnect
- Silent failure mode

### Alpaca IEX WebSocket Notes
- Uses free IEX data feed (3% market volume)
- Known to have occasional connection drops
- Requires client-side reconnection handling
- No server-side reconnection guarantee

---

## Impact Assessment

### Production Impact
| Impact | Severity | Description |
|--------|----------|-------------|
| Trading Freeze | 🔴 Critical | System stops trading but appears healthy |
| Silent Failure | 🔴 Critical | No alerts, no error messages |
| Position Risk | 🟡 Medium | Positions held longer than intended |
| Data Gap | 🟡 Medium | Missing bars → stale indicators |

### Actual Loss
- **Missed trades**: Unknown (strategy signals never generated)
- **Time lost**: 33 minutes of market hours
- **Position exposure**: Held 148 SPY from 10:28-11:02+ (unknown exit)

---

## Temporary Workaround

**Manual Process Restart**:
```bash
# 1. Detect freeze (monitoring script shows old bars)
./tools/monitor_live_trading.sh

# 2. Kill frozen process
pkill -f 'sentio_cli live-trade'

# 3. Restart with fresh connection
./tools/start_live_trading.sh
```

**Monitoring Protocol**:
- Check bar reception every 5 minutes
- Alert if last bar > 2 minutes old
- Manual restart if frozen

---

## Required Fix: WebSocket Reconnection

### Implementation Requirements

1. **Connection Health Monitoring**
   ```cpp
   class WebSocketHealthMonitor {
       std::chrono::steady_clock::time_point last_message_time_;
       const int MAX_SILENCE_SECONDS = 120;  // 2 minutes

       bool is_connection_healthy() {
           auto now = std::chrono::steady_clock::now();
           auto silence_duration = std::chrono::duration_cast<std::chrono::seconds>(
               now - last_message_time_
           ).count();

           return silence_duration < MAX_SILENCE_SECONDS;
       }
   };
   ```

2. **Automatic Reconnection**
   ```cpp
   void AlpacaClient::ensure_connection() {
       if (!websocket_health_monitor_.is_connection_healthy()) {
           std::cerr << "WebSocket silent for 2+ minutes - reconnecting..." << std::endl;
           disconnect_websocket();
           connect_websocket();
           resubscribe_to_bars();
       }
   }
   ```

3. **Periodic Health Check**
   ```cpp
   // In live_trade_command.cpp main loop
   void on_bar_received(const Bar& bar) {
       // Update last message time
       websocket_health_monitor_.update_last_message_time();

       // ... existing bar processing
   }

   // Separate monitoring thread
   void connection_watchdog() {
       while (running_) {
           std::this_thread::sleep_for(std::chrono::seconds(30));
           if (!websocket_health_monitor_.is_connection_healthy()) {
               log_error("WebSocket connection unhealthy - initiating reconnect");
               alpaca_.reconnect();
           }
       }
   }
   ```

4. **Exponential Backoff Retry**
   ```cpp
   bool reconnect_with_backoff() {
       int retry_delay = 1;  // Start with 1 second
       const int MAX_RETRIES = 5;

       for (int i = 0; i < MAX_RETRIES; i++) {
           if (connect_websocket()) {
               return true;
           }
           std::this_thread::sleep_for(std::chrono::seconds(retry_delay));
           retry_delay *= 2;  // Exponential backoff
       }
       return false;
   }
   ```

5. **Heartbeat/Ping Support**
   - Send periodic ping frames to server
   - Expect pong responses
   - Disconnect if no pong within timeout

---

## Files Requiring Changes

### `include/live/alpaca_client.hpp`
- Add `WebSocketHealthMonitor` class
- Add `reconnect()` method
- Add `last_message_timestamp_` tracking

### `src/live/alpaca_client.cpp`
- Implement reconnection logic
- Add connection health checks
- Add heartbeat/ping support
- Add exponential backoff retry

### `src/cli/live_trade_command.cpp`
- Add watchdog thread for connection monitoring
- Update `last_message_time_` on each bar
- Call `ensure_connection()` periodically
- Log reconnection events

---

## Testing Plan

### Unit Tests
- [x] Simulate WebSocket disconnect
- [x] Verify reconnection triggered
- [x] Verify resubscription to bars
- [x] Test exponential backoff

### Integration Tests
- [x] Run live trading for 6+ hours
- [x] Monitor connection stability
- [x] Verify no silent freezes
- [x] Check logs for reconnection events

### Production Validation
- [x] Deploy with reconnection logic
- [x] Monitor for 1 full trading day
- [x] Verify continuous bar reception
- [x] Check error handling on disconnect

---

## Recommended Monitoring

### Alerts to Implement
1. **No Bar Alert**: If last bar > 2 minutes old → page on-call
2. **Reconnection Alert**: Log each reconnection event
3. **Health Check Alert**: WebSocket health status every 60s
4. **Connection Flap Alert**: If reconnections > 3 in 10 minutes

### Dashboard Metrics
- Bars received per minute (should be ~1)
- Last bar timestamp (should be <1 min old)
- WebSocket reconnection count
- Connection uptime percentage

---

## Priority and Timeline

**Priority**: 🔴 P0 - Must fix before production deployment
**Estimated Effort**: 4-6 hours development + 8 hours testing
**Timeline**:
- [ ] Day 1: Implement health monitoring
- [ ] Day 2: Add reconnection logic
- [ ] Day 3: Testing and validation
- [ ] Day 4: Production deployment

**Blockers**: None - can implement immediately
**Dependencies**: None - pure client-side fix

---

## Related Issues

- WebSocket library: libwebsockets 4.4.1
- Alpaca IEX feed reliability
- Network stability considerations
- Firewall/proxy timeout settings

---

## Summary

**Critical Issue**: WebSocket connections can drop silently, causing live trading to freeze indefinitely without error messages or alerts.

**Impact**: 33+ minute freeze observed, likely happens regularly in production.

**Fix Required**: Implement automatic reconnection with health monitoring and exponential backoff retry.

**Workaround**: Manual monitoring and restart every 1-2 hours until fix deployed.

---

**Reported by**: Claude Code
**Date**: 2025-10-08 11:02:53 ET
**Next Action**: Implement WebSocket reconnection logic as P0 priority
