#pragma once
#ifndef __COUTILS_WAIT__
#define __COUTILS_WAIT__

#include <atomic>
#include "coutils/agent.hpp"
#include "coutils/traits.hpp"

namespace coutils {

/**
 * @brief Evaluates `co_await` equivalent in non-coroutine context.
 * 
 * Do not use this in coroutines, it will likely cause deadlock.
 */
template <traits::awaitable T>
static inline decltype(auto) wait(T&& awaitable) {
    auto&& awaiter = ops::get_awaiter(COUTILS_FWD(awaitable));
    {
        using enum std::memory_order;
        std::atomic_flag completed;
        auto set_flag = [&]() -> agent {
            completed.test_and_set(release);
            completed.notify_all(); co_return;
        };
        bool suspended = ops::await_suspend(
            COUTILS_FWD(awaiter),
            set_flag().handle
        );
        if (suspended) { completed.wait(false, acquire); }
    }
    return awaiter.await_resume();
}

} // namespace coutils

#endif // __COUTILS_WAIT__
