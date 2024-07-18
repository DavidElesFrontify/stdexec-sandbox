#include <optick.h>

#include <chrono>
#include <iostream>
#include <thread>

#include <stdexec/execution.hpp>
#include <exec/task.hpp>
#include <exec/static_thread_pool.hpp>
#include <exec/async_scope.hpp>

#include "Image.hpp"
#include "Input.hpp"
#include "Output.hpp"
#include "Transformator.hpp"
#include "util.hpp"
#include "durations.hpp"

constexpr const bool g_enable_capture = true;
static_assert(stdexec::sender<exec::task<void>>);
class Context
{
public:

    template<typename T> 
    void spawn(Input input, Output output, T&& callback)
    {
        OPTICK_EVENT();
        using stdexec::then;
        auto scheduler = m_pool.get_scheduler();
        stdexec::sender auto task_flow = processVideo(std::move(input), std::move(output)) | then(callback);
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

        return stdexec::on(scheduler, just(input)) | then([](Input* input) { OPTICK_THREAD(g_thread_name.c_str())return input->read(); });
    }
    stdexec::sender auto transform(Image image)
    {
        OPTICK_EVENT();
        using stdexec::just;
        using stdexec::then;
        auto scheduler = m_pool.get_scheduler();

        return stdexec::on(scheduler, just(image)) | then([](const Image& image) { return Transform{}.transform(image); });

    }
    stdexec::sender auto saveImage(Image image, Output* output)
    {
        OPTICK_EVENT();
        using stdexec::just;
        using stdexec::then;
        using stdexec::when_all;
        auto scheduler = m_pool.get_scheduler();

        return stdexec::on(scheduler, when_all(just(image), just(output))) | then([](const Image& image, Output* output) { output->write(image); });
    }

    exec::task<void> processVideo(Input input, Output output)
    {
        OPTICK_EVENT();
        while(std::optional<Image> image = co_await readImage(&input))
        {
            co_await transform(*image);
            co_await saveImage(*image, &output);
        }
    }

    exec::static_thread_pool m_pool{32};
    exec::async_scope m_scope;
};

class Server
{
    public:
    void startProcessing(Input input, Output output)
    {

        OPTICK_EVENT();
        m_context.spawn(std::move(input), std::move(output), [](){std::cout << "Processing finished;" << std::endl;});
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
    Context m_context;
};

int main()
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
    return 0;
}

