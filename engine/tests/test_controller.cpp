#include "controller.hpp"
#include <cassert>
int main() { ControllerConfig cfg; cfg.high_water = 10; cfg.low_water = 2; cfg.hold_ticks = 1; AdaptiveController controller(cfg); controller.tick(100); assert(controller.current_policy() == PolicyType::SJF); controller.tick(0); return 0; }
