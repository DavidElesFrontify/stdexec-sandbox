#pragma once
#include <atomic>
#include <coroutine>

class SingleShotEvent
{
    public:
        class [[nodiscard]] Awaiter
        {
            public:
                explicit Awaiter(SingleShotEvent& event)
                : m_event(event)
                {}
                bool await_ready() { return false; }
                bool await_suspend(std::coroutine_handle<> handle)
                {
                    m_awaiting_handle = handle;
                    void* old_value = m_event.getNotSetStateValue();
                    if(m_event.m_awaiting_state.compare_exchange_strong(
                        old_value,
                        &m_awaiting_handle,
                        std::memory_order_release,
                        std::memory_order_relaxed) == false)
                    {
                        assert(old_value == m_event.getTriggeredStateValue());
                        m_event.m_awaiting_state.exchange(nullptr, std::memory_order_acquire);
                        return false;
                    }
                    return true;
                }
                void await_resume() {}
            private:
                SingleShotEvent& m_event;
                std::coroutine_handle<> m_awaiting_handle = std::noop_coroutine();
        };
    public:
        SingleShotEvent()
        : m_awaiting_state(getNotSetStateValue())
        {}
        void set()
        {
            void* old_value = m_awaiting_state.exchange(this, std::memory_order_acquire);
            if(isWaitingState(old_value))
            {
                reset();
                auto* handle = reinterpret_cast<std::coroutine_handle<>*>(old_value);
                handle->resume();
            }
        }

        Awaiter operator co_await() 
        {
            return Awaiter{*this};
        }
    private:
        void reset()
        {
            m_awaiting_state.exchange(nullptr, std::memory_order_acq_rel);
        }
        const void* getTriggeredStateValue() const { return this; }
        void* getTriggeredStateValue() { return this; }
        void* getNotSetStateValue() const { return nullptr; }
        bool isWaitingState(void* state) const
        {
            return state != getTriggeredStateValue() && state != getNotSetStateValue();
        }
        std::atomic<void*> m_awaiting_state;
        std::atomic_bool m_finished {false};
};