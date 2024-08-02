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

#include "QueueScheduler.hpp"
#include "AsyncReader.hpp"


class Context
{
public:

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

        return stdexec::on(scheduler, just(image)) | then([this](const Image& image) { return doTransform(image); });

    }
    Image doTransform(const Image& image)
    {
        return Transform{}.transform(image);
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

    tbbexec::tbb_thread_pool m_pool{32};
    exec::async_scope m_scope;
};