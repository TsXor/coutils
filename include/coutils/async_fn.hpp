#pragma once
#ifndef __COUTILS_DETAIL_ASYNC_FN__
#define __COUTILS_DETAIL_ASYNC_FN__

#include <coroutine>
#include <optional>
#include <utility>
#include "coutils/detail/transfer_to_handle.hpp"
#include "coutils/initializer_tuple.hpp"

namespace coutils {

// wraps coroutine like an async function, won't be executed unless you co_await it
// `co_return coutils::inituple(...)` may be used as an equivalent of `return {...}`
// Note: returned object will be at least constructed once and moved once.
//       If you hate such behaviour, consider using reference parameters to output value.
template <typename T>
struct async_fn {
    struct promise_type;
    using callee_type = std::coroutine_handle<promise_type>;
    callee_type callee;
    std::optional<T> retval = std::nullopt;
    
    constexpr bool await_ready() const noexcept { return false; }
    
    async_fn(promise_type& callee_promise);
    async_fn(const async_fn<T>&) = delete;
    async_fn(async_fn<T>&& other) : callee(std::exchange(other.callee, {})),
        retval(std::exchange(other.retval, std::nullopt)) {}
    ~async_fn();

    operator callee_type() { return callee; }
    operator std::coroutine_handle<void>() { return callee; }

    callee_type await_suspend(std::coroutine_handle<> ch);
    T await_resume() { return *std::move(retval); }
    void manual_resume() { callee.resume(); }
};

template <typename T>
struct async_fn<T>::promise_type {
    std::coroutine_handle<> caller = std::noop_coroutine();
    std::optional<T>* retval_ptr = nullptr;
    
    auto get_return_object() { return async_fn(*this); }
    auto initial_suspend() noexcept { return std::suspend_always{}; }
    auto final_suspend() noexcept { return detail::transfer_to_handle{caller}; }
    
    template <typename ReturnExprT>
    void return_value(ReturnExprT&& expr) {
        if (retval_ptr) {
            if constexpr (is_initializer_tuple_ref_v<ReturnExprT>) {
                emplace_apply(*retval_ptr, std::forward<ReturnExprT>(expr));
            } else {
                retval_ptr->emplace(std::forward<ReturnExprT>(expr));
            }
        }
    }

    // TODO: exception handling
    void unhandled_exception() {}
};

template<class T>
inline async_fn<T>::async_fn(promise_type& callee_promise):
callee(callee_type::from_promise(callee_promise)) {
    callee_promise.retval_ptr = &retval;
}

template<class T>
inline async_fn<T>::~async_fn() {
    if (callee && !callee.done()) { callee.promise().retval_ptr = nullptr; }
}

template<class T>
inline async_fn<T>::callee_type async_fn<T>::await_suspend(std::coroutine_handle<> ch) {
    callee.promise().caller = ch; return callee;
}


// no return value specialization
template <>
struct async_fn<void> {
    struct promise_type;
    using callee_type = std::coroutine_handle<promise_type>;
    callee_type callee;
    
    constexpr bool await_ready() const noexcept { return false; }
    
    async_fn(promise_type& callee_promise);
    async_fn(const async_fn<void>&) = delete;
    async_fn(async_fn<void>&& other) : callee(std::exchange(other.callee, {})) {}
    ~async_fn();

    operator callee_type() { return callee; }
    operator std::coroutine_handle<void>() { return callee; }

    callee_type await_suspend(std::coroutine_handle<> ch);
    void await_resume() {}
    void manual_resume() { callee.resume(); }
};

struct async_fn<void>::promise_type {
    std::coroutine_handle<> caller = std::noop_coroutine();
    
    auto get_return_object() { return async_fn(*this); }
    auto initial_suspend() noexcept { return std::suspend_always{}; }
    auto final_suspend() noexcept { return detail::transfer_to_handle{caller}; }
    
    void return_void() {}

    // TODO: exception handling
    void unhandled_exception() {}
};

inline async_fn<void>::async_fn(promise_type& callee_promise):
callee(callee_type::from_promise(callee_promise)) {}

inline async_fn<void>::~async_fn() {}

inline async_fn<void>::callee_type async_fn<void>::await_suspend(std::coroutine_handle<> ch) {
    callee.promise().caller = ch; return callee;
}

} // namespace coutils

#endif // __COUTILS_DETAIL_ASYNC_FN__
