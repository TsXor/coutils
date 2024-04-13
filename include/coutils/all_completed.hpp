#pragma once
#ifndef __COUTILS_ALL_COMPLETED__
#define __COUTILS_ALL_COMPLETED__

#include <mutex>
#include "coutils/async_fn.hpp"
#include "coutils/awaitable_traits.hpp"
#include "coutils/empty_slot.hpp"

namespace coutils {

namespace detail {

template <typename AwaitableT>
using merge_type = replace_empty_t<typename awaitable_traits<AwaitableT>::await_expr_type>;

template <typename... AwaitableTs>
using merge_fn = async_fn<std::tuple<merge_type<AwaitableTs>...>>;

static inline async_fn<void> latch_fn(size_t n) {
    size_t count = 0; std::mutex lock;
    while (true) {
        co_await std::suspend_always{};
        const std::lock_guard guard(lock);
        if (++count >= n) break;
    }
}

} // namespace detail

// suspend current coroutine and resume after all given awaitables are completed
// the awaitables will be started SERIALLY on the SAME THREAD as caller
// 
// the result of co_await-ing this is the results of all given awaitables packed
// in a tuple (in given order), where void results are replaced with the marker
// object coutils::empty_slot
//
// NOTE: input awaitables should be rvalue references, which means their lifetime
// will not be extended by this coroutine and they should not be used for other purposes
template <typename... AwaitableTs> requires (... && std::is_rvalue_reference_v<AwaitableTs&&>)
static inline auto all_completed(AwaitableTs&&... awaitables) -> detail::merge_fn<AwaitableTs...> {
    auto latch_handle = detail::latch_fn(sizeof...(awaitables) + 1);
    latch_handle.manual_resume(); // async_fn is lazy, start it
    auto reach_latch = [&](bool suspended) { if (!suspended) { latch_handle.manual_resume(); } };
    (..., reach_latch(await_with_caller(awaitables, latch_handle.callee)));
    co_await latch_handle;
    co_return std::tuple(await_expr_replace_empty(awaitables)...);
}

} // namespace coutils

#endif // __COUTILS_ALL_COMPLETED__
