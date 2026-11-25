// Tests for Jain's Fairness Index and FairnessTracker.
#include "fairness.hpp"
#include <cassert>
#include <iostream>
#include <cmath>

static int passed = 0, failed = 0;
#define CHECK_NEAR(a, b, eps) do { \
    if (std::fabs((a)-(b)) < (eps)) { ++passed; } \
    else { ++failed; std::cerr << "FAIL: " << #a << "=" << (a) \
        << " expected~" << (b) << " at line " << __LINE__ << "\n"; } \
} while(0)
#define CHECK(cond) do { \
    if (cond) { ++passed; } \
    else { ++failed; std::cerr << "FAIL: " << #cond << " at line " << __LINE__ << "\n"; } \
} while(0)

void test_jain_perfect() {
    // All equal → J = 1.0
    double x[] = {0.9, 0.9, 0.9};
    CHECK_NEAR(jains_index(x, 3), 1.0, 1e-9);
    std::cout << "Jain perfect fairness: OK\n";
}

void test_jain_one_wins() {
    // Only one class served → J = 1/n
    double x[] = {1.0, 0.0, 0.0};
    CHECK_NEAR(jains_index(x, 3), 1.0/3.0, 1e-6);
    std::cout << "Jain one class: OK\n";
}

void test_jain_worked_example() {
    // HFT=0.98, Retail=0.60 → J ≈ 0.945
    double x[] = {0.98, 0.60};
    double expected = (1.58 * 1.58) / (2.0 * (0.9604 + 0.36));
    CHECK_NEAR(jains_index(x, 2), expected, 1e-4);
    std::cout << "Jain worked example: J=" << jains_index(x, 2) << " (expected~0.945) OK\n";
}

void test_tracker() {
    FairnessTracker ft;
    // Simulate: HFT submits 100, 98 complete; Retail submits 50, 30 complete
    for (int i = 0; i < 100; i++) ft.record_submission(TraderType::HFT);
    for (int i = 0; i < 98;  i++) ft.record_completion(TraderType::HFT);
    for (int i = 0; i < 50;  i++) ft.record_submission(TraderType::RETAIL);
    for (int i = 0; i < 30;  i++) ft.record_completion(TraderType::RETAIL);
    // MM: 0 submissions → x_i = 1.0 by convention
    auto r = ft.compute();
    CHECK_NEAR(r.service_ratio[0], 0.98, 1e-9);
    CHECK_NEAR(r.service_ratio[1], 0.60, 1e-9);
    CHECK_NEAR(r.service_ratio[2], 1.0,  1e-9);  // no submissions → 1.0
    CHECK(r.jain_index > 0.0 && r.jain_index <= 1.0);
    std::cout << "FairnessTracker: J=" << r.jain_index << " OK\n";
}

void test_starvation() {
    FairnessTracker ft;
    ft.set_median_wait_us(100);  // median = 100µs
    // threshold = 10 × 100 = 1000µs
    ft.record_wait(TraderType::HFT, 500);     // ok
    ft.record_wait(TraderType::RETAIL, 1500); // starved!
    ft.record_wait(TraderType::HFT, 2000);    // starved!
    auto r = ft.compute();
    CHECK(r.starvation_count == 2);
    std::cout << "Starvation counter: " << r.starvation_count << " (expected 2) OK\n";
}

int main() {
    std::cout << "=== AdaptiveX Fairness Tests ===\n";
    test_jain_perfect();
    test_jain_one_wins();
    test_jain_worked_example();
    test_tracker();
    test_starvation();
    std::cout << "\nResults: " << passed << " passed, " << failed << " failed\n";
    return failed > 0 ? 1 : 0;
}
