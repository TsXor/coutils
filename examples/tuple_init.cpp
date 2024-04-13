#include <iostream>
#include <coutils.hpp>

struct compound { int a, b, c; };

coutils::async_fn<compound> make_compound() {
    co_return coutils::inituple(1, 2, 3);
}

coutils::async_fn<void> test() {
    auto val = co_await make_compound();
    std::cout << val.a << ' ' << val.b << ' ' << val.c << std::endl;
}

int main() {
    coutils::sync::run_join(test());
}
