#include <iostream>

struct test
{
    unsigned char a;
    unsigned int b;
    char buf[];
};

struct __attribute__ ((__packed__)) testPacked
{
    unsigned char a;
    unsigned int b;
    char buf[];
};

int main()
{
    std::cout << "sizeof(test): " << sizeof(test) << std::endl; // 应该输出 8，考虑内存对齐
    std::cout << "sizeof(testPacked): " << sizeof(testPacked) << std::endl; // 应该输出 5，不考虑内存对齐
    return 0;
}