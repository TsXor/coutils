#pragma once
#ifndef __COUTILS_MULTI_AWAIT__
#define __COUTILS_MULTI_AWAIT__

#include <array>
#include <mutex>
#include <memory>
#include <span>
#include "coutils/value_wrapper.hpp"
#include "coutils/utility.hpp"
#include "coutils/traits.hpp"
#include "coutils/agent.hpp"

namespace coutils {

namespace _ {

template <traits::awaitable... Ts>
struct await_storage {
private:
    template <std::size_t... Is>
    await_storage(std::index_sequence<Is...>, auto&&... args):
        awaitables(COUTILS_FWD(args)...),
        awaiters(ops::get_awaiter(static_cast<Ts&&>(std::get<Is>(awaitables)))...) {}

public:
    wrap_tuple<Ts...> awaitables;
    wrap_tuple<traits::awaiter_cvt_t<Ts>...> awaiters;

    await_storage(auto&&... args) requires (sizeof...(args) == sizeof...(Ts)) :
        await_storage(std::index_sequence_for<Ts...>{}, COUTILS_FWD(args)...) {}

    using any_result = wrap_variant<traits::co_await_t<Ts>...>;
    using all_result = wrap_tuple<traits::co_await_t<Ts>...>;

    void launch(auto&& gen_handle) {
        [&] <std::size_t... Is> (std::index_sequence<Is...>) {
            using _Handles = std::array<std::coroutine_handle<>, sizeof...(Ts)>;
            auto handles = _Handles{gen_handle(Is)...};
            (..., ops::await_launch(std::get<Is>(awaiters), handles[Is]));
        } (std::index_sequence_for<Ts...>{});
    }

    any_result get_any(std::size_t idx) {
        return visit_index<sizeof...(Ts)>(idx,
            COUTILS_VISITOR(I) {
                return any_result(std::in_place_index<I>,
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
    using enum std::memory_order;

    struct controller {
        std::coroutine_handle<> caller = nullptr;
        light_lock lock;
        std::atomic<std::size_t> finished = 0, consumed = 0;
        std::span<std::size_t> order;

        template<typename U, std::size_t N>
        controller(std::array<U, N>& arr) : order(arr) {}

        bool finish(std::size_t id) {
            bool caller_blocked;
            {
                auto guard = std::lock_guard(lock);
                // here is actually a push operation of a SPSC queue
                auto v_finished = finished.load(relaxed);
                auto v_consumed = consumed.load(acquire);
                caller_blocked = v_consumed == v_finished && caller;
                order[v_finished] = id;
                finished.store(v_finished + 1, release);
            }
            return caller_blocked;
        }

        bool consume(std::coroutine_handle<> ch) {
            bool should_suspend;
            {
                // here is actually a pop operation of a SPSC queue
                auto v_consumed = consumed.load(relaxed);
                auto v_finished = finished.load(acquire);
                caller = ch;
                should_suspend = v_consumed + 1 == v_finished && v_consumed + 1 < order.size();
                consumed.store(v_consumed + 1, release);
            }
            return should_suspend;
        }
    };

    static agent shim(std::size_t id, std::shared_ptr<controller> control) noexcept {
        if (control->finish(id)) { control->caller.resume(); }
        co_return;
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
 * threads. After iteration ends, the caller may be resumed any one of given
 * awaitables.
 * 
 * If awaitables are not all consumed when this is destructed, the ones left
 * will have their result dropped.
 * 
 * This class will cause N + 1 heap allocations (N for N shim coroutines and 1
 * for a control block) when `begin()` is first called.
 */
template <traits::awaitable... Ts>
class as_completed : private _::as_completed_shim {
    using enum std::memory_order;
    using _::as_completed_shim::shim;
    using _::as_completed_shim::controller;

    using _Self = as_completed<Ts...>;
    using _Storage = _::await_storage<Ts...>;

    _Storage storage;
    std::array<std::size_t, sizeof...(Ts)> order;
    std::shared_ptr<controller> control;

    std::size_t consumed() const { return control->consumed.load(relaxed); }
    std::size_t finished() const { return control->finished.load(relaxed); }

    auto finish_order() const
        { return std::span<const std::size_t>(order.data(), finished()); }

    bool all_consumed() const { return consumed() == size; }

    bool on_suspend(std::coroutine_handle<> ch) {
        if (control.use_count() == 0) {
            control = std::make_shared<controller>(order);
            control->caller = ch;
            storage.launch([&](size_t idx) {
                return shim(idx, control).handle;
            });
            return true;
        }
        return control->consume(ch);
    }

    decltype(auto) get_result() {
        auto consumed = control->consumed.load(relaxed);
        return storage.get_any(consumed);
    }

    void drop_left() {
        auto consumed = control->consumed.load(relaxed);
        auto finished = control->finished.load(acquire);
        control->caller = {}; ++consumed;
        control->consumed.store(consumed, release);
    }

public:
    as_completed(auto&&... args) : storage(COUTILS_FWD(args)...) {}
    ~as_completed() { drop_left(); }

    constexpr static std::size_t size = sizeof...(Ts);

    friend class iterator;
    class iterator {
        _Self* ptr;

    public:
        iterator(_Self& parent):
            ptr(std::addressof(parent)) {}

        iterator(const iterator&) = delete;
        iterator(iterator&& other):
            ptr(std::exchange(other.ptr, nullptr)) {}

        std::size_t n_consumed() const { return ptr->consumed(); }
        std::size_t n_finished() const { return ptr->finished(); }
        decltype(auto) finish_order() const { return ptr->finish_order(); }

        bool operator==(std::default_sentinel_t) { return ptr->all_consumed(); }
        decltype(auto) operator*() { return ptr->get_result(); }
        iterator& operator++() & { return *this; }

        constexpr bool await_ready() const noexcept { return false; }
        decltype(auto) await_suspend(std::coroutine_handle<> ch)
            { return ptr->on_suspend(ch); }
        void await_resume() {}
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
    struct controller {
        std::coroutine_handle<> caller;
        std::atomic<std::size_t> count;
    };

    static agent shim(size_t id, controller& control) noexcept {
        // When we are the last one, resume parent.
        if (--control.count == 0) { control.caller.resume(); }
        co_return;
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
 * caller may be resumed by any one of given awaitables.
 * 
 * This class will cause N + 1 heap allocations (N for N shim coroutines and 1
 * for a control block) when awaited.
 */
template <traits::awaitable... Ts>
class all_completed : private _::all_completed_shim {
    using _::all_completed_shim::shim;
    using _::all_completed_shim::controller;

    using _Storage = _::await_storage<Ts...>;

    _Storage storage;
    std::unique_ptr<controller> control;

public:
    template <std::size_t... Is>
    all_completed(auto&&... args) : storage(COUTILS_FWD(args)...) {}

    constexpr static std::size_t size = sizeof...(Ts);

    constexpr bool await_ready() const noexcept { return false; }

    decltype(auto) await_suspend(std::coroutine_handle<> ch) {
        control = std::make_unique<controller>();
        control->caller = ch;
        control->count = size;
        storage.launch([&](size_t idx) {
            return shim(idx, *control).handle;
        });
    }

    _Storage::all_result await_resume() {
        return storage.get_all();
    }
};

template <traits::awaitable... Ts>
all_completed(Ts&&...) -> all_completed<Ts...>;

#pragma endregion all_completed

} // namespace coutils

#endif // __COUTILS_MULTI_AWAIT__
