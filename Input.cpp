#include "Input.hpp"


std::optional<Image> Input::read()
{
    if(m_frame_number >= size)
    {
        return std::nullopt;
    }
    return Image{std::to_string(m_frame_number++)};
}
