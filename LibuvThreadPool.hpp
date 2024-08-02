#pragma once
#include <tbbexec/tbb_thread_pool.hpp>

#include <uv.h>

#include <cstdlib>
#include <iostream>


class LibuvTaskArena
{
    struct WorkRequest
    {
        template<typename U>
            requires std::convertible_to<U, std::function<void()>>
        WorkRequest(U&& callback)
            : callback(std::forward<U>(callback))
        {
            request.data = this;
        }
        std::function<void()> callback;
        uv_work_t request{};
    };
    static void onWork(uv_work_t* request_ptr)
    {
        auto* work_request = reinterpret_cast<WorkRequest*>(request_ptr->data);
        work_request->callback();
    }
    static void onDone(uv_work_t* request_ptr, int status)
    {
        auto* work_request = reinterpret_cast<WorkRequest*>(request_ptr->data);
        delete work_request;
    }
    public:
        explicit LibuvTaskArena(uv_loop_t* main_loop)
        : m_main_loop(main_loop)
        {}
        uint32_t getMaxConcurrency() const
        {
            auto max_concurrency = readThreadCountFromEnv();
            return max_concurrency.value_or(4);
        }
        template<typename T>
        void enqueue(T&& callback)
        {
            auto work_request = std::make_unique<WorkRequest>(std::forward<T>(callback));
            uv_queue_work(m_main_loop, &work_request->request, onWork, onDone);
            work_request.release();
        }
    private:
    uv_loop_t* m_main_loop {nullptr};

    std::optional<uint32_t> readThreadCountFromEnv() const
    {
        const char* thread_count_str = std::getenv("UV_THREADPOOL_SIZE");
        return thread_count_str == nullptr ? std::nullopt : std::optional<uint32_t>(std::atoi(thread_count_str));
    }
};

class LibuvThreadPool : public tbbexec::_thpool::thread_pool_base<LibuvThreadPool> {
   public:
    explicit LibuvThreadPool(uv_loop_t* main_loop)
      : m_arena{main_loop} {
    }

    [[nodiscard]]
    auto available_parallelism() const -> std::uint32_t {
      return m_arena.getMaxConcurrency();
    }
   private:
    [[nodiscard]]
    static constexpr auto forward_progress_guarantee() -> stdexec::forward_progress_guarantee {
      return stdexec::forward_progress_guarantee::parallel;
    }

    friend tbbexec::_thpool::thread_pool_base<LibuvThreadPool>;

    template <class PoolType, class ReceiverId>
    friend struct tbbexec::_thpool::operation;

    void enqueue(tbbexec::_thpool::task_base* task, std::uint32_t tid = 0) noexcept {
      m_arena.enqueue([task, tid] { task->__execute(task, /*tid=*/tid); });
}

    LibuvTaskArena m_arena;
};