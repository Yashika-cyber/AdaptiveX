// AdaptiveX — Order-Matching Engine
// Usage: ./adaptivex [options]
//   --rate N        Base order rate (orders/sec, default 1000)
//   --burst N       Burst rate (orders/sec, default 5000)
//   --seconds N     Run duration (default 60)
//   --seed N        RNG seed (default 42)
//   --workers N     Matching worker threads (default 4)
//   --sync locked|lockfree  Sync mode (default locked)
//   --policy priority|sjf|rr|adaptive  Scheduler (default adaptive)
//   --high-water N  Adaptive queue high-water (default 500)
//   --low-water N   Adaptive queue low-water (default 100)
//   --symbols N     Number of stock symbols (default 20, max 20)

#include <iostream>
#include <thread>
#include <atomic>
#include <chrono>
#include <cstring>
#include <cstdlib>
#include <vector>
#include <string>
#include <algorithm>

#include "order.hpp"
#include "generator.hpp"
#include "scheduler.hpp"
#include "controller.hpp"
#include "order_book.hpp"
#include "sync_locked.hpp"
#include "sync_lockfree.hpp"
#include "fairness.hpp"
#include "cache.hpp"
#include "wal.hpp"
#include "metrics.hpp"

struct Config {
    double   rate         = 1000.0;
    double   burst_rate   = 5000.0;
    int      seconds      = 60;
    uint64_t seed         = 42;
    int      workers      = 4;
    SyncMode sync         = SyncMode::LOCKED;
    PolicyType init_policy = PolicyType::ROUND_ROBIN;
    bool     adaptive     = true;
    size_t   high_water   = 500;
    size_t   low_water    = 100;
    int      symbols      = 20;
};

static Config parse_args(int argc, char* argv[]) {
    Config cfg;
    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        auto val = [&]{ return (i + 1 < argc) ? argv[++i] : ""; };
        if      (a == "--rate")       cfg.rate        = std::atof(val());
        else if (a == "--burst")      cfg.burst_rate  = std::atof(val());
        else if (a == "--seconds")    cfg.seconds     = std::atoi(val());
        else if (a == "--seed")       cfg.seed        = std::atoll(val());
        else if (a == "--workers")    cfg.workers     = std::atoi(val());
        else if (a == "--symbols")    cfg.symbols     = std::atoi(val());
        else if (a == "--high-water") cfg.high_water  = std::atoi(val());
        else if (a == "--low-water")  cfg.low_water   = std::atoi(val());
        else if (a == "--sync") {
            std::string v = val();
            if (v == "lockfree") cfg.sync = SyncMode::LOCK_FREE;
            else if (v == "locked") cfg.sync = SyncMode::LOCKED;
            else {
                std::fprintf(stderr, "Invalid --sync value: %s (expected locked or lockfree)\n", v.c_str());
                std::exit(2);
            }
        }
        else if (a == "--policy") {
            std::string v = val();
            if (v == "priority")  { cfg.init_policy = PolicyType::PRIORITY;    cfg.adaptive = false; }
            else if (v == "sjf")  { cfg.init_policy = PolicyType::SJF;         cfg.adaptive = false; }
            else if (v == "rr")   { cfg.init_policy = PolicyType::ROUND_ROBIN; cfg.adaptive = false; }
            else if (v == "adaptive") { cfg.adaptive = true; }
        }
        else if (a == "--help" || a == "-h") {
            std::fputs("Usage: adaptivex [--rate N] [--burst N] [--seconds N] "
                       "[--seed N] [--workers N] [--sync locked|lockfree] "
                       "[--policy priority|sjf|rr|adaptive] "
                       "[--high-water N] [--low-water N] [--symbols N]\n", stderr);
            std::exit(0);
        }
    }
    return cfg;
}

