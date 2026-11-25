#pragma once

#include <list>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

class LRUCache {
public:
    explicit LRUCache(size_t capacity);
    bool access(const std::string& symbol);
    void insert(const std::string& symbol);
    double hit_rate() const;
    void reset_stats();

private:
    size_t cap_;
    mutable std::mutex mu_;
    std::list<std::string> lru_list_;
    std::unordered_map<std::string, std::list<std::string>::iterator> map_;
    uint64_t hits_ = 0;
    uint64_t total_ = 0;
};

class PredictiveCache {
public:
    explicit PredictiveCache(size_t capacity, int prefetch_count, double alpha = 0.2);
    bool access(const std::string& symbol);
    double hit_rate() const;
    void reset_stats();

private:
    void update_freq(const std::string& symbol);
    void update_trans(const std::string& from, const std::string& to);
    double score(const std::string& symbol) const;
    std::vector<std::string> prefetch_candidates() const;
    void evict_if_needed();

    size_t cap_;
    int k_;
    double alpha_;
    mutable std::mutex mu_;
    std::unordered_map<std::string, bool> active_;
    std::unordered_map<std::string, double> freq_;
    std::unordered_map<std::string, std::unordered_map<std::string, double>> trans_;
    std::string last_sym_;
    uint64_t hits_ = 0;
    uint64_t total_ = 0;
};
