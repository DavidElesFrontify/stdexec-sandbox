#pragma once

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
            awaiting_coroutine = handle;
            Awaiter* expected = nullptr;
            const bool success = scheduler->m_awaiter.compare_exchange_strong(expected,
                                                                              this,
                                                                              std::memory_order_release,
                                                                              std::memory_order_relaxed);
            if(success == false)
            {
                throw std::runtime_error("Only one await is supported");
            }
        }

        Res await_resume() 
        {
            if(auto exception = scheduler->resetError(); exception != nullptr) 
            {
                std::rethrow_exception(exception);
            } 
            return scheduler->pop(); 
        }
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
        using stdexec::upon_error;
        if(isClosed())
        {
            throw std::runtime_error("Can't push into a closed queue");
        }
        
        auto skeleton_it = [this]
        {
            auto lock = std::unique_lock(m_results_mutex);
            return m_results.insert(m_results.end(), std::nullopt);
        }();
        auto set_skeleton = [this, skeleton_it](Result result) mutable 
        {
            *skeleton_it = std::move(result);
            if(isReady())
            {
                auto* awaiter = m_awaiter.exchange(nullptr, std::memory_order_acq_rel);
                if(awaiter != nullptr)
                {
                    awaiter->awaiting_coroutine.resume();
                }
            }
        };
        auto on_error = [this, skeleton_it ](std::exception_ptr error) mutable 
        {
            setError(error);
            {
                auto lock = std::unique_lock(m_results_mutex);
                m_results.erase(skeleton_it);
            }
            {
                auto* awaiter = m_awaiter.exchange(nullptr, std::memory_order_acq_rel);
                if(awaiter != nullptr)
                {
                    awaiter->awaiting_coroutine.resume();
                }
            }
        };

        m_scope->spawn( task | stdexec::then(set_skeleton) | upon_error(on_error));
    }

    std::optional<Result> head() const
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

    Result pop()
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

    bool hasWork()
    {
        auto lock = std::shared_lock(m_results_mutex);
        return m_results.empty() == false;
    }

    void close() 
    {
        m_closed = true;
    }

    bool isClosed() const
    {
        return m_closed.load();
    }

    bool isReady() const
    {
        return head() != std::nullopt;
    }

private:
    void setError(std::exception_ptr exception)
    {
        std::lock_guard lock(m_exception_mutex);
        m_last_error = exception;
    }
    std::exception_ptr resetError()
    {
        std::lock_guard lock(m_exception_mutex);
        auto result = m_last_error;
        m_last_error = nullptr;
        return result;
    }

    std::list<std::optional<Result>> m_results;
    mutable std::shared_mutex m_results_mutex;
    std::atomic<Awaiter*> m_awaiter {nullptr};
    mutable std::shared_mutex m_exception_mutex;
    std::exception_ptr m_last_error {nullptr};
    std::atomic_bool m_closed{ false };

    exec::async_scope* m_scope{nullptr};
};