#include <iostream>
#include <limits>

int main()
{
    std::cout << "max_align_t(std::max_align_t): " << alignof(std::max_align_t) << std::endl; 
    return 0;
}