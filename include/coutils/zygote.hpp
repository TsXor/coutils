#pragma once
#ifndef __COUTILS_ZYGOTE__
#define __COUTILS_ZYGOTE__

#include <stdexcept>
#include <optional>
#include <coroutine>
#include "coutils/utility.hpp"
#include "coutils/traits.hpp"

namespace coutils {

enum class promise_state { PENDING, YIELDED, RECEIVED, RETURNED, ERROR };

/**
 * @brief Delegates `await_suspend` to `P::yield_suspend` and `await_resume`
 *        to `P::yield_resume`.
 */
template <typename P>
class yield_result {
    using enum promise_state;
    std::reference_wrapper<P> promise;
public:
    yield_result(P& p) : promise(p) {}
    constexpr bool await_ready() const noexcept { return false; }
    decltype(auto) await_suspend(std::coroutine_handle<> hd) const
        { return promise.get().yield_suspend(hd); }
    decltype(auto) await_resume() const
        { return promise.get().yield_resume(); }
};

namespace mixins {

/**
 * @brief Delegates `return_value` and `return_void` to unified `set_returned`.
 */
template <typename D, typename R>
class promise_return {
    decltype(auto) self() { return static_cast<D&>(*this); }
public:
    void return_value(auto&& expr) noexcept {
        self().template set_returned(COUTILS_FWD(expr));
    }
};

template <typename D>
class promise_return<D, void> {
    decltype(auto) self() { return static_cast<D&>(*this); }
public:
    void return_void() noexcept {
        self().template set_returned(std::monostate{});
    }
};

template <typename D>
struct promise_return<D, disable> {};


/**
 * @brief Delegates `yield_value` to `set_yielded` and `yield_result_type`.
 */
template <typename D, typename Y>
class promise_yield {
    decltype(auto) self() { return static_cast<D&>(*this); }
public:
    decltype(auto) yield_value(auto&& expr) noexcept {
        self().template set_yielded(COUTILS_FWD(expr));
        return yield_result<D>(self());
    }
};

template <typename D>
class promise_yield<D, void> {
    decltype(auto) self() { return static_cast<D&>(*this); }
public:
    decltype(auto) yield_value(std::monostate) noexcept {
        self().template set_yielded(std::monostate{});
        return yield_result<D>(self());
    }
};

template <typename D>
struct promise_yield<D, disable> {};

} // namespace mixins

/**
 * @brief A universal promise type.
 * 
 * This class provides a 5-state promise that is capable of most coroutine
 * features and properly handles exception.
 */
template <typename Y, typename S, typename R>
class zygote_promise:
    public mixins::promise_yield<zygote_promise<Y, S, R>, Y>,
    public mixins::promise_return<zygote_promise<Y, S, R>, R>
{
    using enum promise_state;

    template <typename T>
    static constexpr bool is_ref = std::is_reference_v<T>;
    template <typename T>
    static constexpr bool is_lref = std::is_lvalue_reference_v<T>;
    template <typename T>
    static constexpr bool is_lcref =
        is_lref<T> && std::is_const_v<std::remove_reference_t<T>>;
    template <typename From, typename To>
    static constexpr bool is_compatible =
        std::is_convertible_v<std::add_pointer_t<From>, std::add_pointer_t<To>>;

    template <promise_state status>
    decltype(auto) get_data()
        { return std::get<std::size_t(status)>(data); }

    template <promise_state status>
    void emplace(auto&&... args) noexcept try {
        data.template emplace<std::size_t(status)>(COUTILS_FWD(args)...);
    } catch(...) {
        data.template emplace<std::size_t(ERROR)>(std::current_exception());
    }

    template <promise_state status>
    void emplace_void() noexcept {
        data.template emplace<std::size_t(status)>();
    }

    template <promise_state status>
    void emplace_expr(auto&& expr) noexcept {
        using Expr = std::remove_cvref_t<decltype(expr)>;
        if constexpr (is_initializer_mark_v<Expr>) {
            // apply contained tuple to constructor
            std::apply([&](auto&&... args) {
                emplace<status>(COUTILS_FWD(args)...);
            }, COUTILS_FWD(expr).data);
        } else {
            emplace<status>(COUTILS_FWD(expr));
        }
    }

public:
    static_assert(
        !(std::is_array_v<Y> || std::is_array_v<S> || std::is_array_v<R>),
        "promise cannot hold a C-style array as value, try using std::array "
        "instead"
    );

    static_assert(
        std::is_same_v<Y, disable> == std::is_same_v<S, disable>,
        "yield and send must be both or neither disabled"
    );

    using yield_type = Y;
    using send_type = S;
    using return_type = R;

    wrap_variant<void, Y, S, R, std::exception_ptr> data;

    decltype(auto) status() const noexcept
        { return static_cast<promise_state>(data.index()); }

    void unhandled_exception() noexcept {
        data.template emplace<std::size_t(ERROR)>
            (std::current_exception());
    }

    template <traits::awaitable T>
    constexpr decltype(auto) await_transform(T&& obj)
        { return COUTILS_FWD(obj); }

    decltype(auto) initial_suspend() noexcept
        { return std::suspend_always{}; }
    decltype(auto) final_suspend() noexcept
        { return std::suspend_always{}; }

    decltype(auto) get_yielded() { return get_data<YIELDED>(); }
    decltype(auto) get_received() { return get_data<RECEIVED>(); }
    decltype(auto) get_returned() { return get_data<RETURNED>(); }
    decltype(auto) get_error() { return get_data<ERROR>(); }

    void set_yielded(auto&& expr) noexcept requires (is_ref<Y>) {
        using Expr = decltype(expr);
        // When yielded type is reference, yielded object must be "compatible"
        // with yielded type instead of "convertible" because temporaries
        // generated in conversion process will not be saved in the coroutine
        // stack, and is already destroyed after the coroutine is suspended,
        // which will cause yielded reference to be dangling.
        static_assert(is_compatible<Expr, Y>,
            "Passed object is incompatible with yielded type!"
        );
        if constexpr (is_lref<Y>) {
            static_assert(is_lref<Expr> || is_lcref<Y>,
                "Yielded type is non-const lvalue reference, cannot yield rvalue!"
            );
            emplace<YIELDED>(expr);
        } else {
            static_assert(!is_lref<Expr>,
                "Yielded type is rvalue reference, cannot yield lvalue! Manual "
                "std::move might be needed."
            );
            emplace<YIELDED>(std::move(expr));
        }
    }

    void set_yielded(auto&& expr) noexcept requires (!is_ref<Y>) {
        if constexpr (std::is_void_v<Y>) { emplace_void<YIELDED>(); }
        else { emplace_expr<YIELDED>(COUTILS_FWD(expr)); }
    }
    void set_received(auto&&... args) noexcept {
        if constexpr (std::is_void_v<S>) { emplace_void<RECEIVED>(); }
        else { emplace_expr<RECEIVED>(COUTILS_FWD(args)...); }
    }
    void set_returned(auto&& expr) noexcept {
        if constexpr (std::is_void_v<R>) { emplace_void<RETURNED>(); }
        else { emplace_expr<RETURNED>(COUTILS_FWD(expr)); }
    }

    template <promise_state status>
    void check_value() {
        if (this->status() == status) { return; }
        switch (this->status()) {
            case PENDING: throw std::logic_error("promise pending");
            case RECEIVED: throw std::logic_error("promise received");
            case YIELDED: throw std::logic_error("promise yielded");
            case RETURNED: throw std::logic_error("promise returned");
            case ERROR: std::rethrow_exception(get_error());
            default: throw std::runtime_error("invalid promise state");
        }
    }

    void check_error()
        { if (status() == ERROR) { std::rethrow_exception(get_error()); } }

    void set_default_received() {
        if constexpr (std::default_initializable<S> || std::is_void_v<S>)
            { if (status() == YIELDED) { set_received(); } }
    }

    constexpr void yield_suspend(std::coroutine_handle<>) const noexcept {}

    decltype(auto) yield_resume() {
        set_default_received();
        check_value<RECEIVED>();
        return get_received();
    }
};


template <typename Y, typename S, typename R>
using zygote_handle = std::coroutine_handle<zygote_promise<Y, S, R>>;


/**
 * @brief Wraps a handle of `zygote_promise`.
 * 
 * The third template parameter is for extending, replace it with subclass.
 */
template <typename Y, typename S, typename R,
    std::derived_from<zygote_promise<Y, S, R>> P = zygote_promise<Y, S, R>
> class zygote : public handle_manager<P> {
    using enum promise_state;

public:
    using typename handle_manager<P>::handle_type;
    using yield_type = Y;
    using send_type = S;
    using return_type = R;

    using handle_manager<P>::handle_manager;
    using handle_manager<P>::promise;
    using handle_manager<P>::handle;
    using handle_manager<P>::transfer;
    using handle_manager<P>::destroy;
    using handle_manager<P>::done;
    using handle_manager<P>::resume;

    promise_state status() noexcept { return promise().status(); }

    template <promise_state state>
#ifdef DEBUG
    void assert_state() { promise().template check_value<state>(); }
#else
    void assert_state() {}
#endif

    decltype(auto) yielded() { return promise().get_yielded(); }
    decltype(auto) returned() { return promise().get_returned(); }
    void check_error() { promise().check_error(); }

    void send(auto&&... args) requires (!std::is_void_v<S>) {
        P& p = promise();
        p.set_received(COUTILS_FWD(args)...);
        p.template check_value<RECEIVED>();
    }

    class result_wrapper : private handle_manager<P> {
        using handle_manager<P>::promise;
    public:
        using handle_manager<P>::handle_manager;
        R& get() { return promise().get_returned(); }
        R& operator*() { return get(); }
        R* operator->() { return std::addressof(get()); }
    };

    decltype(auto) move_out_returned() {
        P& p = promise();
        p.template check_value<RETURNED>();
        if constexpr (non_value<R>) {
            auto&& r = p.get_returned();
            destroy(); return static_cast<R>(r);
        } else if constexpr (reconstructible<R>) {
            R r = std::move(p.get_returned());
            destroy(); return r;
        } else {
            return result_wrapper(transfer());
        }
    }
};

} // namespace coutils

#endif // __COUTILS_ZYGOTE__
