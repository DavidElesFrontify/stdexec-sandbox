#pragma once
#include <string>
#include <memory>
#include "Backend.hpp"
class ChannelView;

class Image
{
    friend class ChannelView;
    public:
        explicit Image(std::string name);
        Image(std::string name, const Backend::Channels& channels);
        Image(const Image& o)
        : Image(o.getName())
        {

        }
        Image(Image&& o) = default;

        Image& operator=(const Image& o)
        {
            m_name = o.getName();
            return *this;
        }
        Image& operator=(Image&&) = default;
        void colorize();
        void resize();

        const std::string& getName() const;
    private:
        Lazy<Backend::Channels> readChannels() const { return m_backend->readChannels(); }

        std::string m_name;
        std::unique_ptr<Backend> m_backend { BackendFactory::createBackend() };
};

