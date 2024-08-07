#include "Image.hpp"

#include <iostream>

#include <chrono>
#include <thread>

#include <optick.h>

#include "ChannelView.hpp"
Image::Image(std::string name)
    : m_name(std::move(name))
{}
Image::Image(std::string name, const Backend::Channels& channels)
    : Image(std::move(name))
{
    m_backend->reconstructFromChannels(channels);
}
void Image::colorize()
{
    OPTICK_EVENT();
    OPTICK_TAG("name", m_name.c_str());
    m_backend->colorize();
    std::cout << "Colorize: " << m_name << std::endl;
}
void Image::resize()
{
    OPTICK_EVENT();
    OPTICK_TAG("name", m_name.c_str());
    m_backend->resize();
    std::cout << "Resize: " << m_name << std::endl;
}


const std::string& Image::getName() const
{
    return m_name;
}
