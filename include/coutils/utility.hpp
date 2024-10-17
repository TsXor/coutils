#pragma once
#ifndef __COUTILS_UTILITY__
#define __COUTILS_UTILITY__

#include <type_traits>
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


template <typename T>
concept non_value = std::is_void_v<T> || std::is_reference_v<T>;

namespace _ {

template <typename T, bool = std::is_void_v<T>, bool = std::is_reference_v<T>>
struct non_value_wrapper_impl {};

template <typename T>
class non_value_wrapper_impl<T, false, false> {
    using Self = non_value_wrapper_impl<T, false, false>;

    T value;

public:
    using type = T;

    explicit non_value_wrapper_impl()
        requires std::default_initializable<T> {}

    explicit non_value_wrapper_impl(auto&&... args)
        requires std::constructible_from<T, decltype(args)...> :
        value(COUTILS_FWD(args)...) {}

    non_value_wrapper_impl(const Self& other)
        requires std::copy_constructible<T> :
        value(other.value) {}
    non_value_wrapper_impl(Self&& other)
        requires std::move_constructible<T> :
        value(std::move(other.value)) {}

    T& get() & { return value; }
    T& get() const & { return value; }
    T&& get() && { return std::move(value); }
};

template <typename T>
struct non_value_wrapper_impl<T, true, false> {
    using type = T;
    explicit non_value_wrapper_impl() = default;
    void get() { return; }
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
 * Note that C-style arrays are not replaced, while `std::optional` or
 * `std::variant` cannot store them.
 */
template <typename T>
struct non_value_wrapper : _::non_value_wrapper_impl<T> {
    using _::non_value_wrapper_impl<T>::non_value_wrapper_impl;
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
    T _ref;
public:
    explicit ref_awaiter(T ref) : _ref(static_cast<T>(ref)) {}
    constexpr bool await_ready()
        { return static_cast<T>(_ref).await_ready(); }
    constexpr decltype(auto) await_suspend(std::coroutine_handle<> ch)
        { return static_cast<T>(_ref).await_suspend(ch); }
    constexpr decltype(auto) await_resume()
        { return static_cast<T>(_ref).await_resume(); }
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
 * Currently, when used in `common_promise`, it represents `co_yield` (when used
 * as `Y`) or `co_return` (when used as `R`) is disabled.
 */
struct disable {};


/**
 * @brief Wraps a reference as non-suspending awaitable.
 */
template <typename T> requires std::is_reference_v<T>
struct immediately {
    T _ref;
public:
    explicit immediately(T ref) : _ref(static_cast<T>(ref)) {}
    constexpr bool await_ready() const noexcept { return true; }
    constexpr void await_suspend(std::coroutine_handle<> ch) const noexcept {}
    constexpr decltype(auto) await_resume() { return static_cast<T>(_ref); }
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

    explicit handle_manager(_Handle handle): _handle(handle) {}
    explicit handle_manager(P& promise):
        _handle(_Handle::from_promise(promise)) {}
    ~handle_manager() { destroy(); }

    handle_manager(const handle_manager<P>&) = delete;
    handle_manager(handle_manager<P>&& other) noexcept :
        _handle(std::exchange(other._handle, {})) {}

    P& promise() { return _handle.promise(); }
    _Handle handle() noexcept { return _handle; }
    _Handle transfer() noexcept { return std::exchange(_handle, {}); }
    void destroy() { if (_handle) { transfer().destroy(); } }
    bool done() { return _handle.done(); }
    void resume() { _handle.resume(); }
};

/**
 * @brief A simple promise type that returns `void` and catches exception.
 */
struct simple_promise {
    std::exception_ptr error = nullptr;

    void return_void() noexcept {}
    void unhandled_exception() noexcept
        { error = std::current_exception(); }

    decltype(auto) initial_suspend() noexcept
        { return std::suspend_always{}; }
    decltype(auto) final_suspend() noexcept
        { return std::suspend_always{}; }

    void check_error()
        { if (error) { std::rethrow_exception(error); } }
};

} // namespace coutils

#endif // __COUTILS_UTILITY__
