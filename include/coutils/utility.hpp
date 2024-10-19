#pragma once
#ifndef __COUTILS_UTILITY__
#define __COUTILS_UTILITY__

#include <utility>
#include <type_traits>
#include <atomic>
#include <memory>
#include <variant>
#include <stdexcept>
#include <coroutine>
#include "coutils/macros.hpp"
#include "coutils/traits.hpp"

namespace coutils {

/**
 * @brief Reference wrapper.
 * 
 * Different from `std::reference_wrapper`, rvalue references can be wrapped.
 */
template <typename R> requires std::is_reference_v<R>
class ref {
    std::remove_reference_t<R>* ptr;
public:
    using type = R;
    template <typename T> requires std::is_convertible_v<T&&, R>
    ref(T&& t) : ptr(std::addressof(t)) {}
    ref(const ref&) = default;
    ref& operator=(const ref&) = default;
    R get() { return static_cast<R>(*ptr); }
    operator R() { return get(); }
};

/**
 * @brief Optional reference wrapper.
 */
template <typename R> requires std::is_reference_v<R>
class optref {
    std::remove_reference_t<R>* ptr;
public:
    using type = R;
    optref() : ptr(nullptr) {}
    template <typename T> requires std::is_convertible_v<T&&, R>
    optref(T&& t) : ptr(std::addressof(t)) {}
    optref(const optref&) = default;
    optref& operator=(const optref&) = default;
    bool has_value() { return ptr != nullptr; }
    R get() { return static_cast<R>(*ptr); }
    operator R() { return get(); }
};


/**
 * @brief Wraps arbitrary class.
 */
template <typename T>
class leaf {
    [[no_unique_address]] T value;

public:
    explicit leaf()
        requires std::default_initializable<T> {}

    explicit leaf(auto&&... args)
        requires std::constructible_from<T, decltype(args)...> :
        value(COUTILS_FWD(args)...) {}

    leaf(const leaf<T>& other)
        requires std::copy_constructible<T> :
        value(other.value) {}
    leaf(leaf<T>&& other)
        requires std::move_constructible<T> :
        value(std::move(other.value)) {}

    T& get() & { return value; }
    const T& get() const & { return value; }
    T&& get() && { return std::move(value); }
};


template <typename T>
concept non_value = std::is_void_v<T> || std::is_reference_v<T>;

namespace _ {

template <typename T, bool = std::is_void_v<T>, bool = std::is_reference_v<T>>
struct non_value_wrapper_impl {};

template <typename T>
struct non_value_wrapper_impl<T, false, false> : leaf<T> {
    using type = T;
    using leaf<T>::leaf;
};

template <typename T>
struct non_value_wrapper_impl<T, true, false> {
    using type = T;
    explicit non_value_wrapper_impl() = default;
    explicit non_value_wrapper_impl(std::monostate) {}
    std::monostate get() { return {}; }
};

template <typename T>
struct non_value_wrapper_impl<T, false, true> : ref<T> {
    using type = T;
    using ref<T>::ref;
};

} // namespace _

/**
 * @brief Holds reference and void type in value type.
 * 
 * This is useful when storing arbitrary types into `std::optional` or
 * `std::variant`.
 * 
 * Contained value can be unwrapped using `.get()`. For convenience, result of
 * unwrapping `non_value_wrapper<void>` is `std::monostate`. (This is because
 * there are many restrictions on using void expression and most times it
 * requires to repeatedly check whether resulting type is void.)
 * 
 * Note that C-style arrays are not replaced, while `std::optional` or
 * `std::variant` cannot store them.
 */
template <typename T>
struct non_value_wrapper : _::non_value_wrapper_impl<T> {
    using _::non_value_wrapper_impl<T>::non_value_wrapper_impl;
};

template <typename T>
constexpr bool is_monostate_v =
    std::is_same_v<std::remove_cvref_t<T>, std::monostate>;


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
 * @brief Makes lvalue/rvalue overload of awaitables work.
 * 
 * According to cppreference, in a `co_await` expression, when obtaining
 * awaiter, "If the expression above is a prvalue, the awaiter object is a
 * temporary materialized from it. Otherwise, if the expression above is a
 * glvalue, the awaiter object is the object to which it refers."
 * 
 * This means the awaiter is always in lvalue state when its methods are
 * called, then rvalue overloads will never be used. To overcome this, here is
 * `ref_awaiter`. It remembers reference type in template parameter and casts
 * container reference to call correct overload. 
 */
template <traits::awaiter T> requires std::is_reference_v<T>
class ref_awaiter {
    ref<T> _ref;
public:
    explicit ref_awaiter(T ref) : _ref(COUTILS_FWD(ref)) {}
    constexpr bool await_ready()
        { return _ref.get().await_ready(); }
    constexpr decltype(auto) await_suspend(std::coroutine_handle<> ch)
        { return _ref.get().await_suspend(ch); }
    constexpr decltype(auto) await_resume()
        { return _ref.get().await_resume(); }
};

template <traits::awaiter T> requires std::is_reference_v<T&&>
explicit ref_awaiter(T&&) -> ref_awaiter<T&&>;

/**
 * @brief Adds member `co_await` overload that converts `*this` to ref_awaiter.
 * 
 * Paste this macro to public area of your awaitable class, then lvalue/rvalue
 * method overload of awaitables work.
 */
#define COUTILS_REF_AWAITER_CONV_OVERLOAD \
    decltype(auto) operator co_await() & { return ref_awaiter(*this); } \
    decltype(auto) operator co_await() && { return ref_awaiter(std::move(*this)); }


template <typename T>
concept reconstructible =
    std::copy_constructible<T> || std::move_constructible<T>;


/**
 * @brief An empty class tag.
 * 
 * Currently, when used in `zygote_promise`, it represents `co_yield` (when used
 * as `Y`) or `co_return` (when used as `R`) is disabled.
 */
struct disable {};


/**
 * @brief Wraps a reference as non-suspending awaitable.
 */
template <typename T> requires std::is_reference_v<T>
struct immediately {
    ref<T> _ref;
public:
    explicit immediately(T ref) : _ref(COUTILS_FWD(ref)) {}
    constexpr bool await_ready() const noexcept { return true; }
    constexpr void await_suspend(std::coroutine_handle<> ch) const noexcept {}
    constexpr decltype(auto) await_resume() { return _ref.get(); }
};

template <typename T> requires std::is_reference_v<T&&>
explicit immediately(T&&) -> immediately<T&&>;

decltype(auto) wrap_as_awaitable(auto&& obj) {
    if constexpr (traits::awaitable<decltype(obj)>)
        { return COUTILS_FWD(obj); }
    else { return immediately(COUTILS_FWD(obj)); }
}

/**
 * @brief If `expr` is awaitable returns `co_await expr`, else returns `expr`.
 */
#define COUTILS_AWAIT(expr) (co_await coutils::wrap_as_awaitable(expr))


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
 * @brief A resource managing wrapper of `std::coroutine_handle<P>`.
 */
template <typename P = void>
class handle_manager {
    using _Handle = std::coroutine_handle<P>;
    _Handle _handle;

public:
    using handle_type = _Handle;

