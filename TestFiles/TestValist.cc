#include <stdarg.h>
#include <stdio.h>
#include <iostream>

using namespace std;

void my_printf(const char* fmt, ...)
{
    // va_list 全名是 variable argument list
    // 它将被用来保存函数参数的指针，根据函数参数的类型，依次取出参数
    va_list args;
    // va_start 用来初始化 args，把 args 指向第一个可变参数的位置
    // 如果你的代码编辑器有智能提示，你可以把鼠标移到 va_start 上，查看它的定义
    // 你会发现 va_start 本身并不是一个函数，而是一个宏
    // 它是 __builtin_va_start 的别名
    // __builtin_va_start 是 GCC 内置的一个宏，它的定义在 GCC 的源码中
    va_start(args, fmt);

    // 现在 args 对象中就保存了 fmt 之后的所有参数
    while(*fmt) 
    {
        // 如果当前字符是 %，则取出下一个参数
        if(*fmt == '%' && *(fmt+1))
        {
            ++fmt;
            // 根据标签决定取出的参数类型
            switch (*fmt) {
                case 'd': {
                    // 代表我们要取出的是一个 int 类型的参数
                    int i = va_arg(args, int);
                    // 使用 putchar 函数进行输出
                    putchar(i + '0');
                    break;
                }
                case 'c': {
                    // 代表我们要取出的是一个 char 类型的参数
                    char c = (char)va_arg(args, int);
                    putchar(c);
                    break;
                }
                case 's': {
                    char* s = va_arg(args, char*);
                    for(; *s; ++s)
                    {
                        putchar(*s);
                    }
                    break;
                }
                case 'f': {
                    double d = va_arg(args, double);
                    // 获取 double 类型变量的整数部分和小数部分
                    int int_part = (int)d;
                    double frac_part = d-int_part;

                    
                    if(int_part == 0)
                    {
                        putchar('0');
                    }
                    else
                    {
                        char buffer[20];
                        int i = 0;
                        // buffer 是用于输出结果的缓存
                        while(int_part > 0)
                        {
                            buffer[i++] = (char)(int_part % 10) + '0';
                            int_part /= 10;
                        }
                        // 整数部分现在保存在了 buffer 中，我们逆向输出即可
                        for(int j = i-1; j >= 0; --j)
                        {
                            putchar(buffer[j]);
                        }
                    }
                    // 输出小数点
                    putchar('.');

                    // 处理小数部分
                    frac_part *= 1000000; // 我们保留六位小数
                    int frac_int = (int)frac_part;
                    if(frac_int == 0) {
                        putchar('0');
                    } else {
                        char buffer[20];
                        int i = 0;
                        while (frac_int > 0) {
                            buffer[i++] = (frac_int % 10) + '0';
                            frac_int /= 10;
                        }
                        for(int j = i-1; j >= 0; j--)
                        {
                            putchar(buffer[j]);
                        }
                    }
                }
                default:
                    break;
            }
        }
        // 如果当前字符不是 %，则直接输出
        else
        {
            putchar(*fmt);
        }
        ++fmt;
    }
    // 最后不要忘记调用 va_end 宏关闭 args
    va_end(args);
    return;
}

int main()
{
    double value = 123.456789;
    my_printf("%f", value);
    return 0;
}