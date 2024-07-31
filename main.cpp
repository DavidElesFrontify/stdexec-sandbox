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

constexpr const bool g_enable_capture = true;
constexpr const bool g_run_test = true;
static_assert(stdexec::sender<exec::task<void>>);

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

class Server
{
    public:
    void startProcessing(Input input, Output output)
    {

        OPTICK_EVENT();
        m_context.spawn2(std::move(input), std::move(output), [](){std::cout << "Processing finished;" << std::endl;});
    }
    void actBusy()
    {
        OPTICK_EVENT("HandlingRequestOrWhatever");
        using namespace std::chrono_literals;
        busyWait(durations::busy_operation);
    }
    void run()
    {
        OPTICK_EVENT();
        startProcessing(Input{"Input 01"}, Output{});
        /*startProcessing(Input{"Input 02"}, Output{});
        startProcessing(Input{"Input 03"}, Output{});
        startProcessing(Input{"Input 04"}, Output{});
        startProcessing(Input{"Input 05"}, Output{});
        startProcessing(Input{"Input 06"}, Output{});
        startProcessing(Input{"Input 07"}, Output{});
        startProcessing(Input{"Input 08"}, Output{});
        startProcessing(Input{"Input 09"}, Output{});
        startProcessing(Input{"Input 10"}, Output{});
        startProcessing(Input{"Input 11"}, Output{});
        startProcessing(Input{"Input 12"}, Output{});
        startProcessing(Input{"Input 13"}, Output{});
        startProcessing(Input{"Input 14"}, Output{});
        startProcessing(Input{"Input 15"}, Output{});*/
        actBusy();
    }

    private:
    Context m_context;
};


exec::task<void> testQueue(QueueScheduler<int>& queue, stdexec::scheduler auto scheduler)
{
    using stdexec::on;
    using stdexec::just;
    using stdexec::then;    
    using namespace std::chrono_literals;

    queue.push(on(scheduler, just(42)) | then([](int x) {busyWait(10s); return x;}));
    queue.push(on(scheduler, just(41)) | then([](int x) {busyWait(5s); return x;}));
    queue.push(on(scheduler, just(40)) | then([](int x) {busyWait(10s); return x;}));
    std::cout << "start wait: " << std::endl;
    while(int x = co_await queue)
    {
        queue.push(on(scheduler, just (x - 3)));
        std::cout << x << std::endl;
    }
    std::cout << "End test" << std::endl;
}

int main()
{
    if constexpr(g_run_test)
    {
        if constexpr(g_enable_capture)
        {
            OPTICK_START_CAPTURE();
        }
        {
            OPTICK_FRAME("MainThread");
            Server server;
            server.run();
        }
        if constexpr(g_enable_capture)
        {
            OPTICK_STOP_CAPTURE();
            OPTICK_SAVE_CAPTURE("OptickCapture.opt");
        }
    }
    else
    {
        exec::static_thread_pool pool{3};
        
        auto scheduler = pool.get_scheduler();
        exec::async_scope scope;
        QueueScheduler<int> queue(&scope);
        stdexec::sync_wait(testQueue(queue, scheduler));
    }
    return 0;
}

