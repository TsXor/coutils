#pragma once
#ifndef __COUTILS_DETAIL_GENERATOR__
#define __COUTILS_DETAIL_GENERATOR__

#include <coroutine>
#include <optional>
#include <utility>
#include <iterator>
#include "coutils/initializer_tuple.hpp"

namespace coutils {

template <typename T>
struct generator {
    struct promise_type;
    struct iterator;
    using callee_type = std::coroutine_handle<promise_type>;
    callee_type callee;
    std::optional<T> curval = std::nullopt;

    generator(promise_type& callee_promise);
    generator(const generator&) = delete;
    generator(generator&& other) : callee(std::exchange(other.callee, {})),
        curval(std::exchange(other.curval, std::nullopt)) {}
    ~generator();

    bool finished() const { return !callee || callee.done(); }
    void resume() { callee.resume(); }
    const T& current_value() const { return *curval; }

    iterator begin();
    auto end() const { return std::default_sentinel_t{}; }
};

template <typename T>
struct generator<T>::promise_type {
    std::optional<T>* curval_ptr = nullptr;

    auto get_return_object() { return generator(*this); }
    auto initial_suspend() noexcept { return std::suspend_always{}; }
    auto final_suspend() noexcept { return std::suspend_always{}; }
    template<typename AwaitableT>
    std::suspend_never await_transform(AwaitableT&&) = delete;
    void return_void() {}

    template <typename YieldExprT>
    auto yield_value(YieldExprT expr) noexcept {
        if (curval_ptr) {
            if constexpr (is_initializer_tuple_ref_v<YieldExprT>) {
                emplace_apply(*curval_ptr, std::forward<YieldExprT>(expr));
            } else {
                curval_ptr->emplace(std::forward<YieldExprT>(expr));
            }
        }
        return std::suspend_always{};
    }

    // TODO: exception handling
    void unhandled_exception() {}
};

template <typename T>
inline generator<T>::generator(promise_type& callee_promise):
callee(callee_type::from_promise(callee_promise)) {
    callee_promise.curval_ptr = &curval;
}

template <typename T>
inline generator<T>::~generator() {
    if (callee && !callee.done()) { callee.promise().curval_ptr = nullptr; }
}

template <typename T>
struct generator<T>::iterator {
    generator<T>& gen;
    bool operator==(std::default_sentinel_t) const { return gen.finished(); }
    void operator++() { gen.resume(); }
    const T& operator*() const { return gen.current_value(); }
    const T* operator->() const { return &gen.current_value(); }
};

template <typename T>
inline generator<T>::iterator generator<T>::begin() {
    resume(); // generates the first value
    return { *this };
}

} // namespace coutils

#endif // __COUTILS_DETAIL_GENERATOR__
