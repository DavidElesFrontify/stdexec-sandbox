#include "Output.hpp"

#include <chrono>
#include <thread>

#include <optick.h>

#include <iostream>
#include "util.hpp"
#include "durations.hpp"

void Output::write(const Image& image)
{
    OPTICK_EVENT();
    OPTICK_TAG("Name", image.getName().c_str());
    busyWait(durations::one_write);
    std::cout << "Write image: " << image.getName() << std::endl;
}
