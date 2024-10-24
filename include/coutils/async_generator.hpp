#pragma once
#ifndef __COUTILS_ASYNC_GENERATOR__
#define __COUTILS_ASYNC_GENERATOR__

#include "coutils/zygote.hpp"

namespace coutils {

template <typename Y, typename S>
struct async_generator_promise:
    zygote_promise<Y, S, void>,
    mixins::promise_yield<async_generator_promise<Y, S>, Y>
{
    std::coroutine_handle<> caller = {};

    decltype(auto) final_suspend() noexcept
        { return transfer_to_handle{std::exchange(caller, {})}; }

    decltype(auto) yield_suspend(std::coroutine_handle<>)
        { return std::exchange(caller, {}); }

    using mixins::promise_yield<async_generator_promise<Y, S>, Y>::yield_value;
};

template <typename Y, typename S>
using async_generator_handle = std::coroutine_handle<async_generator_promise<Y, S>>;

namespace _ {

template <typename Y, typename S>
using async_generator_base = zygote<Y, S, void, async_generator_promise<Y, S>>;

} // namespace _

template <typename Y, typename S = void>
class async_generator : private _::async_generator_base<Y, S> {
    using _Base = _::async_generator_base<Y, S>;
    using _Base::transfer;

public:
    using _Base::_Base;

    class iterator : private _Base {
        using enum promise_state;
        using typename _Base::handle_type;

        using _Base::promise;
        using _Base::handle;
        using _Base::transfer;
        using _Base::status;
        using _Base::check_error;
        using _Base::yielded;

    public:
        iterator(handle_type handle) : _Base(handle) {}
        iterator(const iterator&) = delete;
        iterator(iterator&&) = default;

        using _Base::send;
        bool operator==(std::default_sentinel_t) { return status() == RETURNED; }
        decltype(auto) operator*() { return yielded(); }
        decltype(auto) operator->() { return std::addressof(*(*this)); }
        iterator& operator++() & { return *this; }
        iterator operator++() && { return {transfer()}; }

        constexpr bool await_ready() const noexcept { return false; }
        decltype(auto) await_suspend(std::coroutine_handle<> ch)
            { promise().caller = ch; return handle(); }
        iterator& await_resume() & { check_error(); return *this; }
        iterator await_resume() && { check_error(); return {transfer()}; }

        COUTILS_REF_AWAITER_CONV_OVERLOAD
    };

    decltype(auto) begin() { return iterator(transfer()); }
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
