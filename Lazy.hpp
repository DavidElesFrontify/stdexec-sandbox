#pragma once

#include <exec/task.hpp>

template<typename T>
class [[nodiscard]] Lazy: public exec::task<T>
{
    public:
        // Inherit constructors
        using exec::task<T>::task;
        Lazy(Lazy<T>&& o) = default;
        Lazy(exec::task<T>&& o)
            : exec::task<T>(std::move(o))
        {}
        T operator*()
        {
            return stdexec::sync_wait(*this).value();
        }
};