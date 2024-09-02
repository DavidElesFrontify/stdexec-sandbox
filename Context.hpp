#pragma once

#include <optick.h>

#include <chrono>
#include <iostream>
#include <thread>
#include <shared_mutex>
#include <list>
#include <expected>
#include <condition_variable>
#include <optional>
#include <ranges>

#include <stdexec/execution.hpp>
#include <exec/task.hpp>
#include <exec/static_thread_pool.hpp>
#include <exec/async_scope.hpp>
#include <exec/repeat_effect_until.hpp>
#include <tbbexec/tbb_thread_pool.hpp>

#include "Image.hpp"
#include "Input.hpp"
#include "Output.hpp"
#include "Transformator.hpp"
#include "util.hpp"
#include "durations.hpp"
#include "ChannelView.hpp"
// #include "Retry.hpp"

#include "TaskStream.hpp"
#include "AsyncReader.hpp"

template <typename Callable>
class scope_exit
{
public:
    explicit scope_exit(Callable &&callback)
        : m_callback(std::forward<Callable>(callback))
    {
    }
    ~scope_exit()
    {
        try
        {
            m_callback();
        }
        catch (...)
        {
        }
    }

private:
    Callable m_callback;
};

template <typename THREAD_POOL>
class Context
{
public:
    template <class... Args>
        requires stdexec::constructible_from<THREAD_POOL, Args...>
    Context(Args &&...args)
        : m_pool(std::forward<Args>(args)...)
    {
        if (const uint32_t num_of_threads = m_pool.available_parallelism(); num_of_threads < 32)
        {
            std::cout << "WARNING it looks like not all the threads are utilized (32). If using libuv do not forget defining: UV_THREADPOOL_SIZE to 32.Current num of threads: "
                      << m_pool.available_parallelism() << std::endl;
        }
    }
    template <typename T>
    void spawn2(std::ranges::input_range auto input, Output output, T &&callback)
    {
        OPTICK_EVENT();
        using stdexec::just_stopped;
        using stdexec::let_error;
        using stdexec::then;
        using stdexec::upon_error;
        stdexec::sender auto task_flow = processVideoPerFrame(std::move(input), std::move(output)) | then(callback) | let_error([](auto)
                                                                                                                                { std::cout << "Request stop" << std::endl; return just_stopped(); });
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
    template <typename T>
    using Expected = std::expected<T, std::exception_ptr>;
    using Unexpected = std::unexpected<std::exception_ptr>;
    template <typename T>
    class RangeReader
    {
    public:
        explicit RangeReader(std::ranges::input_range auto &&range)
            : RangeReader(std::ranges::begin(range), std::ranges::end(range))
        {
        }

        RangeReader(T begin, T end)
            : m_begin(begin), m_end(end)
        {
        }
        RangeReader(RangeReader&& o)
            : m_begin(std::move(o.m_begin))
            , m_end(std::move(o.m_end))
        {}

        std::optional<Image> operator()()
        {
            std::unique_lock lock(m_begin_mutex);
            if (m_begin == m_end)
            {
                return std::nullopt;
            }
            auto result = *m_begin;
            m_begin++;
            return result;
        }

        bool can_read() const
        {
            std::shared_lock lock(m_begin_mutex);
            return m_begin != m_end;
        }

    private:
        T m_begin;
        T m_end;
        mutable std::mutex m_begin_mutex;
    };

    struct Pipeline
    {
        bool colorize_enabled{true};
        bool resize_enabled{true};
        stdexec::sender auto scheduleOn(stdexec::scheduler auto scheduler, Image image)
        {
            using stdexec::just;
            using stdexec::let_value;
            using stdexec::then;
            return stdexec::on(scheduler, just(image)) | then([this](Image image)
                                                              { return colorize(image); }) |
                   then([this](Image image)
                        { return resize(image); }) |
                   let_value([this](Image image)
                             { return manipulateAlpha(image); });
        }
        Image colorize(Image image)
        {
            if (colorize_enabled)
            {
                image.colorize();
            }
            return image;
        }
        Image resize(Image image)
        {
            if (resize_enabled)
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

    void handle_error(std::exception_ptr error)
    {
        try
        {
            std::rethrow_exception(error);
        }
        catch (const std::runtime_error &ex)
        {
            std::cerr << "Error occurred: " << ex.what() << std::endl;
        }
        catch (...)
        {
            std::cerr << "Unknown error occurred" << std::endl;
        }
    }

    stdexec::sender auto readImage(Input *input)
    {
        OPTICK_EVENT();
        using stdexec::just;
        using stdexec::then;
        auto scheduler = m_pool.get_scheduler();
        auto read_image_callback =
            [](Input *input)
        {
            static uint32_t i = 0;
            OPTICK_THREAD(g_thread_name.c_str());
            if (i++ == 3)
            {
                // throw std::runtime_error("Error during read");
            }
            return input->read();
        };

        return stdexec::on(scheduler, just(input)) | then(read_image_callback);
    }
    stdexec::sender auto transform(Image image)
    {
        OPTICK_EVENT();
        using stdexec::just;
        using stdexec::then;
        auto scheduler = m_pool.get_scheduler();

        return m_pipeline.scheduleOn(scheduler, image);
    }

    stdexec::sender auto processVideoPerFrame(std::ranges::input_range auto input_range, Output output)
    {
        using stdexec::let_value;
        using stdexec::read_env;
        using stdexec::then;

        auto stream = std::make_shared<TaskStream<Expected<Image>>>(&m_scope);
        return stdexec::when_all(
            read_env(stdexec::get_stop_token) | let_value([this, range = std::move(input_range), output, stream](auto stop_token) mutable
                                                          { return readImages(RangeReader{std::ranges::begin(range), std::ranges::end(range)}, stream, stop_token); }),
            writeImages(std::move(output), stream));
    }

    exec::task<void> writeImages(Output output, std::shared_ptr<TaskStream<Expected<Image>>> stream)
    {
        using stdexec::just;
        using stdexec::on;
        using stdexec::then;
        using stdexec::when_all;
        OPTICK_EVENT();
        std::cout << "wait for stream: " << stream->size() << " is closed: " << stream->isClosed() << std::endl;
        while (stream->isClosed() == false || stream->hasWork())
        {
            Expected<Image> image = co_await (*stream);
            if (image.has_value())
            {
                std::cout << "Try write: " << image->getName() << std::endl;
                co_await (when_all(just(std::move(*image)), just(&output)) | then([](Image image, Output *output)
                                                                                  { output->write(image); }));
            }
            else
            {
                std::rethrow_exception(image.error());
                // handle_error(image.error());
                break;
            }
        }
    }
    exec::task<void> readImages(auto range_reader,
                                std::shared_ptr<TaskStream<Expected<Image>>> stream,
                                stdexec::inplace_stop_token stop_token)
    {
        using stdexec::just;
        using stdexec::on;
        using stdexec::then;
        using stdexec::upon_error;
        using stdexec::when_all;
        OPTICK_EVENT();

        scope_exit finally{[&]
                           { stream->close(); }};

        stdexec::sender_of<stdexec::set_value_t(std::optional<Image>)> auto reading_sender = stdexec::on(m_pool.get_scheduler(), just(&range_reader)) | then([](auto *range_reader)
                                                                                                                                                             { return (*range_reader)(); });
        AsyncReader<std::optional<Image>, decltype(reading_sender)> reader(reading_sender, &m_scope);
        auto as_expected = [](auto value)
        { return Expected<Image>{std::move(value)}; };
        auto as_unexpected = [](std::exception_ptr error) -> Expected<Image>
        { return Unexpected{std::move(error)}; };
        while (std::optional<Image> image = co_await reader.asyncRead())
        {
            if (stop_token.stop_requested())
            {
                break;
            }
            stream->push(transform(*image) | then(as_expected) | upon_error(as_unexpected));
        }
    }

    THREAD_POOL m_pool{32};
    exec::async_scope m_scope;
    Pipeline m_pipeline;
};
