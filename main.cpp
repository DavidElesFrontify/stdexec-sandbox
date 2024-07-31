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
constexpr const bool g_run_test = true;
static_assert(stdexec::sender<exec::task<void>>);

template<typename Res>
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
    QueueScheduler(exec::async_scope* scope)
    : m_scope(scope)
    {}

    ~QueueScheduler()
    {
        /*
        We cannot wait until the scope's thread is finished because the coroutine is moved inside
        the running threads. I.e.: When the coroutine owns the queue the queue destructor is called inside the
        coroutine. But when the coroutine is moved inside the scope it will be a dead lock.
        */
        //stdexec::sync_wait(m_scope->on_empty());
    }

    void push(stdexec::sender auto&& task)
    {
        using stdexec::let_value;
        
        auto skeleton_it = [this]
        {
            auto lock = std::unique_lock(m_results_mutex);
            return m_results.insert(m_results.end(), std::nullopt);
        }();
        auto set_skeleton = [this, skeleton_it](Res result) mutable 
        {
            *skeleton_it = std::move(result);
            if(isReady())
            {
                auto* awaiter = m_awaiter.exchange(nullptr);
                if(awaiter != nullptr)
                {
                    awaiter->awaiting_coroutine.resume();
                }
            }
        };

        m_scope->spawn( task | stdexec::then(set_skeleton));
    }

    std::optional<Res> head() const
    {
        std::shared_lock lock(m_results_mutex);
        return m_results.empty() ? std::nullopt : m_results.front();
    }

    bool empty() const
    {
        std::shared_lock lock(m_results_mutex);
        return m_results.empty();
    }
    uint32_t size() const
    {
        std::shared_lock lock(m_results_mutex);
        return m_results.size();
    }

    Res pop()
    {
        auto lock = std::unique_lock(m_results_mutex);
        if(m_results.empty())
        {
            throw std::runtime_error("Queue is emoty, can't pop");
        }
        if(m_results.front().has_value() == false)
        {
            throw std::runtime_error("Queue is not ready, can't pop");
        }
        auto result = std::move(*m_results.front());
        m_results.pop_front();
        return result;
    }
    Awaiter operator co_await()
    {
        return Awaiter{this};
    }

    bool isReady() const
    {
        return head() != std::nullopt;
    }
private:
    std::list<std::optional<Res>> m_results;
    mutable std::shared_mutex m_results_mutex;
    std::atomic<Awaiter*> m_awaiter {nullptr};

    exec::async_scope* m_scope{nullptr};
};

template<typename T, stdexec::sender_of<stdexec::set_value_t(T)> U>
class AsyncReader
{
private:
    static constexpr const uint32_t BACKBUFFER_SIZE = 2;
public:
    using Sender = U;
    using Result = T;
    AsyncReader(Sender sender, exec::async_scope* scope)
        : m_sender(std::move(sender))
        , m_scope(scope)
    {
        asyncReadImpl();
    }

    
    struct [[nodiscard]] Awaiter 
    {
        AsyncReader* reader {nullptr};
        std::coroutine_handle<> awaiting_coroutine = std::noop_coroutine();
        bool await_ready() { return reader->isReady(); }
        bool await_suspend(std::coroutine_handle<> handle)
        {
            if(reader->m_awaiting_coroutine != nullptr)
            {
                throw std::runtime_error("Only one co_await is supported");
            }
            std::unique_lock lock(reader->m_backbuffer_mutex);
            awaiting_coroutine = handle;
            /*
            Lock is necessary to protect both cases, imagine that else is not part:
             - [ThreadA] reader.hasValue()? -> false & release lock
             - [ThreadB] reader.setValue() & release lock
             - [ThreadB] reader.resumeAwaitingCoro() (which is null right now)
             - [ThreadA] reader.setAwawitingCoro() (too late)
            */
            if(reader->m_backbuffer[reader->getReadIndex()].has_value())
            {
                return false;
            }
            else
            {
                reader->m_awaiting_coroutine = this;
                return true;
            }
        }

        Result await_resume() 
        {
            Result result = *reader->clear();
            reader->asyncReadImpl();
            return result;
        }

        explicit Awaiter(AsyncReader* reader)
        : reader(reader)
        {}
    };

    Awaiter asyncRead()
    {
        return Awaiter{this};
    }
private:
    void asyncReadImpl()
    {
        using stdexec::then;
        m_scope->spawn(m_sender | then([this](Result result) { setResult(std::move(result)); resumeAwaitingCoroutine(); } ));
    }
    void setResult(Result result)
    {
        std::lock_guard lock(m_backbuffer_mutex);
        if(m_backbuffer[getWriteIndex()] != std::nullopt)
        {
            throw std::runtime_error("Data lost");
        }
        m_backbuffer[getWriteIndex()] = std::move(result);
        stepBackbuffer();
    }
    bool isReady() const 
    {
        std::lock_guard lock(m_backbuffer_mutex);
        return m_backbuffer[getReadIndex()].has_value();
    }
    std::optional<Result> clear()
    {
        std::lock_guard lock(m_backbuffer_mutex);
        auto result = std::move(m_backbuffer[getReadIndex()]);
        m_backbuffer[getReadIndex()] = std::nullopt;
        return result;
    }
    // Should be outside of the locking scope to avoid keep locking the scope while the coroutine runs
    void resumeAwaitingCoroutine()
    {
        if(m_awaiting_coroutine != nullptr)
        {
            auto coroutine = m_awaiting_coroutine.exchange(nullptr);
            if(coroutine != nullptr)
            {
                coroutine->awaiting_coroutine.resume();
            }
        }
    }

    uint32_t getWriteIndex() const
    {
        return m_head_index;
    }
    uint32_t getReadIndex() const
    {
        return (m_head_index + m_backbuffer.size() - 1) % m_backbuffer.size();
    }
    void stepBackbuffer() 
    {
        m_head_index = (m_head_index + 1) % m_backbuffer.size();
    }
    Sender m_sender;
    exec::async_scope* m_scope {nullptr};
    mutable std::mutex m_backbuffer_mutex;
    std::array<std::optional<Result>, BACKBUFFER_SIZE> m_backbuffer {};
    uint32_t m_head_index{0};
    std::atomic<Awaiter*> m_awaiting_coroutine {nullptr};
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
        std::cout << "WAT: Schedule read" << std::endl;

        return stdexec::on(scheduler, just(input)) | then([](Input* input) { OPTICK_THREAD(g_thread_name.c_str()); return input->read(); });
    }
    std::optional<Image> readImage(Input& input)
    {
        OPTICK_THREAD(g_thread_name.c_str());
        return input.read();
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
    stdexec::sender auto saveImage(Image image, Output* output)
    {
        OPTICK_EVENT();
        using stdexec::just;
        using stdexec::then;
        using stdexec::when_all;
        auto scheduler = m_pool.get_scheduler();

        return stdexec::on(scheduler, when_all(just(image), just(output))) | then([](const Image& image, Output* output) { output->write(image); });
    }

    stdexec::sender auto processFrame(Input* input)
    {
        using stdexec::on;
        using stdexec::then;
        using stdexec::just;
        auto scheduler = m_pool.get_scheduler();
        return on(scheduler, just(input)) 
        | then([this](Input* input) { return readImage(*input);})
        | then([this](std::optional<Image> image) 
                {
                    return image.and_then([this](auto & img)
                        { 
                            return std::optional{doTransform(img)};
                        });
                });
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

