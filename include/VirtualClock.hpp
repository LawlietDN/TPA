#pragma once
#include <atomic>
#include <ctime>

class VirtualClock
{
public:
    static void set(std::time_t t)
    {
        value.store(t, std::memory_order_relaxed);
        enabled.store(true, std::memory_order_relaxed);
    }

    static void disable()
    {
        enabled.store(false, std::memory_order_relaxed);
    }

    static std::time_t now()
    {
        if (enabled.load(std::memory_order_relaxed))
            return value.load(std::memory_order_relaxed);
        return std::time(nullptr);
    }

private:
    static std::atomic<bool> enabled;
    static std::atomic<std::time_t> value;
};
