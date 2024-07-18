#pragma once
#include <chrono>

#include <thread>
#include <format>

inline void busyWait(const std::chrono::milliseconds& duration)
{
    using clock = std::chrono::steady_clock;
    const auto end_time = clock::now() + duration;
    volatile int fake_task = 0;
    while(clock::now() < end_time) { fake_task = fake_task + 1; }
}

inline std::string getThreadIdStr()
{
    std::ostringstream os;
    os << "Thread: " << std::this_thread::get_id();
    return os.str();
}

inline thread_local const std::string g_thread_name = getThreadIdStr();