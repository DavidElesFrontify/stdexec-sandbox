#pragma once

#include "Image.hpp"

#include <atomic>
#include <optional>

class Input
{
    public:
        std::optional<Image> read();

    private:
        std::atomic_uint32_t m_frame_number{0};
        uint32_t size {3};
};
