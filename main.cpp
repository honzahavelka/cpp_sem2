#include <iostream>
#include "mpint/mpint.h"

int main(int argc, const char **argv) {
    MPInt<1> a;
    a = "-10";

    MPInt<1> b;
    b = "2";

    MPInt<2> c;
    c = "2";

    a += c;
    auto d = a - b;

    std::cout << a <<std::endl;
    std::cout << b << std::endl;
    std::cout << c << std::endl;
    std::cout << d << std::endl;

    return 0;
}