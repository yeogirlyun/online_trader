#ifndef SENTIO_POLYGON_CLIENT_ADAPTER_H
#define SENTIO_POLYGON_CLIENT_ADAPTER_H

#include "live/bar_feed_interface.h"
#include "live/polygon_client.hpp"
#include "live/position_book.h"
#include "common/types.h"
#include <memory>

namespace sentio {

/**
 * Polygon Client Adapter
 *
 * Adapts existing PolygonClient to IBarFeed interface.
 * Provides minimal wrapper to enable polymorphic substitution.
 */
class PolygonClientAdapter : public IBarFeed {
public:
    /**
     * Constructor
     *
     * @param proxy_url WebSocket URL for Polygon proxy
     * @param auth_key Authentication key
     */
    PolygonClientAdapter(const std::string& proxy_url, const std::string& auth_key);

    ~PolygonClientAdapter() override;

    // IBarFeed interface implementation
    bool connect() override;
    bool subscribe(const std::vector<std::string>& symbols) override;
    void start(BarCallback callback) override;
    void stop() override;
    std::vector<Bar> get_recent_bars(const std::string& symbol, size_t count = 100) const override;
    bool is_connected() const override;
    bool is_connection_healthy() const override;
    int get_seconds_since_last_message() const override;

private:
    std::unique_ptr<PolygonClient> client_;
};

} // namespace sentio

#endif // SENTIO_POLYGON_CLIENT_ADAPTER_H
