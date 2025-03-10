#include <iostream>
#include <coutils.hpp>

struct compound { int a, b, c; };

struct heavy {
    int a, b, c;
    heavy(int m, int n, int p): a(m), b(n), c(p) {}
    heavy(const heavy&) = delete;
    heavy(heavy&&) = delete;
};

coutils::async_fn<compound> make_compound() {
    co_return coutils::co_result(1, 2, 3);
}

#if 0
coutils::async_fn<heavy> make_heavy() {
    co_return coutils::co_result(1, 2, 3);
}
#endif

coutils::async_fn<void> test() {
    {
        // use a normal value normally
        auto val = co_await make_compound();
        std::cout << "Result of make_compound: "
            << val.a << ' ' << val.b << ' ' << val.c
        << std::endl;
    }
#if 0
    {
        // non-recontructible values are not supported
        auto val = co_await make_heavy();
        std::cout << "Result of make_heavy: "
            << val->a << ' ' << val->b << ' ' << val->c
        << std::endl;
    }
#endif
}

int main() {
    coutils::wait(test());
}
