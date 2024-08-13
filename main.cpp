#include <optick.h>
#include "FakeServerDemo.hpp"
#include "LibuvFakeServer.hpp"
constexpr const bool g_enable_capture = true;

void handle_error(std::exception_ptr)
{
    std::cout << "error" << std::endl;
}
int main()
{
    if constexpr(g_enable_capture)
    {
        OPTICK_START_CAPTURE();
    }
    {
        OPTICK_FRAME("MainThread");
        //runFakeServer();
        runLibUvServer();
    }
    if constexpr(g_enable_capture)
    {
        OPTICK_STOP_CAPTURE();
        OPTICK_SAVE_CAPTURE("OptickCapture.opt");
    }
    return 0;
}

