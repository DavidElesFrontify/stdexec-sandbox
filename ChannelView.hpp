#pragma once
#include "Image.hpp"
#include "Backend.hpp"
#include <functional>
class ChannelView
{
    public:
        explicit ChannelView(const Image& image) 
         : m_channels(image.readChannels())
        {}

        void manipulateChannel(std::function<float(float color)>) {}
        const Backend::Channels& getChannels() const { return m_channels; }
    private:
        Backend::Channels m_channels;
};