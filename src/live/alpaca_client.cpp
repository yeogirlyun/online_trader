#include "live/alpaca_client.hpp"
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <iostream>
#include <sstream>
#include <stdexcept>

using json = nlohmann::json;

namespace sentio {

// Callback for libcurl to capture response data
static size_t write_callback(void* contents, size_t size, size_t nmemb, std::string* userp) {
    userp->append((char*)contents, size * nmemb);
    return size * nmemb;
}

AlpacaClient::AlpacaClient(const std::string& api_key,
                           const std::string& secret_key,
                           bool paper_trading)
    : api_key_(api_key)
    , secret_key_(secret_key)
{
    if (paper_trading) {
        base_url_ = "https://paper-api.alpaca.markets/v2";
    } else {
        base_url_ = "https://api.alpaca.markets/v2";
    }
}

std::map<std::string, std::string> AlpacaClient::get_headers() {
    return {
        {"APCA-API-KEY-ID", api_key_},
        {"APCA-API-SECRET-KEY", secret_key_},
        {"Content-Type", "application/json"}
    };
}

std::string AlpacaClient::http_get(const std::string& endpoint) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        throw std::runtime_error("Failed to initialize CURL");
    }

    std::string url = base_url_ + endpoint;
    std::string response;

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    // Add headers
    struct curl_slist* headers = nullptr;
    auto header_map = get_headers();
    for (const auto& [key, value] : header_map) {
        std::string header = key + ": " + value;
        headers = curl_slist_append(headers, header.c_str());
    }
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    CURLcode res = curl_easy_perform(curl);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        throw std::runtime_error("HTTP GET failed: " + std::string(curl_easy_strerror(res)));
    }

    return response;
}

std::string AlpacaClient::http_post(const std::string& endpoint, const std::string& json_body) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        throw std::runtime_error("Failed to initialize CURL");
    }

    std::string url = base_url_ + endpoint;
    std::string response;

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_body.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    // Add headers
    struct curl_slist* headers = nullptr;
    auto header_map = get_headers();
    for (const auto& [key, value] : header_map) {
        std::string header = key + ": " + value;
        headers = curl_slist_append(headers, header.c_str());
    }
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    CURLcode res = curl_easy_perform(curl);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        throw std::runtime_error("HTTP POST failed: " + std::string(curl_easy_strerror(res)));
    }

    return response;
}

std::string AlpacaClient::http_delete(const std::string& endpoint) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        throw std::runtime_error("Failed to initialize CURL");
    }

    std::string url = base_url_ + endpoint;
    std::string response;

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    // Add headers
    struct curl_slist* headers = nullptr;
    auto header_map = get_headers();
    for (const auto& [key, value] : header_map) {
        std::string header = key + ": " + value;
        headers = curl_slist_append(headers, header.c_str());
    }
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    CURLcode res = curl_easy_perform(curl);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        throw std::runtime_error("HTTP DELETE failed: " + std::string(curl_easy_strerror(res)));
    }

    return response;
}

std::optional<AlpacaClient::AccountInfo> AlpacaClient::get_account() {
    try {
        std::string response = http_get("/account");
        return parse_account_json(response);
    } catch (const std::exception& e) {
        std::cerr << "Error getting account: " << e.what() << std::endl;
        return std::nullopt;
    }
}

std::vector<AlpacaClient::Position> AlpacaClient::get_positions() {
    try {
        std::string response = http_get("/positions");
        return parse_positions_json(response);
    } catch (const std::exception& e) {
        std::cerr << "Error getting positions: " << e.what() << std::endl;
        return {};
    }
}

std::optional<AlpacaClient::Position> AlpacaClient::get_position(const std::string& symbol) {
    try {
        std::string response = http_get("/positions/" + symbol);
        return parse_position_json(response);
    } catch (const std::exception& e) {
        // Position not found is not an error
        return std::nullopt;
    }
}

std::optional<AlpacaClient::Order> AlpacaClient::place_market_order(const std::string& symbol,
                                                                    double quantity,
                                                                    const std::string& time_in_force) {
    try {
        json order_json;
        order_json["symbol"] = symbol;
        order_json["qty"] = std::abs(quantity);
        order_json["side"] = (quantity > 0) ? "buy" : "sell";
        order_json["type"] = "market";
        order_json["time_in_force"] = time_in_force;

        std::string json_body = order_json.dump();
        std::string response = http_post("/orders", json_body);
        return parse_order_json(response);
    } catch (const std::exception& e) {
        std::cerr << "Error placing order: " << e.what() << std::endl;
        return std::nullopt;
    }
}

bool AlpacaClient::close_position(const std::string& symbol) {
    try {
        http_delete("/positions/" + symbol);
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error closing position: " << e.what() << std::endl;
        return false;
    }
}

