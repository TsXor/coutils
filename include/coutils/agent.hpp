#pragma once
#ifndef __COUTILS_AGENT__
#define __COUTILS_AGENT__

#include "coutils/utility.hpp"
#include "coutils/traits.hpp"

namespace coutils {

struct agent_promise : simple_promise {
    decltype(auto) final_suspend() noexcept
        { return std::suspend_never{}; }
};

using agent_handle = std::coroutine_handle<agent_promise>;

namespace _ {

using agent_base = handle_manager<agent_promise>;

} // namespace _

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
struct agent : _::agent_base {
    using _::agent_base::agent_base;
    ~agent() { transfer(); }
    agent& operator=(const agent&) = delete;
    agent& operator=(agent&&) = default;
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
