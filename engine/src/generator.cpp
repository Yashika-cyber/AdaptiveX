#include "generator.hpp"
#include <cstring>
#include <cmath>
#include <stdexcept>

static const std::vector<std::string> SYMBOL_POOL = {
    "AAPL","MSFT","GOOGL","AMZN","TSLA","NVDA","META","NFLX",
    "AMD","INTC","ORCL","CRM","ADBE","PYPL","UBER","LYFT",
    "SNAP","TWTR","RBLX","COIN"
};

OrderGenerator::OrderGenerator(const GeneratorConfig& cfg)
    : cfg_(cfg)
    , rng_(cfg.seed)
    , inter_normal_(cfg.base_rate)
    , inter_burst_(cfg.burst_rate)
    , burst_trans_(cfg.burst_prob)
    , normal_trans_(cfg.normal_prob)
{
    if (cfg.base_rate <= 0.0 || cfg.burst_rate <= 0.0)
        throw std::invalid_argument("order rates must be positive");
    if (cfg.num_symbols < 1 ||
        cfg.num_symbols > static_cast<int>(SYMBOL_POOL.size()))
        throw std::invalid_argument("num_symbols must be between 1 and 20");
    if (cfg.zipf_alpha <= 0.0)
        throw std::invalid_argument("zipf_alpha must be positive");
    if (cfg.hft_frac < 0.0 || cfg.retail_frac < 0.0 ||
        cfg.hft_frac + cfg.retail_frac > 1.0)
        throw std::invalid_argument("trader fractions must be nonnegative and sum to at most 1");
    if (cfg.burst_prob < 0.0 || cfg.burst_prob > 1.0 ||
        cfg.normal_prob < 0.0 || cfg.normal_prob > 1.0)
        throw std::invalid_argument("burst probabilities must be in [0, 1]");

    symbols_.assign(SYMBOL_POOL.begin(), SYMBOL_POOL.begin() + cfg.num_symbols);

    // Base prices: uniform $10–$500
    std::uniform_real_distribution<> price_range(10.0, 500.0);
    base_prices_.resize(cfg.num_symbols);
    for (auto& p : base_prices_) p = price_range(rng_);

    init_zipf();

    symbol_dist_ = std::discrete_distribution<int>(
        zipf_weights_.begin(), zipf_weights_.end());
    side_dist_   = std::discrete_distribution<int>({1, 1});  // 50/50
    double mm_frac = 1.0 - cfg.hft_frac - cfg.retail_frac;
    trader_dist_ = std::discrete_distribution<int>(
        {cfg.hft_frac, cfg.retail_frac, mm_frac});
}

void OrderGenerator::init_zipf() {
    zipf_weights_.resize(cfg_.num_symbols);
    double norm = 0.0;
    for (int k = 1; k <= cfg_.num_symbols; ++k)
        norm += 1.0 / std::pow(k, cfg_.zipf_alpha);
    for (int k = 0; k < cfg_.num_symbols; ++k)
        zipf_weights_[k] = (1.0 / std::pow(k + 1, cfg_.zipf_alpha)) / norm;
}

Order OrderGenerator::make_order(int sym_idx, TraderType type, Side side) {
    Order o{};
    o.id = next_id_++;
    o.sequence = seq_++;

    std::strncpy(o.symbol, symbols_[sym_idx].c_str(), 7);
    o.symbol[7] = '\0';
    o.side = side;
    o.trader_type = type;

    double base = base_prices_[sym_idx];
    double jitter = price_jitter_(rng_);
    o.price = base * (1.0 + jitter);

    // Trader-specific parameters
    switch (type) {
        case TraderType::HFT: {
            std::uniform_int_distribution<uint32_t> qty(1, 100);
            o.quantity = qty(rng_);
            o.service_time_us = std::uniform_int_distribution<uint32_t>(1, 10)(rng_);
            o.priority = 2;  // HFT gets medium priority
            break;
        }
        case TraderType::RETAIL: {
            std::uniform_int_distribution<uint32_t> qty(100, 10000);
            o.quantity = qty(rng_);
            o.service_time_us = std::uniform_int_distribution<uint32_t>(50, 200)(rng_);
            o.priority = 1;  // Retail gets lower priority
            break;
        }
        case TraderType::MARKET_MAKER: {
            std::uniform_int_distribution<uint32_t> qty(10, 1000);
            o.quantity = qty(rng_);
            o.service_time_us = std::uniform_int_distribution<uint32_t>(5, 50)(rng_);
            o.priority = 3;  // MM gets highest priority
            break;
        }
    }

    o.arrival_time = std::chrono::steady_clock::now();
    return o;
}

OrderGenerator::NextOrder OrderGenerator::next() {
    // Markov state transition
    bool burst = in_burst_.load(std::memory_order_relaxed);
    if (burst) {
        if (normal_trans_(rng_)) burst = false;
    } else {
        if (burst_trans_(rng_)) burst = true;
    }
    in_burst_.store(burst, std::memory_order_relaxed);

    const double delay = burst
        ? inter_burst_(rng_)
        : inter_normal_(rng_);

    int    sym_idx = symbol_dist_(rng_);
    Side   side    = static_cast<Side>(side_dist_(rng_));
    auto   type    = static_cast<TraderType>(trader_dist_(rng_));

    return { make_order(sym_idx, type, side), delay };
}
