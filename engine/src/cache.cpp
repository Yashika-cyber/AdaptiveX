#include "cache.hpp"
#include <algorithm>
#include <vector>

// ─────────────────────────── LRUCache ─────────────────────────
LRUCache::LRUCache(size_t cap) : cap_(cap) {}

bool LRUCache::access(const std::string& sym) {
    std::lock_guard<std::mutex> lk(mu_);
    total_++;
    auto it = map_.find(sym);
    if (it == map_.end()) return false;  // miss
    hits_++;
    // Move to front (most recently used)
    lru_list_.splice(lru_list_.begin(), lru_list_, it->second);
    return true;
}

void LRUCache::insert(const std::string& sym) {
    std::lock_guard<std::mutex> lk(mu_);
    if (map_.count(sym)) {
        lru_list_.splice(lru_list_.begin(), lru_list_, map_[sym]);
        return;
    }
    if (lru_list_.size() >= cap_) {
        // Evict LRU (back of list)
        map_.erase(lru_list_.back());
        lru_list_.pop_back();
    }
    lru_list_.push_front(sym);
    map_[sym] = lru_list_.begin();
}

double LRUCache::hit_rate() const {
    std::lock_guard<std::mutex> lk(mu_);
    return total_ == 0 ? 0.0 : static_cast<double>(hits_) / total_;
}

void LRUCache::reset_stats() {
    std::lock_guard<std::mutex> lk(mu_);
    hits_ = total_ = 0;
}

// ─────────────────────────── PredictiveCache ──────────────────
PredictiveCache::PredictiveCache(size_t cap, int k, double alpha)
    : cap_(cap), k_(k), alpha_(alpha) {}

bool PredictiveCache::access(const std::string& sym) {
    std::lock_guard<std::mutex> lk(mu_);
    total_++;
    bool hit = active_.count(sym) > 0;
    if (hit) hits_++;

    // Update frequency EWMA
    update_freq(sym);

    // Update Markov transition
    if (!last_sym_.empty()) update_trans(last_sym_, sym);
    last_sym_ = sym;

    // Insert into cache
    active_[sym] = true;
    evict_if_needed();

    // Prefetch top-k predictions
    if (!last_sym_.empty()) {
        auto candidates = prefetch_candidates();
        for (auto& c : candidates) {
            if (!active_.count(c)) {
                active_[c] = true;
                evict_if_needed();
            }
        }
    }

    return hit;
}

void PredictiveCache::update_freq(const std::string& sym) {
    freq_[sym] = alpha_ + (1.0 - alpha_) * freq_[sym];
    // Decay all others slightly
    for (auto& [k, v] : freq_)
        if (k != sym) v *= (1.0 - alpha_ * 0.1);
}

void PredictiveCache::update_trans(const std::string& from, const std::string& to) {
    trans_[from][to] = alpha_ + (1.0 - alpha_) * trans_[from][to];
}

double PredictiveCache::score(const std::string& sym) const {
    double f = 0.0, t = 0.0;
    auto fi = freq_.find(sym);
    if (fi != freq_.end()) f = fi->second;
    if (!last_sym_.empty()) {
        auto ti = trans_.find(last_sym_);
        if (ti != trans_.end()) {
            auto tj = ti->second.find(sym);
            if (tj != ti->second.end()) t = tj->second;
        }
    }
    return 0.5 * f + 0.5 * t;
}

std::vector<std::string> PredictiveCache::prefetch_candidates() const {
    std::vector<std::pair<double, std::string>> scored;
    for (auto& [sym, _] : freq_)
        scored.emplace_back(score(sym), sym);
    std::sort(scored.rbegin(), scored.rend());
    std::vector<std::string> out;
    for (int i = 0; i < k_ && i < static_cast<int>(scored.size()); i++)
        out.push_back(scored[i].second);
    return out;
}

void PredictiveCache::evict_if_needed() {
    while (active_.size() > cap_) {
        // Evict lowest-score item
        std::string worst;
        double worst_score = 1e18;
        for (auto& [sym, _] : active_) {
            double s = score(sym);
            if (s < worst_score) { worst_score = s; worst = sym; }
        }
        if (!worst.empty()) active_.erase(worst);
        else break;
    }
}

double PredictiveCache::hit_rate() const {
    std::lock_guard<std::mutex> lk(mu_);
    return total_ == 0 ? 0.0 : static_cast<double>(hits_) / total_;
}

void PredictiveCache::reset_stats() {
    std::lock_guard<std::mutex> lk(mu_);
    hits_ = total_ = 0;
}
