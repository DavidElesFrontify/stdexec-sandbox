#include "Image.hpp"

#include <iostream>

Image::Image(std::string name)
    : m_name(std::move(name))
{}
void Image::colorize()
{
    std::cout << "Colorize: " << m_name << std::endl;
}
void Image::resize()
{
    std::cout << "Resize: " << m_name << std::endl;
}

const std::string& Image::getName() const
{
    return m_name;
}
