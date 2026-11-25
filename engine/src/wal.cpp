#include "wal.hpp"
#include <thread>
#include <chrono>
#include <cstdio>
#include <utility>
#include <fstream>
#include <fcntl.h>
#include <unistd.h>

WAL::WAL(const WALConfig& cfg) : cfg_(cfg) {
    if (!cfg_.flush_fn)
        cfg_.flush_fn = [](const std::vector<LogRecord>&) {};
    worker_ = std::thread([this] { run(); });
}

WAL::~WAL() { stop(); }

void WAL::write(LogRecord rec) {
    if (rec.priority == LogPriority::CRITICAL) {
        {
            std::lock_guard<std::mutex> lk(crit_mu_);
            crit_q_.push(std::move(rec));
        }
        crit_cv_.notify_one();
        crit_written_.fetch_add(1, std::memory_order_relaxed);
    } else {
        std::lock_guard<std::mutex> lk(rout_mu_);
        rout_q_.push(std::move(rec));
        rout_written_.fetch_add(1, std::memory_order_relaxed);
    }
}

void WAL::flush_batch(const std::vector<LogRecord>& batch, bool critical) {
    if (batch.empty()) return;
    // A single worker owns the file, preserving record order and avoiding
    // interleaved writes. Append-only JSON lines make recovery straightforward.
    std::string bytes;
    for (const auto& rec : batch) {
        bytes += rec.payload;
        bytes += '\n';
    }
    const int fd = ::open(cfg_.path.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0644);
    bool persisted = false;
    if (fd >= 0) {
        size_t written = 0;
        while (written < bytes.size()) {
            const ssize_t n = ::write(fd, bytes.data() + written, bytes.size() - written);
            if (n <= 0) break;
            written += static_cast<size_t>(n);
        }
        persisted = written == bytes.size() && ::fsync(fd) == 0;
        ::close(fd);
    }
    cfg_.flush_fn(batch);
    if (persisted) flush_count_.fetch_add(1, std::memory_order_relaxed);

    const auto now = std::chrono::steady_clock::now();
    for (const auto& rec : batch) {
        const auto micros = std::chrono::duration_cast<std::chrono::microseconds>(now - rec.enqueue_time).count();
        (critical ? crit_lat_sum_ : rout_lat_sum_).fetch_add(micros, std::memory_order_relaxed);
    }
}

void WAL::run() {
    auto next_batch = std::chrono::steady_clock::now()
                    + std::chrono::milliseconds(cfg_.batch_ms);

    while (true) {
        // ── Flush critical queue immediately ──────────────────
        {
            std::unique_lock<std::mutex> lk(crit_mu_);
            crit_cv_.wait_for(lk, std::chrono::milliseconds(1),
                              [this]{ return !crit_q_.empty() || !running_.load(); });

            while (!crit_q_.empty()) {
                auto rec = std::move(crit_q_.front());
                crit_q_.pop();
                lk.unlock();

                flush_batch({rec}, true);
                lk.lock();
            }
        }

        // ── Flush routine queue on timer ───────────────────────
        auto now = std::chrono::steady_clock::now();
        if (now >= next_batch || !running_.load(std::memory_order_acquire)) {
            next_batch = now + std::chrono::milliseconds(cfg_.batch_ms);

            std::vector<LogRecord> batch;
            {
                std::lock_guard<std::mutex> lk(rout_mu_);
                while (!rout_q_.empty() && batch.size() < cfg_.batch_max) {
                    batch.push_back(std::move(rout_q_.front()));
                    rout_q_.pop();
                }
            }

            if (!batch.empty()) {
                flush_batch(batch, false);
            }
        }
        if (!running_.load(std::memory_order_acquire)) {
            std::lock_guard<std::mutex> crit_lk(crit_mu_);
            std::lock_guard<std::mutex> rout_lk(rout_mu_);
            if (crit_q_.empty() && rout_q_.empty()) break;
        }
    }
}

std::vector<std::string> WAL::recover(const std::string& path) {
    std::ifstream in(path);
    std::vector<std::string> records;
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.front() == '{' && line.back() == '}') records.push_back(line);
    }
    return records;
}

void WAL::stop() {
    running_.store(false, std::memory_order_release);
    crit_cv_.notify_all();
    if (worker_.joinable()) worker_.join();
}

WAL::Stats WAL::stats() const {
    uint64_t cc = crit_written_.load(std::memory_order_relaxed);
    uint64_t rc = rout_written_.load(std::memory_order_relaxed);
    uint64_t fc = flush_count_.load(std::memory_order_relaxed);
    int64_t  cl = crit_lat_sum_.load(std::memory_order_relaxed);
    int64_t  rl = rout_lat_sum_.load(std::memory_order_relaxed);

    double avg_crit = cc > 0 ? cl / 1000.0 / cc : 0.0;  // ms
    double avg_rout = rc > 0 ? rl / 1000.0 / rc : 0.0;
    double lift     = avg_crit > 0 ? avg_rout / avg_crit : 0.0;

    return { cc, rc, fc, avg_crit, avg_rout, lift };
}
