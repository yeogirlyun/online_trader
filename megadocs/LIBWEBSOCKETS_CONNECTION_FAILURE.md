# libwebsockets Connection Failure - Deep Dive Investigation

**Date**: 2025-10-08
**Status**: 🔴 CRITICAL - WebSocket cannot establish connection
**Impact**: Live trading system completely blocked

---

## Problem Summary

Alpaca IEX WebSocket connection fails immediately with "closed before established" error. Connection attempt is rejected before WebSocket handshake even begins.

### Error Message
```
❌ WebSocket connection error: closed before established
→ WebSocket instance destroyed
```

### What We Know

1. **SSL Infrastructure Works**
   - `openssl s_client` successfully connects to `stream.data.alpaca.markets:443`
   - SSL certificate valid (Let's Encrypt, expires Dec 12 2025)
   - TCP/TLS handshake succeeds at OS level

2. **Python Also Fails** (Confirms System-Wide Issue)
   ```python
   [SSL: CERTIFICATE_VERIFY_FAILED] certificate verify failed: unable to get local issuer certificate
   ```
   - Python websocket library fails with same SSL error
   - NOT specific to libwebsockets

3. **CA Bundle Configured** (Still Fails)
   - Using: `/opt/homebrew/etc/ca-certificates/cert.pem` (214KB, valid)
   - libwebsockets acknowledges CA bundle: `→ Using CA bundle: /opt/homebrew/etc/ca-certificates/cert.pem`
   - Connection still fails immediately

4. **No Protocol-Level Debugging**
   - Maximum libwebsockets verbosity enabled (ALL log levels)
   - No HTTP headers, handshake requests, or protocol messages logged
   - Failure occurs BEFORE WebSocket protocol layer
   - Suggests TCP/TLS level failure

5. **Callback Sequence Observed**
   ```
   // Never see these:
   LWS_CALLBACK_WSI_CREATE
   LWS_CALLBACK_CLIENT_APPEND_HANDSHAKE_HEADER
   LWS_CALLBACK_CLIENT_FILTER_PRE_ESTABLISH

   // Only see these:
   LWS_CALLBACK_CLIENT_CONNECTION_ERROR → "closed before established"
   LWS_CALLBACK_WSI_DESTROY → Instance destroyed immediately
   ```

6. **Credentials Valid**
   - Current paper trading credentials: `PKDYQYCJE5MMCTSD2AR5`
   - Alpaca REST API works perfectly (account verified, orders work)
   - Previous credentials (`PK3NCBT07OJZJULDJR5V`) are EXPIRED

---

## Debugging Steps Completed

### ✅ Step 1: Verify Network Connectivity
```bash
openssl s_client -connect stream.data.alpaca.markets:443 -servername stream.data.alpaca.markets
```
**Result**: SUCCESS - SSL handshake completes, certificate valid

### ✅ Step 2: Test with Python WebSocket
**Result**: FAIL - Same SSL certificate verification error

### ✅ Step 3: Configure CA Bundle
- Tested paths: Homebrew ARM, Homebrew x86, macOS system
- Found: `/opt/homebrew/etc/ca-certificates/cert.pem`
- Configured in libwebsockets: `info.client_ssl_ca_filepath`
**Result**: CA bundle loaded, but connection still fails

### ✅ Step 4: Enable Maximum Verbosity
- All libwebsockets log levels: `LLL_ERR | LLL_WARN | LLL_NOTICE | LLL_INFO | LLL_DEBUG | LLL_PARSER | LLL_HEADER | LLL_EXT | LLL_CLIENT | LLL_LATENCY`
**Result**: No additional protocol details revealed

### ✅ Step 5: Disable SSL Verification (Test Only)
- Added: `info.options |= LWS_SERVER_OPTION_PEER_CERT_NOT_REQUIRED`
**Result**: Still fails - Not just a cert validation issue

### ✅ Step 6: Add Handshake Callbacks
- Added callbacks for `LWS_CALLBACK_CLIENT_APPEND_HANDSHAKE_HEADER`, etc.
**Result**: Callbacks never triggered - failure before handshake

---

## Current Configuration

### libwebsockets Version
```
LWS: 4.4.0-unknown, NET CLI SRV H1 H2 WS SS-JSON-POL ConMon IPV6-on
```

### Connection Settings
```cpp
conn_info.context = context;
conn_info.address = "stream.data.alpaca.markets";
conn_info.port = 443;
conn_info.path = "/v2/iex";
conn_info.host = conn_info.address;
conn_info.origin = conn_info.address;
conn_info.protocol = protocols[0].name;  // "polygon-protocol"
conn_info.ssl_connection = LCCSCF_USE_SSL;
```

### Context Options
```cpp
info.port = CONTEXT_PORT_NO_LISTEN;
info.protocols = protocols;
info.gid = -1;
info.uid = -1;
info.options = LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;
info.client_ssl_ca_filepath = "/opt/homebrew/etc/ca-certificates/cert.pem";
```

---

## Theories & Next Steps

### Theory 1: TLS Version Mismatch
**Hypothesis**: Alpaca requires TLS 1.2+, libwebsockets using older version
**Test**: Check TLS version used by libwebsockets vs server requirement
**Command**:
```bash
openssl s_client -connect stream.data.alpaca.markets:443 -tls1_2
```

### Theory 2: SNI (Server Name Indication) Missing
**Hypothesis**: Server requires SNI extension, libwebsockets not sending it
**Test**: Verify SNI is configured
**Fix**: Explicitly set SNI in connection info

### Theory 3: Missing HTTP Headers
**Hypothesis**: Alpaca rejects connections without specific headers
**Test**: Compare openssl vs libwebsockets connection
**Fix**: Add custom headers via `LWS_CALLBACK_CLIENT_APPEND_HANDSHAKE_HEADER`

### Theory 4: IP/Firewall Restriction
**Hypothesis**: Paper trading accounts blocked from WebSocket access
**Test**: Try with live trading credentials (RISKY)
**Alternative**: Contact Alpaca support

### Theory 5: macOS Security Policy
**Hypothesis**: macOS blocking libwebsockets socket connections
**Test**: Check macOS firewall, security & privacy settings
**Command**:
```bash
/usr/libexec/ApplicationFirewall/socketfilterfw --getglobalstate
```

### Theory 6: libwebsockets Build Issue
**Hypothesis**: Library compiled without proper SSL support
**Test**: Check libwebsockets build flags
**Command**:
```bash
brew info libwebsockets
ldd /opt/homebrew/lib/libwebsockets.dylib  # Check SSL linkage
```

---

## Comparison: Working vs Broken

| Tool | Result | Details |
|------|--------|---------|
| openssl s_client | ✅ Works | SSL handshake succeeds, cert valid |
| Python websocket | ❌ Fails | SSL cert verification error |
| libwebsockets (C++) | ❌ Fails | "closed before established" |
| Alpaca REST API | ✅ Works | All endpoints functional |

**Pattern**: Only WebSocket connections fail, REST API works fine.
**Insight**: Issue specific to WebSocket protocol layer or SSL configuration for WebSocket clients.

---

## Code References

- **WebSocket Implementation**: `src/live/polygon_websocket.cpp`
- **Connection Setup**: Lines 272-310 (context creation with CA bundle)
- **Callback Handler**: Lines 25-190 (websocket_callback function)
- **Auto-Reconnection**: Lines 224-327 (receive_loop with exponential backoff)

---

## Environment

- **OS**: macOS 24.6.0 (Darwin)
- **Architecture**: ARM64 (Apple Silicon)
- **Homebrew**: /opt/homebrew
- **CA Bundle**: `/opt/homebrew/etc/ca-certificates/cert.pem` (214KB, Oct 6 2025)
- **libwebsockets**: 4.4.0-unknown via Homebrew
- **Compiler**: AppleClang (from Xcode)

---

## Workaround Options

### Option 1: Switch to Python WebSocket Client
**Pros**: Official Alpaca SDK has auto-reconnection
**Cons**: Requires Python interop, adds complexity

### Option 2: Use Alternative C++ WebSocket Library
- **Boost.Beast**: Modern C++ WebSocket client
- **IXWebSocket**: Lightweight, cross-platform
- **websocketpp**: Header-only, well-documented

### Option 3: Poll REST API Instead
**Pros**: Works reliably
**Cons**: Higher latency, rate limits, no real-time

### Option 4: Proxy Through Python
**Pros**: Leverage working Python client
**Cons**: Inter-process communication overhead

---

## Priority: P0 - Blocking

**Blocking**: Live trading completely non-functional
**Impact**: System cannot receive real-time market data
**Timeline**: Must resolve before market open (9:30 AM ET)

---

## References

- [Alpaca WebSocket API Docs](https://docs.alpaca.markets/docs/real-time-stock-pricing-data)
- [libwebsockets Documentation](https://libwebsockets.org/)
- [Previous Bug Report](./WEBSOCKET_DISCONNECT_BUG.md)
- [Community Research](./ALPACA_WEBSOCKET_RESEARCH_FINDINGS.md)

---

**Next Action**: Test Theory 2 (SNI) and Theory 6 (libwebsockets build) as most likely causes.
