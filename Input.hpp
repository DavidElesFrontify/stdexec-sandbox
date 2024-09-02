#pragma once

#include "Image.hpp"
#include "std_generator.hpp"
#include <atomic>
#include <optional>
#include <utility>

class Input
{
public:
    static coro::generator<Image> async_read(Input input)
    {
        while (auto image = input.read())
        {
            co_yield *image;
        }
    }

    std::optional<Image> read();

    explicit Input(std::string name)
        : m_name(std::move(name))
    {
    }

    Input(Input &&o)
        : m_frame_number(o.m_frame_number.load()), m_size(std::exchange(o.m_size, 0)), m_name(std::exchange(o.m_name, ""))
    {
        o.m_frame_number = 0;
    }

private:
    std::atomic_uint32_t m_frame_number{0};
    uint32_t m_size{6};
    std::string m_name;
};
