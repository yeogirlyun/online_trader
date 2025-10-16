#include "features/indicators.h"
#include <iostream>
#include <iomanip>

using namespace sentio::features::ind;

int main() {
    Boll bb(20, 2);  // Period=20, k=2

    std::cout << std::fixed << std::setprecision(4);

    // Feed 30 bars
    for (int i = 1; i <= 30; ++i) {
        double close = 100.0 + i * 0.1;  // Simple increasing prices
        bb.update(close);

        std::cout << "Bar " << std::setw(2) << i
                  << ": win.size=" << bb.win.size()
                  << ", win.full=" << bb.win.full()
                  << ", is_ready=" << bb.is_ready()
                  << ", mean=" << std::setw(8) << bb.mean
                  << ", sd=" << std::setw(8) << bb.sd;

        if (std::isnan(bb.mean)) {
            std::cout << " [NaN]";
        } else {
            std::cout << " [OK]";
        }
        std::cout << std::endl;
    }

    return 0;
}
