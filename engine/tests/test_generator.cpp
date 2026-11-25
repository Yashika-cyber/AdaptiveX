#include "generator.hpp"

#include <cmath>
#include <iostream>

int main() {
    GeneratorConfig config;
    config.base_rate = 1000.0;
    config.burst_rate = 5000.0;
    config.seed = 1234;

    OrderGenerator first(config);
    OrderGenerator second(config);
    double delay_total = 0.0;

    for (int i = 0; i < 1000; ++i) {
        const auto a = first.next();
        const auto b = second.next();
        if (a.order.id != b.order.id ||
            a.order.symbol[0] != b.order.symbol[0] ||
            a.order.price != b.order.price ||
            a.delay_sec != b.delay_sec ||
            a.delay_sec <= 0.0) {
            std::cerr << "Generator output is not deterministic or has invalid delay\n";
            return 1;
        }
        delay_total += a.delay_sec;
    }

    const double mean_delay = delay_total / 1000.0;
    if (mean_delay < 0.0005 || mean_delay > 0.002) {
        std::cerr << "Unexpected mean inter-arrival delay: " << mean_delay << '\n';
        return 1;
    }
    std::cout << "Generator reproducibility and rate: OK\n";
}
