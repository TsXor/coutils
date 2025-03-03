#pragma once
#ifndef __COUTILS_ASYNC_GENERATOR__
#define __COUTILS_ASYNC_GENERATOR__

#include <iterator>
#include "coutils/zygote.hpp"

namespace coutils {

template <typename Y, typename S>
struct async_generator_promise: zygote_promise<async_generator_promise<Y, S>, Y, S, void> {
    std::coroutine_handle<> caller = {};
    decltype(auto) final_suspend() noexcept
        { return transfer_to_handle{std::exchange(caller, {})}; }
    decltype(auto) yield_suspend(std::coroutine_handle<>)
        { return std::exchange(caller, {}); }
};

template <typename Y, typename S>
using async_generator_handle = std::coroutine_handle<async_generator_promise<Y, S>>;

template <typename Y, typename S = void>
class async_generator {
    using _Ops = zygote_ops<async_generator_promise<Y, S>>;
    owning_handle<async_generator_promise<Y, S>> handle;

public:
    async_generator(async_generator_promise<Y, S>& p) : handle(p) {}

    class iterator {
        using enum promise_state;
        using _Ops = zygote_ops<async_generator_promise<Y, S>>;
        owning_handle<async_generator_promise<Y, S>> handle;

    public:
        iterator(decltype(handle)&& h) noexcept : handle(std::move(h)) {}

        bool operator==(std::default_sentinel_t) noexcept
            { return _Ops::status(handle) == RETURNED; }
        decltype(auto) operator*()
            { _Ops::check_error(handle); return _Ops::yielded(handle); }
        decltype(auto) operator->() { return std::addressof(*(*this)); }
        iterator& operator++() & noexcept { return *this; }

        constexpr bool await_ready() const noexcept { return false; }
        std::coroutine_handle<> await_suspend(std::coroutine_handle<> ch)
            { handle.promise().caller = ch; return handle; }
        void await_resume() {}
    };

    decltype(auto) begin() { return iterator(std::move(handle)); }
    decltype(auto) end() const { return std::default_sentinel; }
};

} // namespace coutils

template <typename Y, typename S, typename... Args>
struct std::coroutine_traits<coutils::async_generator<Y, S>, Args...> {
    using promise_type = coutils::promise_bridge<
        coutils::async_generator<Y, S>,
        coutils::async_generator_promise<Y, S>
    >;
};

#endif // __COUTILS_ASYNC_GENERATOR__