bool AlpacaClient::close_all_positions() {
    try {
        http_delete("/positions");
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error closing all positions: " << e.what() << std::endl;
        return false;
    }
}

std::optional<AlpacaClient::Order> AlpacaClient::get_order(const std::string& order_id) {
    try {
        std::string response = http_get("/orders/" + order_id);
        return parse_order_json(response);
    } catch (const std::exception& e) {
        std::cerr << "Error getting order: " << e.what() << std::endl;
        return std::nullopt;
    }
}

bool AlpacaClient::cancel_order(const std::string& order_id) {
    try {
        http_delete("/orders/" + order_id);
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error canceling order: " << e.what() << std::endl;
        return false;
    }
}

bool AlpacaClient::is_market_open() {
    try {
        std::string response = http_get("/clock");
        json clock = json::parse(response);
        return clock["is_open"].get<bool>();
    } catch (const std::exception& e) {
        std::cerr << "Error checking market status: " << e.what() << std::endl;
        return false;
    }
}

// JSON parsing helpers

std::optional<AlpacaClient::AccountInfo> AlpacaClient::parse_account_json(const std::string& json_str) {
    try {
        json j = json::parse(json_str);
        AccountInfo info;
        info.account_number = j["account_number"].get<std::string>();
        info.buying_power = std::stod(j["buying_power"].get<std::string>());
        info.cash = std::stod(j["cash"].get<std::string>());
        info.portfolio_value = std::stod(j["portfolio_value"].get<std::string>());
        info.equity = std::stod(j["equity"].get<std::string>());
        info.last_equity = std::stod(j["last_equity"].get<std::string>());
        info.pattern_day_trader = j.value("pattern_day_trader", false);
        info.trading_blocked = j.value("trading_blocked", false);
        info.account_blocked = j.value("account_blocked", false);
        return info;
    } catch (const std::exception& e) {
        std::cerr << "Error parsing account JSON: " << e.what() << std::endl;
        return std::nullopt;
    }
}

std::vector<AlpacaClient::Position> AlpacaClient::parse_positions_json(const std::string& json_str) {
    std::vector<Position> positions;
    try {
        json j = json::parse(json_str);
        for (const auto& item : j) {
            Position pos;
            pos.symbol = item["symbol"].get<std::string>();
            pos.quantity = std::stod(item["qty"].get<std::string>());
            pos.avg_entry_price = std::stod(item["avg_entry_price"].get<std::string>());
            pos.current_price = std::stod(item["current_price"].get<std::string>());
            pos.market_value = std::stod(item["market_value"].get<std::string>());
            pos.unrealized_pl = std::stod(item["unrealized_pl"].get<std::string>());
            pos.unrealized_pl_pct = std::stod(item["unrealized_plpc"].get<std::string>());
            positions.push_back(pos);
        }
    } catch (const std::exception& e) {
        std::cerr << "Error parsing positions JSON: " << e.what() << std::endl;
    }
    return positions;
}

std::optional<AlpacaClient::Position> AlpacaClient::parse_position_json(const std::string& json_str) {
    try {
        json j = json::parse(json_str);
        Position pos;
        pos.symbol = j["symbol"].get<std::string>();
        pos.quantity = std::stod(j["qty"].get<std::string>());
        pos.avg_entry_price = std::stod(j["avg_entry_price"].get<std::string>());
        pos.current_price = std::stod(j["current_price"].get<std::string>());
        pos.market_value = std::stod(j["market_value"].get<std::string>());
        pos.unrealized_pl = std::stod(j["unrealized_pl"].get<std::string>());
        pos.unrealized_pl_pct = std::stod(j["unrealized_plpc"].get<std::string>());
        return pos;
    } catch (const std::exception& e) {
        std::cerr << "Error parsing position JSON: " << e.what() << std::endl;
        return std::nullopt;
    }
}

std::optional<AlpacaClient::Order> AlpacaClient::parse_order_json(const std::string& json_str) {
    try {
        json j = json::parse(json_str);
        Order order;
        order.order_id = j["id"].get<std::string>();
        order.symbol = j["symbol"].get<std::string>();
        order.quantity = std::stod(j["qty"].get<std::string>());
        order.side = j["side"].get<std::string>();
        order.type = j["type"].get<std::string>();
        order.time_in_force = j["time_in_force"].get<std::string>();
        order.status = j["status"].get<std::string>();
        order.filled_qty = std::stod(j["filled_qty"].get<std::string>());
        if (!j["filled_avg_price"].is_null()) {
            order.filled_avg_price = std::stod(j["filled_avg_price"].get<std::string>());
        } else {
            order.filled_avg_price = 0.0;
        }
        return order;
    } catch (const std::exception& e) {
        std::cerr << "Error parsing order JSON: " << e.what() << std::endl;
        return std::nullopt;
    }
}

} // namespace sentio
