#pragma once

#include <stdexec/execution.hpp>

#if 1

namespace RetryInternal
{
    template<typename S, typename R>
    struct Operator;

    template<typename S, typename R>
    struct Receiver: stdexec::receiver_adaptor<Receiver<S, R>>
    {
        Operator<S, R>* m_op {nullptr};

        R&& base() && noexcept
        {
            return std::move(m_op->m_reciever);
        }
        const R& base() const & noexcept
        {
            return m_op->m_reciever;
        }

        explicit Receiver(Operator<S, R>* op)
        : m_op(op)
        {}

        template<typename Error>
        void set_error(Error&&)&& noexcept {
            m_op->rerty();
        }
    };

    template<typename S>
    struct Sender 
    {
        using sender_concept = stdexec::sender_t;
        S m_sender;

        explicit Sender(S s)
        : m_sender(std::move(s))
        {}

        template<typename>
        using error_signiture = stdexec::completion_signatures<>;
        template<typename... Ts>
        using value_signiture = stdexec::completion_signatures<stdexec::set_value_t(Ts...)>;

        template<typename Env>
        auto get_completion_signatures(Env&&) const ->
            stdexec::transform_completion_signatures_of<
                S&,
                Env,
                stdexec::completion_signatures<stdexec::set_error_t(std::exception_ptr)>,
                value_signiture,
                error_signiture>
        {
            return {};
        }
        template<stdexec::receiver R>
        friend Operator<S, R> tag_invoke(stdexec::connect_t, Sender&& self, R r)
        {
            return {std::move(self.m_sender), std::move(r)};
        }

        stdexec::env_of_t<S> get_env() const noexcept
        {
            return stdexec::get_env(m_sender);
        }
    };

    template<typename S, typename R>
    struct Operator
    {
        S m_sender;
        R m_reciever;
        std::optional<stdexec::connect_result_t<S&, Receiver<S, R>>> m_connect_result {};

        Operator(S s, R r)
        : m_sender(std::move(s))
        , m_reciever(std::move(r))
        {}

        Operator(Operator&&) = delete;

        auto connect() noexcept 
        {
            return [this] { return stdexec::connect(m_sender, Receiver<S, R>(this)); };
        }

        void retry() noexcept
        {
            try
            {
                m_connect_result.emplace(connect());
                stdexec::start(*m_connect_result);
            }
            catch(...)
            {
                stdexec::set_error(std::move(m_reciever), std::current_exception);
            }
        }
        friend void tag_invoke(stdexec::start_t, Operator& op) noexcept
        {
            stdexec::start(*op.m_connect_result);
        }
    };
}

template<stdexec::sender S>
stdexec::sender auto retry(S s)
{
    return RetryInternal::Sender(std::move(s));
}
#else
///////////////////////////////////////////////////////////////////////////////
// retry algorithm:

// _conv needed so we can emplace construct non-movable types into
// a std::optional.
template <std::invocable F>
  requires std::is_nothrow_move_constructible_v<F>
struct _conv {
  F f_;

  explicit _conv(F f) noexcept
    : f_(static_cast<F&&>(f)) {
  }

  operator std::invoke_result_t<F>() && {
    return static_cast<F&&>(f_)();
  }
};

template <class S, class R>
struct _op;

// pass through all customizations except set_error, which retries the operation.
template <class S, class R>
struct _retry_receiver : stdexec::receiver_adaptor<_retry_receiver<S, R>> {
  _op<S, R>* o_;

  R&& base() && noexcept {
    return static_cast<R&&>(o_->r_);
  }

  const R& base() const & noexcept {
    return o_->r_;
  }

  explicit _retry_receiver(_op<S, R>* o)
    : o_(o) {
  }

  template <class Error>
  void set_error(Error&&) && noexcept {
    o_->_retry(); // This causes the op to be retried
  }
};

// Hold the nested operation state in an optional so we can
// re-construct and re-start it if the operation fails.
template <class S, class R>
struct _op {
  S s_;
  R r_;
  std::optional<stdexec::connect_result_t<S&, _retry_receiver<S, R>>> o_;

  _op(S s, R r)
    : s_(static_cast<S&&>(s))
    , r_(static_cast<R&&>(r))
    , o_{_connect()} {
  }

  _op(_op&&) = delete;

  auto _connect() noexcept {
    return _conv{[this] {
      return stdexec::connect(s_, _retry_receiver<S, R>{this});
    }};
  }

  void _retry() noexcept {
    try {
      o_.emplace(_connect()); // potentially throwing
      stdexec::start(*o_);
    } catch (...) {
      stdexec::set_error(static_cast<R&&>(r_), std::current_exception());
    }
  }

  friend void tag_invoke(stdexec::start_t, _op& o) noexcept {
    stdexec::start(*o.o_);
  }
};

template <class S>
struct _retry_sender {
  using sender_concept = stdexec::sender_t;
  S s_;

  explicit _retry_sender(S s)
    : s_(static_cast<S&&>(s)) {
  }

  template <class>
  using _error = stdexec::completion_signatures<>;
  template <class... Ts>
  using _value = stdexec::completion_signatures<stdexec::set_value_t(Ts...)>;

  template <class Env>
  auto get_completion_signatures(Env&&) const -> stdexec::transform_completion_signatures_of<
    S&,
    Env,
    stdexec::completion_signatures<stdexec::set_error_t(std::exception_ptr)>,
    _value,
    _error> {
    return {};
  }

  template <stdexec::receiver R>
  friend auto tag_invoke(stdexec::connect_t, _retry_sender&& self, R r) -> _op<S, R> {
    return {static_cast<S&&>(self.s_), static_cast<R&&>(r)};
  }

  auto get_env() const noexcept -> stdexec::env_of_t<S> {
    return stdexec::get_env(s_);
  }
};

template <stdexec::sender S>
stdexec::sender auto retry(S s) {
  return _retry_sender{static_cast<S&&>(s)};
}

#endif