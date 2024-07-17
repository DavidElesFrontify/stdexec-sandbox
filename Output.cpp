#include "Output.hpp"


#include <iostream>

void Output::write(const Image& image)
{
    std::cout << "Write image: " << image.getName() << std::endl;
}
