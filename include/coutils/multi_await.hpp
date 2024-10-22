#pragma once
#ifndef __COUTILS_MULTI_AWAIT__
#define __COUTILS_MULTI_AWAIT__

#include <array>
#include <mutex>
#include <memory>
#include "coutils/utility.hpp"
#include "coutils/traits.hpp"
#include "coutils/agent.hpp"

namespace coutils {

namespace _ {

struct shim_controller_base {
    static constexpr auto npos = std::size_t(-1);
    light_lock lock;
    std::coroutine_handle<> caller = {};

    constexpr bool await_ready() const noexcept
        { return caller.address() == nullptr; }
    constexpr decltype(auto) await_suspend(std::coroutine_handle<> ch)
        noexcept { return std::exchange(caller, ch); }
    constexpr void await_resume() const noexcept {}
};

template <traits::awaitable... Ts>
struct await_storage {
private:
    template <typename T>
    using _Rval = non_value_wrapper<traits::co_await_t<T>>;

    template <std::size_t... Is>
    await_storage(std::index_sequence<Is...>, auto&&... args):
        awaitables(COUTILS_FWD(args)...),
        awaiters(ops::get_awaiter(static_cast<Ts&&>(std::get<Is>(awaitables)))...) {}

public:
    std::tuple<Ts...> awaitables;
    std::tuple<traits::awaiter_cvt_t<Ts>...> awaiters;

    await_storage(auto&&... args) requires (sizeof...(args) == sizeof...(Ts)) :
        await_storage(std::index_sequence_for<Ts...>{}, COUTILS_FWD(args)...) {}

    using any_result = std::variant<_Rval<Ts>..., std::monostate>;
    using all_result = std::tuple<_Rval<Ts>...>;

    void launch(auto&& gen_handle) {
        [&] <std::size_t... Is> (std::index_sequence<Is...>) {
            using _Handles = std::array<std::coroutine_handle<>, sizeof...(Ts)>;
            auto handles = _Handles{gen_handle(Is)...};
            (..., ops::await_launch(std::get<Is>(awaiters), handles[Is]));
        } (std::index_sequence_for<Ts...>{});
    }

    void get_any(std::size_t idx, any_result& result) {
        visit_index<sizeof...(Ts)>(idx,
            [&] <std::size_t I> (index_constant<I>) {
                result.template emplace<I>(
                    ops::await_resume(std::get<I>(awaiters))
                );
            }
        );
    }

    all_result get_all() {
        return [&] <std::size_t... Is> (std::index_sequence<Is...>) {
            return all_result(
                ops::await_resume(std::get<Is>(awaiters))...
            );
        } (std::index_sequence_for<Ts...>{});
    }
};

} // namespace _

#pragma region as_completed

namespace _ {

struct as_completed_shim {
    struct controller : shim_controller_base {
        std::size_t id = npos;
    };

    static agent shim(std::size_t id, std::shared_ptr<controller> control) noexcept {
        auto guard = std::lock_guard(control->lock);
        control->id = id;
        co_await *control;
        // When parent is alive and we are the last one,
        // transfer control to parent again.
        if (control.use_count() == 2 && control->caller) {
            auto& dropped_control = *control.get();
            control.reset();
            co_await dropped_control;
        }
    }
};

} // namespace _

/**
 * @brief Launch multiple awaitables and consume their output one by one.
 * 
 * The result is a variant of `non_value_wrapper` of all results. You can check
 * its `.index()` to know which result it contains and use `coutils::visit_index`
 * (or `std::visit`, but it cannot utilize index value) to execute corresponding
 * operations. After result is extracted from the variant, use its `.get()`
 * method to get its contained value or reference.
 * 
 * By default (when directly constructing this class like `as_completed(...)`),
 * rvalue reference parameters are moved into this class, which means
 * awaitables (and their awaiters) should have an eligible move constructor
 * (or copy constructor).
 * 
 * First, the awaitables will be started serially on the same thread as caller,
 * then, as the iterator increments, the caller may be resumed on different
 * threads, but it is guarded with a lock so that only one thread can resume
 * and execute the caller at the same time. When iteration ends, the caller is
 * resumed by the last fulfilled awaitable.
 * 
 * If awaitables are not all consumed when this is destructed, the ones left
 * will have their result dropped.
 * 
 * This class will cause N + 1 heap allocations (N for N shim coroutines and 1
 * for a control block) when `begin()` is first called.
 */
template <traits::awaitable... Ts>
class as_completed : private _::as_completed_shim {
    using _::as_completed_shim::shim;
    using _::as_completed_shim::controller;

