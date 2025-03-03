#pragma once
#ifndef __COUTILS_TASK__
#define __COUTILS_TASK__

#include "coutils/zygote.hpp"

namespace coutils {

template <typename T>
struct task_promise: zygote_promise<task_promise<T>, zygote_disable, zygote_disable, T> {};

template <typename T>
using task_handle = std::coroutine_handle<task_promise<T>>;

/**
 * @brief A manipulatable coroutine wrapper.
 */
template <typename T>
struct task {
    owning_handle<task_promise<T>> handle;
    task(task_promise<T>& p) : handle(p) {}
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
