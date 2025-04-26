#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int testPtr2Ptr(char* err, int* len, int** p2p)
{
    // 现在我们做出这样的假设：
    // 我们在这个函数内，需要对 len 指向的内容进行修改
    // 假设我们这样调用这个函数
    // testPtr2Ptr(e, &l, &p);
    // 那么我们如果在这个函数中为 len 重新分配内存（使用 malloc）
    // 那么是没法达到修改 l 的目的的
    // 因为 len 在函数内部只是一个指向 l 的指针
    // 所以，如果要实现对外部变量的内存重分配，就需要像 p2p 这样，传入一个指针的指针
    if(len != NULL)
    {
        len = (int*)malloc(sizeof(int));
        *len = 10;
        printf("*len = %d\n", *len);
    }
    if(p2p != NULL)
    {
        *p2p = (int*)malloc(sizeof(int));
        **p2p = 10;
        printf("**p2p = %d\n", **p2p);
    }
    return 0;
}

int main()
{
    int l = 0;
    int *p = (int*)malloc(sizeof(int));
    
    *p = 1;
    char* e;
    /* 测试无法通过为 len 重新分配内存来改变 l */
    testPtr2Ptr(e, &l, NULL);
    printf("l = %d\n", l);      // l 的值依旧是 0
    testPtr2Ptr(e, NULL, &p);
    printf("*p = %d\n", *p);    // p 的值被修改为 10
    return 0;
}