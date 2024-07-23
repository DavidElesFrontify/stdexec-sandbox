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

#include "Image.hpp"
#include "Input.hpp"
#include "Output.hpp"
#include "Transformator.hpp"
#include "util.hpp"
#include "durations.hpp"

constexpr const bool g_enable_capture = true;
constexpr const bool g_run_test = false;
static_assert(stdexec::sender<exec::task<void>>);

template<typename Res, stdexec::scheduler Sched>
class QueueScheduler
{
public:

    struct Awaiter
    {
        QueueScheduler* scheduler {nullptr};
        std::coroutine_handle<> awaiting_coroutine = std::noop_coroutine();
        bool await_ready() { return scheduler->isReady(); }
        void await_suspend(std::coroutine_handle<> handle)
        {
            std::cout << "Suspend execution" << std::endl;
            if(scheduler->m_awaiter != nullptr)
            {
                throw std::runtime_error("Queue is already awaited. Multiple await is not supported");
            }
            awaiting_coroutine = handle;
            scheduler->m_awaiter = this;
        }

        Res await_resume() { return scheduler->pop(); }
    };
    using Result = Res;
    explicit QueueScheduler(Sched scheduler)
        : m_scheduler(std::move(scheduler))
    {}

    ~QueueScheduler()
    {
        stdexec::sync_wait(m_scope.on_empty());
    }

    void push(stdexec::sender auto&& task)
    {
        auto lock = std::unique_lock(m_results_mutex);
        
        auto skeleton_it = m_results.insert(m_results.end(), std::nullopt);
        auto set_skeleton = [this, skeleton_it](Res result) mutable 
        {
            *skeleton_it = std::move(result);
            std::cout << "Finished: " << result << std::endl;
            if(isReady())
            {
                auto* awaiter = m_awaiter.exchange(nullptr);
                if(awaiter != nullptr)
                {
                    std::cout << "Resume coroutine: " << std::endl;
                    awaiter->awaiting_coroutine.resume();
                }
            }
            std::cout << "Task finished: " << result;
        };
        std::cout << "Schedule task" << std::endl;
        m_scope.spawn(task | stdexec::then(set_skeleton));
    }

    std::optional<Res> head() const
    {
        std::shared_lock lock(m_results_mutex);
        return m_results.empty() ? std::nullopt : m_results.front();
    }

    Res pop()
    {
        auto lock = std::unique_lock(m_results_mutex);
        if(m_results.empty())
        {
            throw std::runtime_error("Queue is emoty, can't pop");
        }
        if(m_results.front() == std::nullopt)
        {
            throw std::runtime_error("Queue is not ready, can't pop");
        }
        auto result = std::move(*m_results.front());
        m_results.pop_front();
        std::cout << "pop " << result << std::endl;
        return result;
    }
    Awaiter operator co_await()
    {
        return Awaiter{this};
    }

    Sched& getScheduler() { return m_scheduler; }

    bool isReady() const
    {
        return head() != std::nullopt;
    }
private:
    std::list<std::optional<Res>> m_results;
    mutable std::shared_mutex m_results_mutex;
    std::atomic<Awaiter*> m_awaiter {nullptr};

    exec::async_scope m_scope;
    Sched m_scheduler;
};

class Context
{
public:

    template<typename T> 
    void spawn(Input input, Output output, T&& callback)
    {
        OPTICK_EVENT();
        using stdexec::then;
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


template<typename T>
exec::task<void> testQueue(T& queue)
{
    using stdexec::on;
    using stdexec::just;
    using stdexec::then;    
    using namespace std::chrono_literals;

    queue.push(on(queue.getScheduler(), just(42)) | then([](int x) {busyWait(10s); return x;}));
    queue.push(on(queue.getScheduler(), just(41)) | then([](int x) {busyWait(5s); return x;}));
    queue.push(on(queue.getScheduler(), just(40)) | then([](int x) {busyWait(10s); return x;}));
    std::cout << "start wait: " << std::endl;
    while(int x = co_await queue)
    {
        std::cout << "schedule new task: " << x  - 1 << std::endl;
        queue.push(on(queue.getScheduler(), just (x - 1)));
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

        QueueScheduler<int, decltype(scheduler)> queue(scheduler);
        stdexec::sync_wait(testQueue(queue));
    }
    return 0;
}

