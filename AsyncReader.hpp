#pragma once

#include <stdexec/execution.hpp>
#include <exec/async_scope.hpp>

#include <chrono>
#include <iostream>
#include <thread>
#include <shared_mutex>
#include <list>
#include <condition_variable>

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