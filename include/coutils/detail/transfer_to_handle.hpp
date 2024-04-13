#pragma once
#ifndef __COUTILS_DETAIL_TRANSFER_TO_HANDLE__
#define __COUTILS_DETAIL_TRANSFER_TO_HANDLE__

#include <coroutine>

namespace coutils::detail {

// transfer control to other handle
struct transfer_to_handle {
    std::coroutine_handle<> other;
    constexpr bool await_ready() const noexcept { return false; }
    constexpr void await_resume() const noexcept {}
    auto await_suspend(std::coroutine_handle<> ch) const noexcept { return other; }
};

} // namespace coutils::detail

#endif // __COUTILS_DETAIL_TRANSFER_TO_HANDLE__
