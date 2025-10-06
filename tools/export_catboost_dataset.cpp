#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <cmath>
#include <algorithm>

struct Row {
    long long ts{};
    double open{};
    double high{};
    double low{};
    double close{};
    double volume{};
};

static std::vector<Row> read_csv(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) throw std::runtime_error("failed to open: " + path);
    std::vector<Row> out;
    std::string line;
    // read header
    if (!std::getline(f, line)) return out;
    while (std::getline(f, line)) {
        if (line.empty()) continue;
        std::stringstream ss(line);
        std::string tok;
        Row r;
        // Try common column order: timestamp,open,high,low,close,volume
        std::getline(ss, tok, ','); r.ts = std::stoll(tok);
        std::getline(ss, tok, ','); r.open = std::stod(tok);
        std::getline(ss, tok, ','); r.high = std::stod(tok);
        std::getline(ss, tok, ','); r.low = std::stod(tok);
        std::getline(ss, tok, ','); r.close = std::stod(tok);
        std::getline(ss, tok, ','); r.volume = std::stod(tok);
        out.push_back(r);
    }
    return out;
}

static double sma(const std::vector<Row>& a, size_t i, size_t n) {
    if (i + 1 < n) return a[i].close;
    double s = 0.0; for (size_t k = i + 1 - n; k <= i; ++k) s += a[k].close; return s / n;
}

static double vol_sma(const std::vector<Row>& a, size_t i, size_t n) {
    if (i + 1 < n) return a[i].volume;
    double s = 0.0; for (size_t k = i + 1 - n; k <= i; ++k) s += a[k].volume; return s / n;
}

static double stdev(const std::vector<Row>& a, size_t i, size_t n) {
    if (i + 1 < n) return 0.0;
    double m = sma(a, i, n);
    double acc = 0.0; for (size_t k = i + 1 - n; k <= i; ++k) { double d = a[k].close - m; acc += d * d; }
    return std::sqrt(std::max(0.0, acc / n));
}

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "Usage: export_catboost_dataset <input_csv> <output_csv> [horizon=1]\n";
        return 1;
    }
    const std::string in_path = argv[1];
    const std::string out_path = argv[2];
    const int horizon = (argc >= 4 ? std::max(1, std::atoi(argv[3])) : 1);

    auto data = read_csv(in_path);
    if (data.size() < 100) {
        std::cerr << "not enough rows" << std::endl;
        return 2;
    }

    std::ofstream out(out_path);
    if (!out.is_open()) throw std::runtime_error("failed to write: " + out_path);

    // Header: label then features (CatBoost expects label first by default)
    out << "label";
    std::vector<std::string> fnames = {
        "ret1","logret1","range","vol_ratio","close_sma5","close_sma10","close_sma20","close_sma50",
        "stdev5","stdev20","mom5","mom10","hl_spread","time_frac","sma5_ratio","sma20_ratio","sma50_ratio"
    };
    for (auto& n : fnames) out << "," << n;
    out << "\n";

    for (size_t i = 50; i + horizon < data.size(); ++i) {
        const auto& cur = data[i];
        const auto& prev = data[i-1];
        const auto& fut = data[i + horizon];
        double f_ret = (fut.close - cur.close) / std::max(1e-12, cur.close);
        int label = (f_ret > 0.0 ? 1 : 0); // binary up/down

        double ret1 = (cur.close - prev.close) / std::max(1e-12, prev.close);
        double logret1 = std::log(std::max(1e-12, cur.close / std::max(1e-12, prev.close)));
        double range = (cur.high - cur.low) / std::max(1e-12, cur.close);
        double volratio = cur.volume / std::max(1e-12, vol_sma(data, i, 20));
        double csma5 = cur.close / std::max(1e-12, sma(data, i, 5));
        double csma10 = cur.close / std::max(1e-12, sma(data, i, 10));
        double csma20 = cur.close / std::max(1e-12, sma(data, i, 20));
        double csma50 = cur.close / std::max(1e-12, sma(data, i, 50));
        double sd5 = stdev(data, i, 5);
        double sd20 = stdev(data, i, 20);
        double mom5 = (cur.close - data[i-5].close) / std::max(1e-12, data[i-5].close);
        double mom10 = (cur.close - data[i-10].close) / std::max(1e-12, data[i-10].close);
        double hl_spread = (cur.high - cur.low) / std::max(1e-12, cur.high);
        double day_ms = 24.0 * 60.0 * 60.0 * 1000.0;
        double time_frac = std::fmod(static_cast<double>(cur.ts), day_ms) / day_ms;
        double sma5_ratio = csma5;
        double sma20_ratio = csma20;
        double sma50_ratio = csma50;

        out << label
            << "," << ret1
            << "," << logret1
            << "," << range
            << "," << volratio
            << "," << csma5
            << "," << csma10
            << "," << csma20
            << "," << csma50
            << "," << sd5
            << "," << sd20
            << "," << mom5
            << "," << mom10
            << "," << hl_spread
            << "," << time_frac
            << "," << sma5_ratio
            << "," << sma20_ratio
            << "," << sma50_ratio
            << "\n";
    }

    std::cout << "Wrote dataset to " << out_path << std::endl;
    return 0;
}


