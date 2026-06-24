#include <iostream>
#include <format>


int main()
{
    std::cout << "Hello, World!" << std::endl;
    std::cout << std::format("{}", ~5 + 1) << std::endl;
    std::cout << std::format("{}", ~-5 + 1) << std::endl;

    std::cout << (-1 >> 2) << std::endl;
    int a = -18;
    std::cout << (a >> 2) << std::endl;
    int b = 18;
    std::cout << (b >> 2) << std::endl;
    return 0;
}
