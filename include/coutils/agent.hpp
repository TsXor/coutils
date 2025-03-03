#pragma once
#ifndef __COUTILS_AGENT__
#define __COUTILS_AGENT__

#include "coutils/utility.hpp"
#include "coutils/traits.hpp"

namespace coutils {

struct agent_promise {
    void return_void() noexcept {}
    [[noreturn]] void unhandled_exception() noexcept { std::terminate(); }

    decltype(auto) initial_suspend() noexcept
        { return std::suspend_always{}; }
    decltype(auto) final_suspend() noexcept
        { return std::suspend_never{}; }
};

using agent_handle = std::coroutine_handle<agent_promise>;

/**
 * @brief A reduced type of coroutine.
 * 
 * This can be used to generate coroutines for explicitly manipulating other
 * coroutines. For its usage see `wait`.
 * 
 * Coroutines of this type does not handle exceptions, so they are expected to
 * be `noexcept`. 
 * 
 * Coroutines of this type does not suspend at final suspend point, so they
 * will be automatically destroyed after they finish.
 */
struct agent {
    std::coroutine_handle<agent_promise> handle;
    agent(agent_promise& p) : handle(decltype(handle)::from_promise(p)) {}
};

} // namespace coutils

template <typename... Args>
struct std::coroutine_traits<coutils::agent, Args...> {
    using promise_type = coutils::promise_bridge<
        coutils::agent,
        coutils::agent_promise
    >;
};

#endif // __COUTILS_AGENT__
