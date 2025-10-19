#include <iostream>
#include "mpint/mpint.h"

int main(int argc, const char **argv) {

    MPInt<1> a;
    a = "-10";
    MPInt<2> b;
    b = "-219";

    int x = -10;
    int y = -219;
    int z = y / x;

    auto c = b / a;

    b %= a;
    y %= x;
    std::cout << b << std::endl;
    std::cout << y << std::endl;
    std::cout << c << std::endl;
    std::cout << z << std::endl;


    MPInt<MPInt<1>::Unlimited> d;
    d = "1 000 000 000 000 000 000";


    return 0;
}