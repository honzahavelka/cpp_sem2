#include <iostream>
#include "mpint/mpint.h"

int main(int argc, const char **argv) {

    MPInt<1> a;
    a = "5";

    MPInt<MPInt<0>::Unlimited> b;
    b = "5";

    auto c = a + b;
    std::cout << a <<std::endl;
    std::cout << b << std::endl;
    std::cout << c << std::endl;
    return 0;
}