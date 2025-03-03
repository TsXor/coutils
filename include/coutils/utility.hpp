#pragma once
#ifndef __COUTILS_UTILITY__
#define __COUTILS_UTILITY__

#include <cstddef>
#include <utility>
#include <type_traits>
#include <atomic>
#include <memory>
#include <coroutine>
#include "coutils/macros.hpp"
#include "coutils/traits.hpp"

namespace coutils {

/**
 * @brief Lightweight lock implemented with `std::atomic_flag`.
 */
class light_lock {
    using enum std::memory_order;

    std::atomic_flag _flag = ATOMIC_FLAG_INIT;

public:
    explicit light_lock() {};

    void lock() noexcept {
        while (_flag.test_and_set(acquire)) {
            _flag.wait(true, relaxed);
        }
    }

    void unlock() noexcept {
        _flag.clear(release);
        _flag.notify_one();
    }

    bool try_lock() noexcept {
        return !_flag.test_and_set(acquire);
    }
};

class empty_lock {
public:
    explicit empty_lock() = default;
    empty_lock(const empty_lock&) = default;
    constexpr void lock() noexcept {}
    constexpr void unlock() noexcept {}
    constexpr bool try_lock() noexcept { return true; }
    auto ref() noexcept { return empty_lock(); }
};


/**
 * @brief An awaitable that transfers control to another handle.
 * 
 * When handle is null, immediately resumes caller.
 */
struct transfer_to_handle {
    std::coroutine_handle<> other;
    constexpr bool await_ready() const noexcept
        { return other.address() == nullptr; }
    constexpr decltype(auto) await_suspend(std::coroutine_handle<>)
        const noexcept { return other; }
    constexpr void await_resume() const noexcept {}
};


/**
 * @brief A template that connects coroutine return type and promise type.
 * 
 * With this we don't need to split functions out of the class.
 */
template <typename Returned, typename Promise>
    requires (std::is_constructible_v<Returned, Promise&>)
struct promise_bridge : public Promise {
    constexpr decltype(auto) get_return_object()
        noexcept(std::is_nothrow_constructible_v<Returned, Promise&>)
        { return Returned(static_cast<Promise&>(*this)); }
};


/**
 * @brief A class tag type that packs a tuple.
 */
template <typename... InitTs>
struct initializer_mark {
    std::tuple<InitTs...> data;
    constexpr initializer_mark(auto&&... args):
        data(COUTILS_FWD(args)...) {}
};

/**
 * @brief Checks if a type is instantiation of `initializer_mark`.
 */
template <typename T>
struct is_initializer_mark : public std::false_type {};
template <typename... InitTs>
struct is_initializer_mark<initializer_mark<InitTs...>> : public std::true_type {};
template <typename T>
inline constexpr bool is_initializer_mark_v = is_initializer_mark<T>::value;


/**
 * @brief Makes an `initializer_mark` with references to arguments.
 * 
 * `Promise::return_value` is called before destroying local variables, so it
 * is okay to forward only references.
 */
constexpr decltype(auto) co_result(auto&&... args) {
    using Result = initializer_mark<decltype(args)...>;
    return Result(COUTILS_FWD(args)...);
}


template <typename P>
auto handle_cast(std::coroutine_handle<> h) -> std::coroutine_handle<P>
    { return std::coroutine_handle<P>::from_address(h.address()); }

/**
 * @brief An owning version of `std::coroutine_handle<P>`.
 */
template <typename P = void>
class owning_handle {
    using _Handle = std::coroutine_handle<P>;
    using _ErasedHandle = std::coroutine_handle<>;
    _Handle _handle;

public:
    using handle_type = _Handle;

    owning_handle() noexcept : _handle() {}
    owning_handle(std::nullptr_t) noexcept : _handle(nullptr) {}
    owning_handle(_Handle handle) noexcept : _handle(handle) {}
    owning_handle(P& promise):
        _handle(_Handle::from_promise(promise)) {}
    ~owning_handle() { destroy(); }
    owning_handle& operator=(std::nullptr_t) noexcept
        { _handle = nullptr; return *this; }

    owning_handle(const owning_handle&) = delete;
    owning_handle& operator=(const owning_handle&) = delete;
    owning_handle(owning_handle&& other) noexcept :
        _handle(std::exchange(other._handle, {})) {}
    owning_handle& operator=(owning_handle&& other) noexcept
        { destroy(); _handle = std::exchange(other._handle, {}); }

    operator _Handle() const noexcept { return _handle; }
    operator _ErasedHandle() const noexcept { return _handle; }
    P& promise() const { return _handle.promise(); }
    explicit operator bool() const noexcept { return static_cast<bool>(_handle); }
    _Handle handle() const noexcept { return _handle; }
    _Handle transfer() noexcept { return std::exchange(_handle, {}); }
    void destroy() { if (_handle) { transfer().destroy(); } }
    bool done() const noexcept { return _handle.done(); }
    void resume() const { _handle.resume(); }
};


template <std::size_t I>
using index_constant = std::integral_constant<std::size_t, I>;

namespace _ {

template <std::size_t, typename, typename>
struct jump_table_impl;

template <std::size_t Max, std::size_t... Is, typename VisRef>
struct jump_table_impl<Max, std::index_sequence<Is...>, VisRef> {
    template <std::size_t I>
    static decltype(auto) visitor_invoker(VisRef vis)
        { return vis(index_constant<I>{}); }

    static constexpr auto table = std::array{visitor_invoker<Is>...};
};

template <std::size_t Max, typename VisRef>
struct jump_table : jump_table_impl<Max, std::make_index_sequence<Max>, VisRef> {};

} // namespace _

/**
 * @brief "Constantify" a dynamic index in given range.
 * 
 * This is similar to a switch-case.
 */
template <std::size_t max, typename Visitor>
decltype(auto) visit_index(std::size_t idx, Visitor&& vis) {
    return _::jump_table<max, Visitor&&>::table[idx](COUTILS_FWD(vis));
}

#define COUTILS_VISITOR(var) \
    [&] <std::size_t var> (coutils::index_constant<var>)

/**
 * @brief Shortcut for using `visit_index` on `std::variant`.
 */
template <typename Variant, typename Visitor>
decltype(auto) visit_variant(Variant&& var, Visitor&& vis) {
    constexpr auto vsize = std::variant_size_v<std::remove_cvref_t<Variant>>;
    return visit_index<vsize>(var.index(), COUTILS_FWD(vis));
}

} // namespace coutils

#endif // __COUTILS_UTILITY__