    explicit handle_manager() : _handle({}) {}
    explicit handle_manager(_Handle handle): _handle(handle) {}
    explicit handle_manager(P& promise):
        _handle(_Handle::from_promise(promise)) {}
    ~handle_manager() { destroy(); }

    handle_manager(const handle_manager<P>&) = delete;
    handle_manager<P>& operator=(const handle_manager<P>&) = delete;
    handle_manager(handle_manager<P>&& other) noexcept :
        _handle(std::exchange(other._handle, {})) {}
    handle_manager<P>& operator=(handle_manager<P>&& other) noexcept
        { destroy(); _handle = std::exchange(other._handle, {}); }

    P& promise() { return _handle.promise(); }
    bool empty() { return !_handle; }
    _Handle handle() noexcept { return _handle; }
    _Handle transfer() noexcept { return std::exchange(_handle, {}); }
    void destroy() { if (_handle) { transfer().destroy(); } }
    bool done() { return _handle.done(); }
    void resume() { _handle.resume(); }
};

/**
 * @brief A simple promise type that returns `void` and ignores exception.
 */
struct simple_promise {
    void return_void() noexcept {}
    void unhandled_exception() noexcept {}

    decltype(auto) initial_suspend() noexcept
        { return std::suspend_always{}; }
    decltype(auto) final_suspend() noexcept
        { return std::suspend_always{}; }
};


template <std::size_t I>
using index_constant = std::integral_constant<std::size_t, I>;

namespace _ {

template <std::size_t I, typename VisRef>
static void visitor_invoker(VisRef vis) { vis(index_constant<I>{}); }

template <std::size_t, typename, typename>
struct jump_table_impl;

template <std::size_t Max, std::size_t... Is, typename VisRef>
struct jump_table_impl<Max, std::index_sequence<Is...>, VisRef> {
    using func = void(VisRef);
    static constexpr std::array<func*, Max> table{visitor_invoker<Is, VisRef>...};
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
void visit_index(std::size_t idx, Visitor&& vis) {
    _::jump_table<max, Visitor&&>::table[idx](COUTILS_FWD(vis));
}

#define COUTILS_VISITOR(var) \
    [&] <std::size_t var> (coutils::index_constant<var>) -> void

/**
 * @brief Shortcut for using `visit_index` on `std::variant`.
 */
template <typename Variant, typename Visitor>
void visit_variant(Variant&& var, Visitor&& vis) {
    constexpr auto vsize = std::variant_size_v<std::remove_cvref_t<Variant>>;
    visit_index<vsize>(var.index(), COUTILS_FWD(vis));
}

} // namespace coutils

#endif // __COUTILS_UTILITY__
