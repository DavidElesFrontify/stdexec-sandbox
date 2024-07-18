#include "Image.hpp"

#include <iostream>

#include <chrono>
#include <thread>

#include <optick.h>
#include "util.hpp"
#include "durations.hpp"
Image::Image(std::string name)
    : m_name(std::move(name))
{}
void Image::colorize()
{
    OPTICK_EVENT();
    OPTICK_TAG("name", m_name.c_str());
    busyWait(durations::one_transform);
    std::cout << "Colorize: " << m_name << std::endl;
}
void Image::resize()
{
    OPTICK_EVENT();
    busyWait(durations::one_transform);
    OPTICK_TAG("name", m_name.c_str());
    std::cout << "Resize: " << m_name << std::endl;
}

const std::string& Image::getName() const
{
    return m_name;
}
