#include <iostream>
#include <coutils.hpp>

static inline std::ostream& operator<<(std::ostream& os, std::monostate) {
    return (os << "<std::monostate>");
}

template <typename T>
static inline std::ostream& operator<<(std::ostream& os, coutils::non_value_wrapper<T>& val) {
    return (os << val.get());
}

coutils::async_fn<void> task_a() { co_return; }
coutils::async_fn<int> task_b() { co_return 42; }

coutils::async_fn<void> test() {
    std::cout << "coutils::all_completed:" << std::endl;
    auto [a, b] = co_await coutils::all_completed(task_a(), task_b());
    std::cout << "(" << a << ", " << b << ")" << std::endl;

    std::cout << "coutils::as_completed:" << std::endl;
    COUTILS_FOR(auto&& var, coutils::as_completed(task_a(), task_b()))
        coutils::visit_variant(var, COUTILS_VISITOR(I) {
            auto&& val = coutils::get_unwrap<I>(var);
            std::cout << "[" << I << "]: " << val << std::endl;
        });
    COUTILS_ENDFOR()
}

int main() {
    coutils::wait(test());
}
