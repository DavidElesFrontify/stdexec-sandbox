#include "Input.hpp"

#include "util.hpp"

#include <chrono>
#include <thread>

#include <optick.h>

#include "durations.hpp"
std::optional<Image> Input::read()
{
    OPTICK_EVENT();
    OPTICK_TAG("name", m_name.c_str());
    if(m_frame_number >= m_size)
    {
        busyWait(durations::one_read_eof);
        return std::nullopt;
    }
    busyWait(durations::one_read);
    return Image{m_name + "-" + std::to_string(m_frame_number++)};
}
