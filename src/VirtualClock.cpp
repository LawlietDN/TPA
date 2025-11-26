#include "VirtualClock.hpp"

std::atomic<bool> VirtualClock::enabled{false};
std::atomic<std::time_t> VirtualClock::value{0};
