#pragma once
#ifndef __COUTILS_VALUE_WRAPPER__
#define __COUTILS_VALUE_WRAPPER__

#include <utility>
#include <type_traits>
#include <tuple>
#include <variant>
#include "coutils/macros.hpp"

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
    ref(T&& t) noexcept : ptr(std::addressof(t)) {}
    ref(const ref&) = default;
    ref& operator=(const ref&) = default;
    R get() noexcept { return static_cast<R>(*ptr); }
    operator R() noexcept { return get(); }
};

/**
 * @brief Optional reference wrapper.
 */
template <typename R> requires std::is_reference_v<R>
class optref {
    std::remove_reference_t<R>* ptr;
public:
    using type = R;
    optref() noexcept : ptr(nullptr) {}
    template <typename T> requires std::is_convertible_v<T&&, R>
    optref(T&& t) noexcept : ptr(std::addressof(t)) {}
    optref(const optref&) = default;
    optref& operator=(const optref&) = default;
    bool has_value() noexcept { return ptr != nullptr; }
    R get() noexcept { return static_cast<R>(*ptr); }
    operator R() noexcept { return get(); }
};


/**
 * @brief Wraps arbitrary class.
 */
template <typename T>
class leaf {
    [[no_unique_address]] T value;

public:
    explicit leaf()
        noexcept(std::is_nothrow_default_constructible_v<T>)
        requires std::default_initializable<T> {}

    explicit leaf(auto&&... args)
        noexcept(std::is_nothrow_constructible_v<T, decltype(args)...>)
        requires std::constructible_from<T, decltype(args)...> :
        value(COUTILS_FWD(args)...) {}

    leaf(const leaf<T>& other)
        noexcept(std::is_nothrow_copy_constructible_v<T>)
        requires std::copy_constructible<T> :
        value(other.value) {}
    leaf(leaf<T>&& other)
        noexcept(std::is_nothrow_move_constructible_v<T>)
        requires std::move_constructible<T> :
        value(std::move(other.value)) {}

    T& get() & noexcept { return value; }
    const T& get() const& noexcept { return value; }
    T&& get() && noexcept { return std::move(value); }
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
concept reconstructible =
    std::copy_constructible<T> || std::move_constructible<T>;


template <typename T>
concept non_value = std::is_void_v<T> || std::is_reference_v<T>;

namespace _ {

template <typename T>
struct non_value_wrapper_impl;

template <typename T> requires std::is_object_v<T>
struct non_value_wrapper_impl<T> : leaf<T> {
    using type = T;
    using unwrap_type = T;
    using leaf<T>::leaf;
};

template <typename T> requires std::is_void_v<T>
struct non_value_wrapper_impl<T> {
    using type = T;
    using unwrap_type = std::monostate;
    explicit non_value_wrapper_impl() = default;
    explicit non_value_wrapper_impl(std::monostate) {}
    std::monostate get() { return {}; }
};

template <typename T> requires std::is_reference_v<T>
struct non_value_wrapper_impl<T> : ref<T> {
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


template <typename... Ts>
struct wrap_tuple : public std::tuple<non_value_wrapper<Ts>...> {
private:
    using _Super = std::tuple<non_value_wrapper<Ts>...>;
    template <std::size_t I>
    decltype(auto) _get(auto&& self) noexcept
        { return std::get<I>(cast_ref<_Super>(COUTILS_FWD(self))).get(); }
public:
    using _Super::_Super;
    template <std::size_t I>
    decltype(auto) get() & noexcept { return _get<I>(*this); }
    template <std::size_t I>
    decltype(auto) get() const& noexcept { return _get<I>(*this); }
    template <std::size_t I>
    decltype(auto) get() && noexcept { return _get<I>(std::move(*this)); }
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


template <typename... Ts>
struct wrap_variant : public std::variant<non_value_wrapper<Ts>...> {
private:
    using _Super = std::variant<non_value_wrapper<Ts>...>;
    template <std::size_t I>
    decltype(auto) _get(auto&& self) noexcept
        { return std::get<I>(cast_ref<_Super>(COUTILS_FWD(self))).get(); }
public:
    using _Super::_Super;
    template <std::size_t I>
    decltype(auto) get() & noexcept { return _get<I>(*this); }
    template <std::size_t I>
    decltype(auto) get() const& noexcept { return _get<I>(*this); }
    template <std::size_t I>
    decltype(auto) get() && noexcept { return _get<I>(std::move(*this)); }
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
struct std::tuple_size<coutils::wrap_tuple<Ts...>>:
    std::integral_constant<std::size_t, sizeof...(Ts)> {};

template<std::size_t I, typename... Ts>
struct std::tuple_element<I, coutils::wrap_tuple<Ts...>> {
    using type = typename coutils::non_value_wrapper<
        coutils::typack_index_t<I, Ts...>
    >::unwrap_type;
};

template<std::size_t I, typename Tuple>
    requires coutils::is_wrap_tuple_v<std::remove_cvref_t<Tuple>>
constexpr decltype(auto) std::get(Tuple&& t) noexcept
    { return COUTILS_FWD(t).template get<I>(); }


template<typename... Ts>
struct std::variant_size<coutils::wrap_variant<Ts...>>:
    std::integral_constant<std::size_t, sizeof...(Ts)> {};

template<std::size_t I, typename... Ts>
struct std::variant_alternative<I, coutils::wrap_variant<Ts...>> {
    using type = typename coutils::non_value_wrapper<
        coutils::typack_index_t<I, Ts...>
    >::unwrap_type;
};

template<std::size_t I, typename Variant>
    requires coutils::is_wrap_variant_v<std::remove_cvref_t<Variant>>
constexpr decltype(auto) std::get(Variant&& t) noexcept
    { return COUTILS_FWD(t).template get<I>(); }

#endif // __COUTILS_VALUE_WRAPPER__
