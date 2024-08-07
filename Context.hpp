#pragma once

#include <optick.h>

#include <chrono>
#include <iostream>
#include <thread>
#include <shared_mutex>
#include <list>
#include <condition_variable>

#include <stdexec/execution.hpp>
#include <exec/task.hpp>
#include <exec/static_thread_pool.hpp>
#include <exec/async_scope.hpp>
#include <tbbexec/tbb_thread_pool.hpp>


#include "Image.hpp"
#include "Input.hpp"
#include "Output.hpp"
#include "Transformator.hpp"
#include "util.hpp"
#include "durations.hpp"
#include "ChannelView.hpp"

#include "QueueScheduler.hpp"
#include "AsyncReader.hpp"

template<typename THREAD_POOL>
class Context
{
public:
    template <class... Args>
      requires stdexec::constructible_from<THREAD_POOL, Args...>
    Context(Args&&... args)
        : m_pool(std::forward<Args>(args)...)
    {
        if(const uint32_t num_of_threads = m_pool.available_parallelism(); num_of_threads < 32)
        {
            std::cout << "WARNING it looks like not all the threads are utilized (32). If using libuv do not forget defining: UV_THREADPOOL_SIZE to 32.Current num of threads: " 
                    << m_pool.available_parallelism() << std::endl;
        }
    }
    template<typename T> 
    void spawn2(Input input, Output output, T&& callback)
    {
        OPTICK_EVENT();
        using stdexec::then;
        stdexec::sender auto task_flow = processVideoPerFrame(std::move(input), std::move(output)) | then(callback);
        m_scope.spawn(std::move(task_flow));
    }
    ~Context()
    {
        waitForAll();
    }
    void waitForAll()
    {
        stdexec::sync_wait(m_scope.on_empty());
    }
private:

    struct Pipeline
    {
        bool colorize_enabled {true};
        bool resize_enabled {true};
        stdexec::sender auto scheduleOn(stdexec::scheduler auto scheduler, Image image)
        {
            using stdexec::just;
            using stdexec::then;
            using stdexec::let_value;
            return stdexec::on(scheduler, just(image)) 
            | then([this](Image image) { return colorize(image); })
            | then([this](Image image) { return resize(image); })
            | let_value([this](Image image) { return manipulateAlpha(image); });
        }
        Image colorize(Image image)
        {
            if(colorize_enabled)
            {
                image.colorize();
            }
            return image;
        }
        Image resize(Image image)
        {
            if(resize_enabled)
            {
                image.resize();
            }
            return image;
        }
        exec::task<Image> manipulateAlpha(Image image)
        {
            co_await image.changeColor(0.2f);
            co_return image;
        }
        Image manipulateAlphaBlocking(Image image)
        {
            *image.changeColor(0.2f);
            return image;
        }
    };
    stdexec::sender auto readImage(Input* input)
    {
        OPTICK_EVENT();
        using stdexec::just;
        using stdexec::then;
        auto scheduler = m_pool.get_scheduler();

        return stdexec::on(scheduler, just(input)) | then([](Input* input) { OPTICK_THREAD(g_thread_name.c_str()); return input->read(); });
    }
    stdexec::sender auto transform(Image image)
    {
        OPTICK_EVENT();
        using stdexec::just;
        using stdexec::then;
        auto scheduler = m_pool.get_scheduler();

        return m_pipeline.scheduleOn(scheduler, image);

    }

    stdexec::sender auto processVideoPerFrame(Input input, Output output)
    {
        auto queue = std::make_shared<QueueScheduler<std::optional<Image>>>(&m_scope);
        return stdexec::when_all(readImages(std::move(input), queue), writeImages(std::move(output), queue));
    }

    exec::task<void> writeImages(Output output, std::shared_ptr<QueueScheduler<std::optional<Image>>> queue)
    {
        using stdexec::when_all;
        using stdexec::then;
        using stdexec::just;
        using stdexec::on;
        OPTICK_EVENT();
        while(std::optional<Image> image = co_await (*queue))
        {
            co_await (when_all(just(std::move(*image)), just(&output)) 
                      | then([](Image image, Output* output)
                             {
                                 output->write(image);
                             }));
        }
    }
    exec::task<void> readImages(Input input, std::shared_ptr<QueueScheduler<std::optional<Image>>> queue)
    {
        using stdexec::when_all;
        using stdexec::then;
        using stdexec::just;
        using stdexec::on;
        OPTICK_EVENT();
        stdexec::sender_of<stdexec::set_value_t(std::optional<Image>)> auto reading_sender = readImage(&input);
        AsyncReader<std::optional<Image>, decltype(reading_sender)> reader(reading_sender, &m_scope);
        auto as_optional = [](auto value) { return std::optional{std::move(value)}; };
        while(std::optional<Image> image = co_await reader.asyncRead())
        {
            queue->push(transform(*image) | then(as_optional));
        }
        queue->push(just(std::nullopt));

    }

    THREAD_POOL m_pool{32};
    exec::async_scope m_scope;
    Pipeline m_pipeline;
};