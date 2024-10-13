#include <iostream>
#include <coutils.hpp>

auto fibonacci_sequence(unsigned n) -> coutils::generator<std::uint64_t> {
    if (n > 94)
        throw std::runtime_error("Too big Fibonacci sequence. Elements would overflow.");
    
    if (n == 0) co_return;
    co_yield 0;
    if (n == 1) co_return;
    co_yield 1;
    if (n == 2) co_return;

    std::uint64_t a = 0;
    std::uint64_t b = 1;

    for (unsigned i = 2; i < n; ++i) {
        std::uint64_t s = a + b;
        co_yield s;
        a = b; b = s;
    }
}

int main() {
    auto gen_and_print = [] (unsigned n) {
        for (auto v : fibonacci_sequence(n)) {
            std::cout << v << ' ';
        }
        std::cout << std::endl;
    };

    gen_and_print(42);

    try {
        gen_and_print(100); // throws an exception
    } catch (const std::exception& exc) {
        std::cerr << "Caught exception: " << exc.what() << std::endl; 
    }
}
