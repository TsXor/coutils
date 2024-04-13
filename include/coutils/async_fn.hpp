#pragma once
#ifndef __COUTILS_DETAIL_ASYNC_FN__
#define __COUTILS_DETAIL_ASYNC_FN__

#include <coroutine>
#include <optional>
#include "coutils/detail/transfer_to_handle.hpp"

namespace coutils {

// wraps coroutine like an async function
// won't be executed unless you co_await it
// Note: returned object will be at least constructed once and moved once.
//       If you hate such behaviour, consider using reference parameters to output value.
template <typename T>
struct async_fn {
    struct promise_type;
    using callee_type = std::coroutine_handle<promise_type>;
    callee_type callee;
    std::optional<T> retval = std::nullopt;
    
    constexpr bool await_ready() const noexcept { return false; }
    
    async_fn(promise_type& callee_promise):
        callee(callee_type::from_promise(callee_promise)) {
            callee_promise.retval_ptr = &retval;
        }
    async_fn(const async_fn<T>&) = delete;
    async_fn(async_fn<T>&&) = default;
    ~async_fn() = default;

    operator callee_type() { return callee; }

    auto await_suspend(std::coroutine_handle<> ch) {
        callee.promise().caller = ch;
        return callee;
    }
    T&& await_resume() { return *std::move(retval); }
    void manual_resume() { callee.resume(); }

    struct promise_type {
        std::coroutine_handle<> caller = std::noop_coroutine();
        std::optional<T>* retval_ptr = nullptr;
        auto get_return_object() { return async_fn(*this); }
        template <typename RetT>
        void return_value(RetT&& ret) { if (retval_ptr) { retval_ptr->emplace(std::move(ret)); } }
        auto initial_suspend() { return std::suspend_always{}; }
        auto final_suspend() noexcept { return detail::transfer_to_handle{caller}; }
        void unhandled_exception() {}
    };
};

// no return value specialization
template <>
struct async_fn<void> {
    struct promise_type;
    using callee_type = std::coroutine_handle<promise_type>;
    callee_type callee;
    
    constexpr bool await_ready() const noexcept { return false; }
    
    async_fn(promise_type& callee_promise):
        callee(callee_type::from_promise(callee_promise)) {}
    async_fn(const async_fn<void>&) = delete;
    async_fn(async_fn<void>&&) = default;
    ~async_fn() = default;

    operator callee_type() { return callee; }

    auto await_suspend(std::coroutine_handle<> ch) {
        callee.promise().caller = ch;
        return callee;
    }
    void await_resume() {}
    void manual_resume() { callee.resume(); }

    struct promise_type {
        std::coroutine_handle<> caller = std::noop_coroutine();
        auto get_return_object() { return async_fn(*this); }
        void return_void() {}
        auto initial_suspend() { return std::suspend_always{}; }
        auto final_suspend() noexcept { return detail::transfer_to_handle{caller}; }
        void unhandled_exception() {}
    };
};

} // namespace coutils

#endif // __COUTILS_DETAIL_ASYNC_FN__