    using _Self = as_completed<Ts...>;
    using _Storage = _::await_storage<Ts...>;

    _Storage storage;
    std::shared_ptr<controller> control;
    _Storage::any_result result;

    bool finished() const { return control.use_count() == 1; }

    void on_suspend(std::coroutine_handle<> ch) {
        if (control.use_count() == 0) {
            control = std::make_shared<controller>();
            control->caller = ch;
            control->id = sizeof...(Ts);
            storage.launch([&](size_t idx) {
                return shim(idx, control).transfer();
            });
        } else {
            std::exchange(control->caller, ch).resume();
        }
    }

    void on_resume() {
        if (control.use_count() == 1) {
            // Finalize the last shim.
            std::exchange(control->caller, {}).resume();
            result.template emplace<sizeof...(Ts)>();
        } else {
            storage.get_any(control->id, result);
        }
    }

    void drop_left() {
        if (control->caller) {
            std::exchange(control->caller, {}).resume();
        }
    }

public:
    as_completed(auto&&... args) : storage(COUTILS_FWD(args)...),
        result(std::in_place_index<sizeof...(Ts)>) {}

    ~as_completed() { drop_left(); }

    friend class iterator;
    class iterator {
        _Self* ptr;

    public:
        iterator(_Self& parent):
            ptr(std::addressof(parent)) {}

        iterator(const iterator&) = delete;
        iterator(iterator&& other):
            ptr(std::exchange(other.ptr, nullptr)) {}

        bool operator==(std::default_sentinel_t) { return ptr->finished(); }
        decltype(auto) operator*() { return (ptr->result); }
        decltype(auto) operator->() { return std::addressof(*(*this)); }
        iterator& operator++() & { return *this; }
        iterator operator++() && { return std::move(*this); }

        constexpr bool await_ready() const noexcept { return false; }
        decltype(auto) await_suspend(std::coroutine_handle<> ch)
            { return ptr->on_suspend(ch); }
        iterator& await_resume() & { ptr->on_resume(); return *this; }
        iterator await_resume() && { ptr->on_resume(); return std::move(*this); }

        COUTILS_REF_AWAITER_CONV_OVERLOAD
    };

    decltype(auto) begin() { return iterator(*this); }
    decltype(auto) end() const { return std::default_sentinel; }
};

template <traits::awaitable... Ts>
as_completed(Ts&&...) -> as_completed<Ts...>;

#pragma endregion as_completed

#pragma region all_completed

namespace _ {

struct all_completed_shim {
    struct controller : shim_controller_base {
        std::size_t count = npos;
    };

    static agent shim(size_t id, controller& control) noexcept {
        auto guard = std::lock_guard(control.lock);
        --(control.count);
        // When we are the last one, transfer control to parent.
        if (control.count == 0) { co_await control; }
    }
};

} // namespace _

/**
 * @brief Launch multiple awaitables and get their output when all of them are
 * completed.
 * 
 * The result is a tuple of `non_value_wrapper` of all results (this is needed
 * to hold `void` results and make resulting tuple copy/move constructible).
 * After result is extracted from the tuple, use its `.get()`
 * method to get its contained value or reference.
 * 
 * By default (when directly constructing this class like `all_completed(...)`),
 * rvalue reference parameters are moved into this class, which means
 * awaitables (and their awaiters) should have an eligible move constructor
 * (or copy constructor).
 * 
 * First, the awaitables will be started serially on the same thread as caller,
 * then, the caller will be resumed when all awaitables are fulfilled. The
 * caller is resumed by the last fulfilled awaitable.
 * 
 * This class will cause N heap allocations (for N shim coroutines) when awaited.
 */
template <traits::awaitable... Ts>
class all_completed : private _::all_completed_shim {
    using _::all_completed_shim::shim;
    using _::all_completed_shim::controller;

    using _Storage = _::await_storage<Ts...>;

    _Storage storage;
    controller control;

public:
    template <std::size_t... Is>
    all_completed(auto&&... args) : storage(COUTILS_FWD(args)...) {}

    constexpr bool await_ready() const noexcept { return false; }

    decltype(auto) await_suspend(std::coroutine_handle<> ch) {
        control.caller = ch;
        control.count = sizeof...(Ts);
        storage.launch([&](size_t idx) {
            return shim(idx, control).transfer();
        });
    }

    _Storage::all_result await_resume() {
        std::exchange(control.caller, {}).resume();
        return storage.get_all();
    }
};

template <traits::awaitable... Ts>
all_completed(Ts&&...) -> all_completed<Ts...>;

#pragma endregion all_completed

} // namespace coutils

#endif // __COUTILS_MULTI_AWAIT__
