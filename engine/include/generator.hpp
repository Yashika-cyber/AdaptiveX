#pragma once

#include "order.hpp"

#include <atomic>
#include <random>
#include <string>
#include <vector>

struct GeneratorConfig {
    double base_rate = 1000.0;
    double burst_rate = 5000.0;
    double burst_prob = 0.05;
    double normal_prob = 0.2;
    double hft_frac = 0.5;
    double retail_frac = 0.3;
    double zipf_alpha = 1.2;
    int num_symbols = 20;
    uint64_t seed = 42;
};

class OrderGenerator {
public:
    struct NextOrder {
        Order order;
        double delay_sec;
    };

    explicit OrderGenerator(const GeneratorConfig& cfg);
    NextOrder next();
    bool in_burst() const { return in_burst_.load(std::memory_order_relaxed); }

private:
    void init_zipf();
    Order make_order(int sym_idx, TraderType type, Side side);

    GeneratorConfig cfg_;
    std::mt19937_64 rng_;
    std::exponential_distribution<double> inter_normal_;
    std::exponential_distribution<double> inter_burst_;
    std::bernoulli_distribution burst_trans_;
    std::bernoulli_distribution normal_trans_;
    std::vector<std::string> symbols_;
    std::vector<double> base_prices_;
    std::vector<double> zipf_weights_;
    std::discrete_distribution<int> symbol_dist_;
    std::discrete_distribution<int> side_dist_;
    std::discrete_distribution<int> trader_dist_;
    std::uniform_real_distribution<double> price_jitter_{-0.01, 0.01};
    uint64_t next_id_ = 1;
    uint64_t seq_ = 1;
    std::atomic<bool> in_burst_{false};
};
