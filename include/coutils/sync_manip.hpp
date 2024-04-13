#pragma once
#ifndef __COUTILS_SYNC_MANIP___
#define __COUTILS_SYNC_MANIP___

#include <atomic>
#include "coutils/async_fn.hpp"
#include "coutils/awaitable_traits.hpp"

namespace coutils::sync {

namespace detail {

static inline async_fn<void> notify_flag(std::atomic<bool>& atomic_flag) {
    atomic_flag = true;
    atomic_flag.notify_all();
    co_return;
}

} // namespace detail

// run an awaitable but do not suspend current coroutine (and do not care about
// its return value), useful for running awaitable in non-coroutines
// returns true if the awaitable asked for a suspension
template <typename AwaitableT, typename... ArgTs>
static inline bool unleash(ArgTs... args) {
    return await_with_caller(AwaitableT(std::forward<ArgTs>(args)...), std::noop_coroutine());
}

// manage awaitables in synchronous functions
// to use on multiple awaitables, use with all_completed
template <typename AwaitableT>
struct controlled {
    std::atomic<bool> completed = false;
    bool caller_suspend = false;
    AwaitableT wrapped;

    template <typename... ArgTs>
    controlled(ArgTs... args) : wrapped(std::forward<ArgTs>(args)...) {}
    
    void start() { caller_suspend = await_with_caller(wrapped, detail::notify_flag(completed)); }
    void join() { if (caller_suspend) { completed.wait(false); } }
    auto result() { return await_expr_value(wrapped); }
};

// function shortcut
template <typename AwaitableT, typename... ArgTs>
static inline auto make_run_join(ArgTs... args) {
    controlled<AwaitableT> synced(std::forward<ArgTs>(args)...);
    synced.start(); synced.join(); return synced.result();
}

// function shortcut
template <typename AwaitableT>
static inline auto run_join(AwaitableT&& awaitable) {
    return make_run_join<AwaitableT>(std::forward<AwaitableT>(awaitable));
}

} // namespace coutils::sync

#endif // __COUTILS_SYNC_MANIP___
