#include <iostream>
#include <coutils.hpp>

static inline std::ostream& operator<<(std::ostream& os, coutils::empty_slot_t es) {
    os << "<coutils::empty_slot>"; return os;
}

coutils::async_fn<void> task_a() { co_return; }
coutils::async_fn<int> task_b() { co_return 42; }

coutils::async_fn<void> test() {
    auto [a, b] = co_await coutils::all_completed(task_a(), task_b());
    std::cout << a << ' ' << b << std::endl;
}

int main() {
    coutils::sync::run_join(test());
}