int main(int argc, char* argv[]) {
    Config cfg = parse_args(argc, argv);
    if (cfg.rate <= 0.0 || cfg.burst_rate <= 0.0 || cfg.seconds <= 0 ||
        cfg.workers <= 0 || cfg.symbols < 1 || cfg.symbols > 20 ||
        cfg.low_water >= cfg.high_water) {
        std::fputs("Invalid configuration: rates, duration, and workers must be positive; "
                   "symbols must be 1..20; low-water must be below high-water\n", stderr);
        return 2;
    }

    // ── Components ────────────────────────────────────────────
    GeneratorConfig gen_cfg;
    gen_cfg.base_rate   = cfg.rate;
    gen_cfg.burst_rate  = cfg.burst_rate;
    gen_cfg.seed        = cfg.seed;
    gen_cfg.num_symbols = cfg.symbols;
    OrderGenerator gen(gen_cfg);

    ControllerConfig ctrl_cfg;
    ctrl_cfg.initial_policy = cfg.init_policy;
    ctrl_cfg.high_water     = cfg.high_water;
    ctrl_cfg.low_water      = cfg.low_water;
    AdaptiveController ctrl(ctrl_cfg);

    // Use locked or lock-free inter-thread channel
    OrderBook   book;
    FairnessTracker fairness;
    PredictiveCache pred_cache(/*cap*/50, /*k*/5);
    LRUCache        lru_cache(50);
    WAL             wal;

    LatencyHistogram  latency_hist;
    ThroughputCounter tput;

    std::atomic<bool> stop_generating{false};
    std::atomic<bool> stop_workers{false};
    LockedQueue locked_q;
    LockFreeQueue lockfree_q;

    // ── Worker threads (matching) ─────────────────────────────
    auto worker_fn = [&]() {
        while (!stop_workers.load(std::memory_order_acquire) || ctrl.queue_size() > 0) {
            std::optional<Order> opt;
            opt = ctrl.pop_wait();
            if (!opt) continue;

            Order& o = *opt;
            int64_t wait_us = latency_us(o);

            // Cache lookup
            std::string sym(o.symbol);
            pred_cache.lookup(sym);
            bool lru_hit = lru_cache.access(sym);
            pred_cache.observe(sym);
            if (!lru_hit) lru_cache.insert(sym);

            // Order book matching
            auto fills = book.match(o);

            // Record latency
            latency_hist.record(wait_us);
            fairness.record_wait(o.trader_type, wait_us);
            fairness.record_completion(o.trader_type);
            tput.record_completion();

            // WAL: fills are critical, arrivals are routine
            if (!fills.empty()) {
                for (auto& f : fills) {
                    LogRecord lr;
                    lr.priority = LogPriority::CRITICAL;
                    lr.enqueue_time = std::chrono::steady_clock::now();
                    lr.payload = "{\"fill\":" + std::to_string(f.buy_order_id) + "}";
                    wal.write(std::move(lr));
                }
            }
        }
    };

    std::vector<std::thread> workers;
    workers.reserve(cfg.workers);
    for (int i = 0; i < cfg.workers; i++) workers.emplace_back(worker_fn);

    // The ingress channel is independently selectable; scheduler queues remain
    // policy-aware and mutex-protected.
    std::thread dispatch_thread([&]() {
        for (;;) {
            std::optional<Order> order = cfg.sync == SyncMode::LOCK_FREE
                ? lockfree_q.try_pop()
                : locked_q.try_pop();
            if (order) {
                ctrl.push(std::move(*order));
            } else if (stop_generating.load(std::memory_order_acquire)) {
                break;
            } else {
                std::this_thread::sleep_for(std::chrono::microseconds(50));
            }
        }
    });

    // ── Generator thread ──────────────────────────────────────
    std::thread gen_thread([&]() {
        while (!stop_generating.load(std::memory_order_acquire)) {
            auto [order, delay_sec] = gen.next();
            fairness.record_submission(order.trader_type);

            // Log arrival as routine WAL entry
            {
                LogRecord lr;
                lr.priority = LogPriority::ROUTINE;
                lr.enqueue_time = std::chrono::steady_clock::now();
                lr.payload = "{\"arr\":" + std::to_string(order.id) + "}";
                wal.write(std::move(lr));
            }

            // Push to scheduler
            if (cfg.sync == SyncMode::LOCK_FREE) {
                while (!lockfree_q.try_push(order))
                    std::this_thread::yield();
            } else {
                locked_q.push(std::move(order));
            }

            if (delay_sec > 0.0)
                std::this_thread::sleep_for(std::chrono::duration<double>(delay_sec));
        }
    });

    // ── Metrics loop (main thread) ────────────────────────────
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(cfg.seconds);
    int64_t t = 0;

    while (std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        ++t;

        // Update adaptive controller
        size_t qlen = ctrl.queue_size();
        if (cfg.adaptive) ctrl.tick(qlen);

        // Gather metrics
        auto [p50, p90, p95, p99] = latency_hist.compute();
        fairness.set_median_wait_us(p50);
        double through = tput.take(1.0);
        auto fair = fairness.compute();
        auto ws   = wal.stats();

        MetricsSnapshot snap{};
        snap.t            = t;
        snap.policy       = policy_name(ctrl.current_policy());
        snap.in_burst     = gen.in_burst();
        snap.p50_us       = p50;
        snap.p90_us       = p90;
        snap.p95_us       = p95;
        snap.p99_us       = p99;
        snap.throughput   = through;
        snap.fairness     = fair.jain_index;
        for (int i = 0; i < NUM_TRADER_TYPES; i++)
            snap.service_ratio[i] = fair.service_ratio[i];
        snap.starvation_count = fair.starvation_count;
        snap.cache_hit_pred  = pred_cache.hit_rate();
        snap.cache_hit_lru   = lru_cache.hit_rate();
        snap.wal_crit_lat_ms = ws.avg_critical_latency_ms;
        snap.wal_rout_lat_ms = ws.avg_routine_latency_ms;
        snap.wal_lift        = ws.priority_lift;
        snap.wal_flush_count = ws.flush_count;
        snap.policy_switches = ctrl.switch_count();
        snap.queue_len       = qlen;
        auto symbols = book.active_symbols();
        if (!symbols.empty()) {
            std::sort(symbols.begin(), symbols.end());
            snap.book_symbol = symbols.front();
            for (const auto& level : book.bid_levels(snap.book_symbol))
                snap.bids.push_back({level.price, level.total_qty});
            for (const auto& level : book.ask_levels(snap.book_symbol))
                snap.asks.push_back({level.price, level.total_qty});
        }

        print_json(snap);
    }

    // ── Shutdown ──────────────────────────────────────────────
    stop_generating.store(true, std::memory_order_release);
    gen_thread.join();
    dispatch_thread.join();
    stop_workers.store(true, std::memory_order_release);
    for (auto& w : workers) w.join();
    wal.stop();

    // Print final summary to stderr
    auto fair = fairness.compute();
    fprintf(stderr,
        "\n[done] t=%llds | tput=%.0f/s | J=%.3f | switches=%d\n"
        "       HFT=%.2f Retail=%.2f MM=%.2f | starved=%llu\n",
        (long long)t,
        tput.take(1.0),
        fair.jain_index,
        ctrl.switch_count(),
        fair.service_ratio[0], fair.service_ratio[1], fair.service_ratio[2],
        (unsigned long long)fair.starvation_count);

    return 0;
}
