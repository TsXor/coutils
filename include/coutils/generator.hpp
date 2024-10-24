#pragma once
#ifndef __COUTILS_GENERATOR__
#define __COUTILS_GENERATOR__

#include "coutils/zygote.hpp"

namespace coutils {

template <typename Y, typename S>
struct generator_promise : zygote_promise<Y, S, void> {
    void await_transform(auto&&) = delete;
};

template <typename Y, typename S>
using generator_handle = std::coroutine_handle<generator_promise<Y, S>>;

namespace _ {

template <typename Y, typename S>
using generator_base = zygote<Y, S, void, generator_promise<Y, S>>;

} // namespace _

template <typename Y, typename S = void>
class generator : private _::generator_base<Y, S> {
    using _Base = _::generator_base<Y, S>;
    using _Base::transfer;

public:
    using _Base::_Base;

    class iterator : private _Base {
        using enum promise_state;
        using typename _Base::handle_type;
        using _Base::transfer;
        using _Base::status;
        using _Base::resume;
        using _Base::check_error;
        using _Base::yielded;

    public:
        iterator(handle_type handle) : _Base(handle) { ++(*this); }
        iterator(const iterator&) = delete;
        iterator(iterator&&) = default;

        using _Base::send;
        bool operator==(std::default_sentinel_t) { return status() == RETURNED; }
        decltype(auto) operator*() { return yielded(); }
        decltype(auto) operator->() { return std::addressof(*(*this)); }
        iterator& operator++() & { resume(); check_error(); return *this; }
        iterator operator++() && { resume(); check_error(); return {transfer()}; }
    };

    decltype(auto) begin() { return iterator(transfer()); }
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
