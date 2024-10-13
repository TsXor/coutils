#pragma once
#ifndef __COUTILS_WAIT___
#define __COUTILS_WAIT___

#include <atomic>
#include "coutils/agent.hpp"
#include "coutils/traits.hpp"

namespace coutils {

/**
 * @brief Evaluates `co_await` equivalent in non-coroutine context.
 */
template <traits::awaitable T>
static inline decltype(auto) wait(T&& awaitable) {
    using enum std::memory_order;
    std::atomic_flag completed;
    auto&& awaiter = ops::get_awaiter(COUTILS_FWD(awaitable));
    auto flag_setter = [](std::atomic_flag& completed) -> agent {
        completed.test_and_set(release);
        completed.notify_all(); co_return;
    }(completed);
    bool suspended = ops::await_with_caller(
        COUTILS_FWD(awaiter),
        flag_setter.handle()
    );
    if (suspended) { completed.wait(false, acquire); }
    return awaiter.await_resume();
}

} // namespace coutils

#endif // __COUTILS_WAIT___
