#include <iostream>
#include "mpint/mpint.h"

int main(int argc, const char **argv) {
    MPInt<1> a;
    a = "-10";

    MPInt<1> b;
    b = "25";

    MPInt<2> c;
    c = "2";


    std::cout << a <<std::endl;
    std::cout << b << std::endl;
    std::cout << c << std::endl;

    auto e = a * b;
    std::cout << e << std::endl;

    return 0;
}