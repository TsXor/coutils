#pragma once
#ifndef __COUTILS_TASK__
#define __COUTILS_TASK__

#include "coutils/common_promise.hpp"

namespace coutils {

template <typename T>
struct task_promise: common_promise<disable, disable, T> {};

template <typename T>
using task_handle = std::coroutine_handle<task_promise<T>>;

namespace _ {

template <typename T>
using task_base = common_manager<disable, disable, T, task_promise<T>>;

} // namespace _

/**
 * @brief A manipulatable coroutine wrapper.
 */
template <typename T>
struct task : _::task_base<T> {
    using _::task_base<T>::task_base;
};

} // namespace coutils

template <typename T, typename... Args>
struct std::coroutine_traits<coutils::task<T>, Args...> {
    using promise_type = coutils::promise_bridge<
        coutils::task<T>,
        coutils::task_promise<T>
    >;
};

#endif // __COUTILS_TASK__
