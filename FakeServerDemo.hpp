#pragma once

#include "Context.hpp"
#include <tbbexec/tbb_thread_pool.hpp>
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
        startProcessing(Input{"Input 02"}, Output{});
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
        startProcessing(Input{"Input 15"}, Output{});
        actBusy();
    }

    private:
    Context<tbbexec::tbb_thread_pool> m_context{32};
};


exec::task<void> testQueue(TaskStream<int>& queue, stdexec::scheduler auto scheduler)
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

void runFakeServer()
{
    Server server;
    server.run();
}