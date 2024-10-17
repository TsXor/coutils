#include <iostream>
#include <coutils.hpp>

using uint = unsigned int;

coutils::async_fn<uint> identity(uint n) { co_return n; }

auto iota(uint n) -> coutils::async_generator<uint> {
    for (uint i = 0; i < n; ++i) {
        auto r = co_await identity(i);
        co_yield r;
    }
}

coutils::async_fn<void> test() {
    COUTILS_FOR(auto v, iota(42))
        std::cout << v << ' ';
    COUTILS_ENDFOR()
}

int main() {
    coutils::wait(test());
}
