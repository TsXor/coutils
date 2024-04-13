#pragma once
#ifndef __COUTILS_INITIALIZER_TUPLE__
#define __COUTILS_INITIALIZER_TUPLE__

#include <tuple>
#include <type_traits>

namespace coutils {

template <typename... InitTs>
struct initializer_tuple {
    std::tuple<InitTs...> data;
    static constexpr std::size_t size = std::tuple_size_v<decltype(data)>;

    template <typename... ArgTs>
    initializer_tuple(ArgTs&&... args) : data(std::forward<ArgTs>(args)...) {}
    
    template <std::size_t I> auto get() & noexcept { return std::get<I>(data); }
    template <std::size_t I> auto get() const& noexcept { return std::get<I>(data); }
    template <std::size_t I> auto get() && noexcept { return std::get<I>(std::move(data)); }
    template <std::size_t I> auto get() const&& noexcept { return std::get<I>(std::move(data)); }
};

template <typename T>
struct is_initializer_tuple : public std::false_type {};
template <typename... InitTs>
struct is_initializer_tuple<initializer_tuple<InitTs...>> : public std::true_type {};
template <typename T>
inline constexpr bool is_initializer_tuple_v = is_initializer_tuple<T>::value;
template <typename T>
using is_initializer_tuple_ref = is_initializer_tuple<std::remove_cvref_t<T>>;
template <typename T>
inline constexpr bool is_initializer_tuple_ref_v = is_initializer_tuple_ref<T>::value;

namespace detail {

template <typename ContainerT, typename InitT, std::size_t... I>
auto emplace_apply_impl(ContainerT&& container, InitT&& init, std::index_sequence<I...>) {
    return std::forward<ContainerT>(container).emplace(std::forward<InitT>(init).template get<I>()...);
}

} // namespace detail

template <typename ContainerT, typename InitT>
    requires is_initializer_tuple_ref_v<InitT>
auto emplace_apply(ContainerT&& container, InitT&& init) {
    return detail::emplace_apply_impl(
        std::forward<ContainerT>(container), std::forward<InitT>(init),
        std::make_index_sequence<init.size>{}
    );
}

// make_initializer_tuple
template <typename... ArgTs>
constexpr auto inituple(ArgTs&&... args) {
    return initializer_tuple<std::unwrap_ref_decay_t<ArgTs>...>(std::forward<ArgTs>(args)...);
}

} // namespace coutils

#endif // __COUTILS_INITIALIZER_TUPLE__
