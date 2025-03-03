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


/**
 * @brief Get I-th type in a type pack.
 */
template <std::size_t I, typename... Ts>
struct typack_index;

template <std::size_t I>
struct typack_index<I> {};

template <typename T, typename... Left>
struct typack_index<0, T, Left...> { using type = T; };

template <std::size_t I, typename T, typename... Left>
struct typack_index<I, T, Left...> : typack_index<I - 1, Left...> {};

template <std::size_t I, typename... Ts>
using typack_index_t = typename typack_index<I, Ts...>::type;


/**
 * @brief Cast reference to given type, automatically keeping lvalue/rvalue
 *        reference type.
 */
template <typename T> requires (!std::is_reference_v<T>)
decltype(auto) cast_ref(auto&& ref) {
    if constexpr (std::is_lvalue_reference_v<decltype(ref)>)
        { return static_cast<T&>(ref); }
    else { return static_cast<T&&>(ref); }
}


template <typename T>
concept non_value = std::is_void_v<T> || std::is_reference_v<T>;

namespace _ {

template <typename T, bool = std::is_void_v<T>, bool = std::is_reference_v<T>>
struct non_value_wrapper_impl {};

template <typename T>
struct non_value_wrapper_impl<T, false, false> : leaf<T> {
    using type = T;
    using unwrap_type = T;
    using leaf<T>::leaf;
};

template <typename T>
struct non_value_wrapper_impl<T, true, false> {
    using type = T;
    using unwrap_type = std::monostate;
    explicit non_value_wrapper_impl() = default;
    explicit non_value_wrapper_impl(std::monostate) {}
    std::monostate get() { return {}; }
};

template <typename T>
struct non_value_wrapper_impl<T, false, true> : ref<T> {
    using type = T;
    using unwrap_type = T;
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

/**
 * @brief Checks if a type is instantiation of `non_value_wrapper`.
 */
template <typename T>
struct is_non_value_wrapper : public std::false_type {};
template <typename T>
struct is_non_value_wrapper<non_value_wrapper<T>> : public std::true_type {};
template <typename T>
inline constexpr bool is_non_value_wrapper_v = is_non_value_wrapper<T>::value;


namespace _ {

template <std::size_t I, typename T>
decltype(auto) super_get(T&& obj) {
    using _Super = typename std::remove_cvref_t<T>::super_type;
    return std::get<I>(cast_ref<_Super>(COUTILS_FWD(obj))).get();
}

} // namespace _


template <typename... Ts>
struct wrap_tuple : public std::tuple<non_value_wrapper<Ts>...> {
    using super_type = std::tuple<non_value_wrapper<Ts>...>;
    using super_type::super_type;
};

/**
 * @brief Checks if a type is instantiation of `wrap_tuple`.
 */
template <typename T>
struct is_wrap_tuple : public std::false_type {};
template <typename... Ts>
struct is_wrap_tuple<wrap_tuple<Ts...>> : public std::true_type {};
template <typename T>
inline constexpr bool is_wrap_tuple_v = is_wrap_tuple<T>::value;

} // namespace coutils

template<typename... Ts>
struct std::tuple_size<coutils::wrap_tuple<Ts...>>:
    std::integral_constant<std::size_t, sizeof...(Ts)> {};

template<std::size_t I, typename... Ts>
struct std::tuple_element<I, coutils::wrap_tuple<Ts...>> {
    using type = typename coutils::non_value_wrapper<
        coutils::typack_index_t<I, Ts...>
    >::unwrap_type;
};

template<size_t I, typename... Ts>
constexpr decltype(auto) std::get(coutils::wrap_tuple<Ts...>& t) noexcept
    { return coutils::_::super_get<I>(COUTILS_FWD(t)); }
template<size_t I, typename... Ts>
constexpr decltype(auto) std::get(const coutils::wrap_tuple<Ts...>& t) noexcept
    { return coutils::_::super_get<I>(COUTILS_FWD(t)); }
template<size_t I, typename... Ts>
constexpr decltype(auto) std::get(coutils::wrap_tuple<Ts...>&& t) noexcept
    { return coutils::_::super_get<I>(COUTILS_FWD(t)); }

namespace coutils {


template <typename... Ts>
struct wrap_variant : public std::variant<non_value_wrapper<Ts>...> {
    using super_type = std::variant<non_value_wrapper<Ts>...>;
    using super_type::super_type;
};

/**
 * @brief Checks if a type is instantiation of `wrap_variant`.
 */
template <typename T>
struct is_wrap_variant : public std::false_type {};
template <typename... Ts>
struct is_wrap_variant<wrap_variant<Ts...>> : public std::true_type {};
template <typename T>
inline constexpr bool is_wrap_variant_v = is_wrap_variant<T>::value;

} // namespace coutils

template<typename... Ts>
struct std::variant_size<coutils::wrap_variant<Ts...>>:
    std::integral_constant<std::size_t, sizeof...(Ts)> {};

template<std::size_t I, typename... Ts>
struct std::variant_alternative<I, coutils::wrap_variant<Ts...>> {
    using type = typename coutils::non_value_wrapper<
        coutils::typack_index_t<I, Ts...>
    >::unwrap_type;
};

template<size_t I, typename... Ts>
constexpr decltype(auto) std::get(coutils::wrap_variant<Ts...>& v) noexcept
    { return coutils::_::super_get<I>(COUTILS_FWD(v)); }
template<size_t I, typename... Ts>
constexpr decltype(auto) std::get(const coutils::wrap_variant<Ts...>& v) noexcept
    { return coutils::_::super_get<I>(COUTILS_FWD(v)); }
template<size_t I, typename... Ts>
constexpr decltype(auto) std::get(coutils::wrap_variant<Ts...>&& v) noexcept
    { return coutils::_::super_get<I>(COUTILS_FWD(v)); }

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
 * @brief A simple promise type that returns `void` and terminates on exception.
 */
struct simple_promise {
    void return_void() noexcept {}
    [[noreturn]] void unhandled_exception() noexcept { std::terminate(); }

    decltype(auto) initial_suspend() noexcept
        { return std::suspend_always{}; }
    decltype(auto) final_suspend() noexcept
        { return std::suspend_always{}; }
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
