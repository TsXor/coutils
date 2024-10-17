#pragma once
#ifndef __COUTILS_ASYNC_FN__
#define __COUTILS_ASYNC_FN__

#include "coutils/zygote.hpp"

namespace coutils {

template <typename T>
struct async_fn_promise: zygote_promise<disable, disable, T> {
    std::coroutine_handle<> caller = {};
    decltype(auto) final_suspend() noexcept
        { return transfer_to_handle{std::exchange(caller, {})}; }
};

template <typename T>
using async_fn_handle = std::coroutine_handle<async_fn_promise<T>>;

namespace _ {

template <typename T>
using async_fn_base = zygote<disable, disable, T, async_fn_promise<T>>;

} // namespace _

/**
 * @brief Wraps coroutine as a lazy async function.
 * 
 * `co_return coutils::co_result(...)` can be used as an equivalent of `return {...}`.
 * 
 * Note: When copy or move contructor is available, returned object will be at
 *       least constructed once and moved once. Otherwise, the result of
 *       `co_await` expression will be a wrapper of callee's handle, where a
 *       reference of returned object can be obtained.
 */
template <typename T>
class async_fn : private _::async_fn_base<T> {
    using _Base = _::async_fn_base<T>;
    using enum promise_state;
    using _Base::handle;
    using _Base::promise;
    using _Base::move_out_returned;

public:
    using _Base::_Base;

    constexpr bool await_ready() const noexcept { return false; }
    decltype(auto) await_suspend(std::coroutine_handle<> ch)
        { promise().caller = ch; return handle(); }
    decltype(auto) await_resume() { return move_out_returned(); }
};

} // namespace coutils

template <typename T, typename... Args>
struct std::coroutine_traits<coutils::async_fn<T>, Args...> {
    using promise_type = coutils::promise_bridge<
        coutils::async_fn<T>,
        coutils::async_fn_promise<T>
    >;
};

#endif // __COUTILS_ASYNC_FN__
