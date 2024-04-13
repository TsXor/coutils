#pragma once
#ifndef __COUTILS_DETAIL_AWAITABLE_TRAITS__
#define __COUTILS_DETAIL_AWAITABLE_TRAITS__

#include <coroutine>
#include <type_traits>

namespace coutils {

template <typename T>
struct awaitable_traits {
    using await_expr_type = std::invoke_result<decltype(T::await_resume), T*>::type;
    using suspend_return_type = std::invoke_result<decltype(T::await_suspend), T*, std::coroutine_handle<>>::type;
    
    static constexpr bool is_void_suspend = std::is_same_v<suspend_return_type, void>;
    static constexpr bool is_bool_suspend = std::is_same_v<suspend_return_type, bool>;
    static constexpr bool is_handle_suspend = std::is_convertible_v<suspend_return_type, std::coroutine_handle<>>;
};

template <typename T>
using awaitable_traits_of = awaitable_traits<std::remove_cvref_t<T>>;

template <typename AwaitableT>
    requires awaitable_traits_of<AwaitableT>::is_void_suspend
static inline bool await_with_caller(AwaitableT&& awaitable, std::coroutine_handle<> caller) {
    bool ready = awaitable.await_ready();
    if (ready) { return false; }
    awaitable.await_suspend(caller);
    return true;
}
template <typename AwaitableT>
    requires awaitable_traits_of<AwaitableT>::is_bool_suspend
static inline bool await_with_caller(AwaitableT&& awaitable, std::coroutine_handle<> caller) {
    bool ready = awaitable.await_ready();
    if (ready) { return false; }
    bool suspended = awaitable.await_suspend(caller);
    return suspended;
}
template <typename AwaitableT>
    requires awaitable_traits_of<AwaitableT>::is_handle_suspend
static inline bool await_with_caller(AwaitableT&& awaitable, std::coroutine_handle<> caller) {
    bool ready = awaitable.await_ready();
    if (ready) { return false; }
    std::coroutine_handle<> hd = awaitable.await_suspend(caller);
    hd.resume(); return true;
}

template <typename AwaitableT>
static inline auto await_expr_value(AwaitableT&& awaitable) {
    return awaitable.await_resume();
}

} // namespace coutils

#endif // __COUTILS_DETAIL_AWAITABLE_TRAITS__
