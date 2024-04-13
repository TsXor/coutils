#pragma once
#ifndef __COUTILS_DETAIL_EMPTY_SLOT__
#define __COUTILS_DETAIL_EMPTY_SLOT__

#include <type_traits>
#include <utility>
#include "coutils/awaitable_traits.hpp"

namespace coutils {

struct empty_slot_t { explicit empty_slot_t() = default; };
static inline constexpr auto empty_slot = empty_slot_t();

template <typename T> struct replace_empty { using type = T; };
template <> struct replace_empty<void> { using type = empty_slot_t; };
template <typename T>  using replace_empty_t = replace_empty<T>::type;

template <typename AwaitableT>
static inline auto await_expr_replace_empty(AwaitableT&& awaitable) {
    if constexpr (std::is_void_v<typename awaitable_traits_of<AwaitableT>::await_expr_type>) {
        return empty_slot;
    } else {
        return await_expr_value(std::forward<AwaitableT>(awaitable));
    }
}

} // namespace coutils

#endif // __COUTILS_DETAIL_EMPTY_SLOT__
