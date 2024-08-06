#include "Backend.hpp"
#include "util.hpp"
#include "durations.hpp"
void BackendA::colorize()
{
    busyWait(durations::one_transform);
};
void BackendA::resize() 
{
    busyWait(durations::one_transform);
}
Backend::Channels BackendA::readChannels() const 
{
    busyWait(durations::one_transform);
    return {};
}
void BackendA::reconstructFromChannels(const Channels&) 
{
    busyWait(durations::one_transform);
}
void BackendB::colorize() 
{
    busyWait(durations::one_transform);
};
void BackendB::resize() 
{
    busyWait(durations::one_transform);
}
Backend::Channels BackendB::readChannels() const 
{
    busyWait(durations::one_transform);
    return {};
}
void BackendB::reconstructFromChannels(const Channels&) 
{
    busyWait(durations::one_transform);
}