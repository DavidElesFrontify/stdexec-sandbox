#pragma once

#include <chrono>

namespace durations
{
inline std::chrono::seconds one_read_eof {1};
inline std::chrono::seconds one_read {5};
inline std::chrono::seconds one_transform {2};
inline std::chrono::seconds one_write {5};
inline std::chrono::seconds busy_operation {60};
}