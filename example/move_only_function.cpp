#include "move_only_function.hpp"
#include <iostream>

int main() {
    move_only_function<void()> f = [] { std::cout << "Hi\n"; };
    f();
}
