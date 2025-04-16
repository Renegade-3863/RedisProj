#include <iostream>
#include <iomanip>

int main()
{
    int a = 0;
    std::cout << "Address of a: 0x" << std::setw(16) << std::setfill('0') << reinterpret_cast<uintptr_t>(&a)  << std::endl; 
    return 0;
}