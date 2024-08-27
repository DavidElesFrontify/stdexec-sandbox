#include "Backend.hpp"
#include "util.hpp"
#include "durations.hpp"
#include "SingleShotEvent.hpp"

#include <optick.h>
#define ERROR_IN_READ_CHANNELS 0

void BackendA::colorize()
{
    OPTICK_EVENT();
    busyWait(durations::one_transform);
};
void BackendA::resize() 
{
    OPTICK_EVENT();
    busyWait(durations::one_transform);
}
Lazy<Backend::Channels> BackendA::readChannels() const 
{
    OPTICK_EVENT();
    SingleShotEvent event;
    std::thread([&]
    {
        OPTICK_THREAD("Background worker");
        OPTICK_EVENT();
        busyWait(durations::one_transform);
        event.set();
    }).detach();
    co_await event;
#if ERROR_IN_READ_CHANNELS 
    throw std::runtime_error("Cannot read channels");
#endif
    co_return Backend::Channels{};
}
void BackendA::reconstructFromChannels(const Channels&) 
{
    OPTICK_EVENT();
    busyWait(durations::one_transform);
}
void BackendB::colorize() 
{
    OPTICK_EVENT();
    busyWait(durations::one_transform);
};
void BackendB::resize() 
{
    OPTICK_EVENT();
    busyWait(durations::one_transform);
}
Lazy<Backend::Channels> BackendB::readChannels() const 
{
    OPTICK_EVENT();
    busyWait(durations::one_transform);
    co_return Backend::Channels{};
}
void BackendB::reconstructFromChannels(const Channels&) 
{
    OPTICK_EVENT();
    busyWait(durations::one_transform);
}