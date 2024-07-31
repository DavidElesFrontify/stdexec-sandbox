#include "Input.hpp"

#include "util.hpp"

#include <chrono>
#include <thread>
#include <iostream>

#include <optick.h>

#include "durations.hpp"
std::optional<Image> Input::read()
{
    OPTICK_EVENT();
    OPTICK_TAG("name", m_name.c_str());
    if(m_frame_number >= m_size)
    {
        std::cout << "Read: " << m_name << "-EOF" << std::endl;
        busyWait(durations::one_read_eof);
        return std::nullopt;
    }
    const std::string image_name = m_name + "-" + std::to_string(m_frame_number++);
    std::cout << "Read: " << image_name << std::endl;
    busyWait(durations::one_read);
    return Image{image_name};
}
