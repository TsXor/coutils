#pragma once
#ifndef __COUTILS_CRT_ASYNC_FN__
#define __COUTILS_CRT_ASYNC_FN__

#include "./zygote.hpp"

namespace coutils::crt {

template <typename T>
struct async_fn_promise: zygote_promise<async_fn_promise<T>, zygote_disable, zygote_disable, T> {
    std::coroutine_handle<> caller = {};
    decltype(auto) final_suspend() noexcept
        { return transfer_to_handle{std::exchange(caller, {})}; }
};

template <typename T>
using async_fn_handle = std::coroutine_handle<async_fn_promise<T>>;

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
class async_fn {
    using _Ops = zygote_ops<async_fn_promise<T>>;
    owning_handle<async_fn_promise<T>> handle;

public:
    async_fn(async_fn_promise<T>& p) : handle(p) {}

    constexpr bool await_ready() const noexcept { return false; }
    std::coroutine_handle<> await_suspend(std::coroutine_handle<> ch)
        { handle.promise().caller = ch; return handle; }
    decltype(auto) await_resume() { return _Ops::move_out_returned(handle); }
};

} // namespace coutils::crt

template <typename T, typename... Args>
struct std::coroutine_traits<coutils::crt::async_fn<T>, Args...> {
    using promise_type = coutils::promise_bridge<
        coutils::crt::async_fn<T>,
        coutils::crt::async_fn_promise<T>
    >;
};

#endif // __COUTILS_CRT_ASYNC_FN__
