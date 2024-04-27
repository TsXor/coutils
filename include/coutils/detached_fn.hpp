#pragma once
#ifndef __COUTILS_DETACHED_FN__
#define __COUTILS_DETACHED_FN__

#include <coroutine>
#include <utility>
#include "coutils/initializer_tuple.hpp"

namespace coutils {

// no return value specialization
struct detached_fn {
    struct promise_type;
    using callee_type = std::coroutine_handle<promise_type>;
    callee_type callee;
    
    constexpr bool await_ready() const noexcept { return false; }
    
    detached_fn(promise_type& callee_promise);
    detached_fn(const detached_fn&) = delete;
    detached_fn(detached_fn&& other) : callee(std::exchange(other.callee, {})) {}
    ~detached_fn();

    operator callee_type() { return callee; }
    operator std::coroutine_handle<void>() { return callee; }
    void resume() { callee.resume(); }
};

struct detached_fn::promise_type {
    auto get_return_object() { return detached_fn(*this); }
    auto initial_suspend() noexcept { return std::suspend_always{}; }
    auto final_suspend() noexcept { return std::suspend_never{}; }
    
    void return_void() {}

    // TODO: exception handling
    void unhandled_exception() {}
};

inline detached_fn::detached_fn(promise_type& callee_promise):
callee(callee_type::from_promise(callee_promise)) {}

inline detached_fn::~detached_fn() {}

} // namespace coutils

#endif // __COUTILS_DETACHED_FN__
