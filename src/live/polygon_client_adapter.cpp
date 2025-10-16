#include "live/polygon_client_adapter.h"

namespace sentio {

PolygonClientAdapter::PolygonClientAdapter(const std::string& proxy_url,
                                         const std::string& auth_key)
    : client_(std::make_unique<PolygonClient>(proxy_url, auth_key))
{
}

PolygonClientAdapter::~PolygonClientAdapter() {
    stop();
}

bool PolygonClientAdapter::connect() {
    return client_->connect();
}

bool PolygonClientAdapter::subscribe(const std::vector<std::string>& symbols) {
    return client_->subscribe(symbols);
}

void PolygonClientAdapter::start(BarCallback callback) {
    client_->start(callback);
}

void PolygonClientAdapter::stop() {
    client_->stop();
}

std::vector<Bar> PolygonClientAdapter::get_recent_bars(const std::string& symbol, size_t count) const {
    return client_->get_recent_bars(symbol, count);
}

bool PolygonClientAdapter::is_connected() const {
    return client_->is_connected();
}

bool PolygonClientAdapter::is_connection_healthy() const {
    return client_->is_connection_healthy();
}

int PolygonClientAdapter::get_seconds_since_last_message() const {
    return client_->get_seconds_since_last_message();
}

} // namespace sentio
