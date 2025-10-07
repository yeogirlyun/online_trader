#pragma once

#include <stdexcept>
#include <string>

namespace sentio {

// ============================================================================
// Transient Errors (retry/reconnect)
// ============================================================================

/**
 * @brief Base class for transient errors that can be retried
 */
class TransientError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

/**
 * @brief Feed disconnection error (can reconnect)
 */
class FeedDisconnectError : public TransientError {
public:
    using TransientError::TransientError;
};

/**
 * @brief Broker API error (rate limit, temporary unavailable)
 */
class BrokerApiError : public TransientError {
public:
    int status_code;

    BrokerApiError(const std::string& msg, int code)
        : TransientError(msg), status_code(code) {}
};

// ============================================================================
// Fatal Errors (flatten + exit)
// ============================================================================

/**
 * @brief Base class for fatal trading errors (requires panic flatten)
 */
class FatalTradingError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

/**
 * @brief Position reconciliation failed (local != broker)
 */
class PositionReconciliationError : public FatalTradingError {
public:
    using FatalTradingError::FatalTradingError;
};

/**
 * @brief Feature engine corruption or validation failure
 */
class FeatureEngineError : public FatalTradingError {
public:
    using FatalTradingError::FatalTradingError;
};

/**
 * @brief Invalid bar data that cannot be processed
 */
class InvalidBarError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

} // namespace sentio
