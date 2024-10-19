#pragma once
#ifndef __COUTILS_TRAITS__
#define __COUTILS_TRAITS__

#include <coroutine>
#include <type_traits>
#include "coutils/macros.hpp"

namespace coutils::traits {

template <typename T>
struct is_std_co_handle : std::false_type {};
template <typename Promise>
struct is_std_co_handle<std::coroutine_handle<Promise>> : std::true_type {};
template <typename T>
inline constexpr bool is_std_co_handle_v = is_std_co_handle<T>::value;

template <typename T>
using await_resume_t =
    decltype(std::declval<T>().await_resume());
template <typename T>
using await_suspend_t =
    decltype(std::declval<T>().await_suspend(std::coroutine_handle<>{}));

template <typename T>
concept valid_suspend_result =
    std::same_as<T, void> || std::same_as<T, bool> || is_std_co_handle_v<T>;

template <typename T>
concept awaiter = 
    requires (T obj) { {obj.await_ready()} -> std::convertible_to<bool>; } &&
    requires (T obj) { obj.await_resume(); } &&
    requires (T obj, std::coroutine_handle<> handle)
        { {obj.await_suspend(handle)} -> valid_suspend_result; };

namespace _ {

template <typename T>
concept member_co_await =
    requires (T obj) { {obj.operator co_await()} -> awaiter; };
template <typename T>
concept non_member_co_await =
    requires (T obj) { {operator co_await(static_cast<T&&>(obj))} -> awaiter; };

} // namespace _

template <typename T>
concept awaiter_convertible =
    _::non_member_co_await<T> || _::member_co_await<T>;

template <typename T>
concept awaitable = awaiter_convertible<T> || awaiter<T>;

namespace _ {

template <typename T, bool = member_co_await<T>, bool = non_member_co_await<T>>
struct awaiter_cvt_impl {};

template <typename T>
struct awaiter_cvt_impl<T, false, false> { using type = T&&; };

template <typename T>
struct awaiter_cvt_impl<T, true, false>
    { using type = decltype(std::declval<T>().operator co_await()); };

template <typename T>
struct awaiter_cvt_impl<T, false, true>
    { using type = decltype(operator co_await(std::declval<T&&>())); };

} // namespace _

template <typename T>
struct awaiter_cvt : _::awaiter_cvt_impl<T> {};

template <typename T>
using awaiter_cvt_t = typename awaiter_cvt<T>::type;

template <typename T>
using co_await_t = await_resume_t<awaiter_cvt_t<T>>;

template <typename P, typename R>
concept can_return =
    (std::is_void_v<R> && requires(P p) { p.return_void(); }) ||
    requires(P p, R r) { p.return_value(r); };

template <typename P, typename Y>
concept can_yield =
    requires(P p, Y y) { {p.yield_value(y)} -> awaitable; };

} // namespace coutils::traits

namespace coutils::ops {

template <traits::awaitable T>
static inline decltype(auto) get_awaiter(T&& awaitable) {
    if constexpr (traits::_::member_co_await<T>) {
        return awaitable.operator co_await();
    } else if constexpr (traits::_::non_member_co_await<T>) {
        return operator co_await(static_cast<T&&>(awaitable));
    } else if constexpr (traits::awaiter<T>) {
        return COUTILS_FWD(awaitable);
    }
}

template <traits::awaiter T>
static inline bool await_suspend(T&& awaiter, std::coroutine_handle<> caller) {
    if (awaiter.await_ready()) { return false; }
    using Suspend = traits::await_suspend_t<T>;
    if constexpr (std::same_as<Suspend, void>) {
        awaiter.await_suspend(caller); return true;
    } else if constexpr (std::same_as<Suspend, bool>) {
        return awaiter.await_suspend(caller);
    } else if constexpr (traits::is_std_co_handle_v<Suspend>) {
        awaiter.await_suspend(caller).resume(); return true;
    }
}

template <traits::awaiter T>
static inline void await_launch(T&& awaiter, std::coroutine_handle<> caller) {
    if (!await_suspend(COUTILS_FWD(awaiter), caller)) { caller.resume(); }
}

template <traits::awaiter T>
static inline decltype(auto) await_resume(T&& awaiter) {
    if constexpr (std::is_void_v<traits::await_resume_t<T>>)
        { return std::monostate{}; }
    else { return awaiter.await_resume(); }
}

} // namespace coutils::ops

#endif // __COUTILS_TRAITS__
