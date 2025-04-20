#include <regex.h>
#include <stdio.h>

int main()
{
    regex_t regex;
    regmatch_t matches[1];
    /**
     * 正则表达式解释
     * ^ 匹配字符串的开头
     * [a-zA-Z] 匹配任意一个字母
     * + 匹配前面的元素一次或多次
     * $ 匹配字符串的结尾
    */
    // 综合上面的解释，这个正则表达式用于匹配只包含字母的字符串
    const char* pattern = "^[a-zA-Z]+$";
    const char* string = "HelloWorld";

    // 编译正则表达式
    if(regcomp(&regex, pattern, REG_EXTENDED) != 0) 
    {
        printf("Failed to compile regex.\n");
        return 1;
    }

    // 执行正则表达式匹配
    if(regexec(&regex, string, 1, matches, 0) == 0)
    {
        printf("Match found\n");
    }
    else
    {
        printf("No match found\n");
    }

    // 释放编译后的正则表达式
    regfree(&regex);

    return 0;
}