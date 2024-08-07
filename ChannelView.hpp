#pragma once
#include "Image.hpp"
#include "Backend.hpp"
#include <functional>
#include <iostream>
class ChannelView
{
    public:
        static Lazy<ChannelView> asyncCreate(Image image)
        {
            co_return ChannelView{co_await image.readChannels()};
        }

        void manipulateChannel(std::function<float(float color)> callback) 
        {
            std::cout << "Transform from 0.8 to " << callback(0.8f) << std::endl;
        }
        const Backend::Channels& getChannels() const { return m_channels; }
    private:
        explicit ChannelView(Backend::Channels channels)
         : m_channels(std::move(channels))
        {}
        Backend::Channels m_channels;
};