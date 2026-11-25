#include "cache.hpp"
#include <cassert>
int main() { PredictiveCache cache(4, 1); assert(!cache.lookup("AAPL")); cache.observe("AAPL"); assert(cache.lookup("AAPL")); assert(cache.hit_rate() == 0.5); return 0; }
