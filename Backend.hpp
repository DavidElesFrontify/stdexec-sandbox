#pragma once
#include <memory>
class Backend
{
public:
    virtual ~Backend() = default;
    struct Channels
    {};

    virtual Channels readChannels() const = 0;
    virtual void reconstructFromChannels(const Channels&) = 0;
    virtual void colorize() = 0;
    virtual void resize() = 0;
};

class BackendA : public Backend
{
public:
    ~BackendA() final = default;
    Channels readChannels() const final;
    void reconstructFromChannels(const Channels&) final;
    void colorize() final;
    void resize() final;
};
class BackendB : public Backend
{
public:
    ~BackendB() final = default;
    Channels readChannels() const final;
    void reconstructFromChannels(const Channels&) final;
    void colorize() final;
    void resize() final;
};

class BackendFactory
{
    public:
    static inline bool use_backend_a {true};
    static std::unique_ptr<Backend> createBackend() 
    {
        if(use_backend_a)
        {
            return std::make_unique<BackendA>();
        }
        else
        {
            return std::make_unique<BackendB>(); 
        }
    }
};
