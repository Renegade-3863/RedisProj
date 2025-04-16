#include <iostream>
#include <stdlib.h>
#include <string>

int sprintf(char* str, const char* format, ...) __attribute__((deprecated("Please use snprintf instead!")));

double sprintf(std::string str, const std::string format, ...)
{
    str = format;
    return str.size();
}

int main()
{
    std::cout << "Testing attribute deprecation..." << std::endl;
    std::cout << "Using sprintf..." << std::endl;
    // 这里我们尝试使用 sprintf 函数
    char str[100];
    sprintf(str, "Hello, world!");
    std::cout << str << std::endl;
    return 0;
}