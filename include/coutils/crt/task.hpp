#pragma once
#ifndef __COUTILS_CRT_TASK__
#define __COUTILS_CRT_TASK__

#include "./zygote.hpp"

namespace coutils::crt {

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

} // namespace coutils::crt

template <typename T, typename... Args>
struct std::coroutine_traits<coutils::crt::task<T>, Args...> {
    using promise_type = coutils::promise_bridge<
        coutils::crt::task<T>,
        coutils::crt::task_promise<T>
    >;
};

#endif // __COUTILS_CRT_TASK__
