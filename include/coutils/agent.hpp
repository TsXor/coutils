#pragma once
#ifndef __COUTILS_AGENT__
#define __COUTILS_AGENT__

#include "coutils/utility.hpp"
#include "coutils/traits.hpp"

namespace coutils {

struct agent_promise : simple_promise {};

using agent_handle = std::coroutine_handle<agent_promise>;

namespace _ {

using agent_base = handle_manager<agent_promise>;

} // namespace _

/**
 * @brief A reduced type of coroutine.
 * 
 * This can be used to generate coroutines for explicitly manipulating other
 * coroutines. For its usage see `wait` and `all_completed`.
 */
struct agent : _::agent_base {
    using _::agent_base::agent_base;
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
