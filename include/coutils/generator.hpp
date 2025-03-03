#pragma once
#ifndef __COUTILS_GENERATOR__
#define __COUTILS_GENERATOR__

#include "coutils/zygote.hpp"

namespace coutils {

template <typename Y, typename S>
struct generator_promise : zygote_promise<generator_promise<Y, S>, Y, S, void> {
    void await_transform(auto&&) = delete;
};

template <typename Y, typename S>
using generator_handle = std::coroutine_handle<generator_promise<Y, S>>;

template <typename Y, typename S = void>
class generator {
    using _Ops = zygote_ops<generator_promise<Y, S>>;
    owning_handle<generator_promise<Y, S>> handle;

public:
    generator(generator_promise<Y, S>& p) : handle(p) {}

    class iterator {
        using enum promise_state;
        using _Ops = zygote_ops<generator_promise<Y, S>>;
        owning_handle<generator_promise<Y, S>> handle;

    public:
        iterator(decltype(handle)&& h) noexcept : handle(std::move(h)) {}

        bool operator==(std::default_sentinel_t) noexcept
            { return _Ops::status(handle) == RETURNED; }
        decltype(auto) operator*()
            { _Ops::check_error(handle); return _Ops::yielded(handle); }
        decltype(auto) operator->() { return std::addressof(*(*this)); }
        iterator& operator++() & { handle.resume(); return *this; }
    };

    decltype(auto) begin() { handle.resume(); return iterator(std::move(handle)); }
    decltype(auto) end() const { return std::default_sentinel; }
};

} // namespace coutils

template <typename Y, typename S, typename... Args>
struct std::coroutine_traits<coutils::generator<Y, S>, Args...> {
    using promise_type = coutils::promise_bridge<
        coutils::generator<Y, S>,
        coutils::generator_promise<Y, S>
    >;
};

#endif // __COUTILS_GENERATOR__
