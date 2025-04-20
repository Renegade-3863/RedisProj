# Redis 的基本组件之一：SDS (Simple Dynamic String)
#### 定义在 sds.h 和 sds.c 文件中

-- -
- 我们先看 sds.h 中的函数声明
    ### 首先，是很多和下面结构类似的结构体定义
    ``` C
    struct __attribute__ ((__packed__)) sdshdr5 {
        unsigned char flags; /* 3 lsb of type, and 5 msb of string length */
        char buf[];
    };
    ```
    - 这个结构体定义了一个字符串头部，它包含了一个标志位和一个缓冲区
    - 一个新奇的点在于这个 **__attribute_\_ ((__packed_\_))** 编译指令
        - 一言以蔽之，这个编译指令告诉编译器，不要对这个结构体进行字节对齐
        - 因为这个结构体只是一个头部，它的大小是固定的，不需要进行字节对齐，而内存对齐会浪费空间
        - 有关使用这个编译指令的代码，详见 [packedAttr.cc](../TestFiles/packedAttr.cc)
    - Redis 定义了 5 种不同的头部信息结构体，按大小分别为 5、8、16、32、64 字节
    - 这里的官方注释：3 lsb of type, 5 unused bits，意思是：
        - 头部的前 3 位用来表示字符串的类型
        - 头部的后 5 位未使用

    ### 另一个新颖的语法是下面这一行宏定义：
    ``` C
    #define SDS_HDR_VAR(T, s) struct sdshdr##T *sh = (void*)((s)-(sizeof(struct sdshdr##T)));
    ```
    - 一个很有趣，并且个人认为很有用的预处理语法：
        - sdshdr##T
            - 这个语法事用来在预处理阶段对字符串进行拼接的
            - 它会把 sdshdr 和 T 进行**拼接**，生成上面提到的那 5 种结构体名称的其中一种，<u>这取决于你 T 用什么值来调用</u>
    - 学会了这个用法，我们就能直观理解这句宏替换在干嘛了
        - 本质上，它相当于在当前 scope 下定义了一个指向 sdshdr##T 类型对象的指针 sh，后续在本 scope 中都可以使用这个 sh 来执行一些操作了
        - 而 s 是原本的指向字符串本体的开头位置的指针，我们对它进行指针运算，往前挪动到对应的 header 信息开头处，就拿到了这个字符串的头信息结构体 （逻辑上，这两份数据是存储在一起的）
    ### 还有一个有趣的指针运算是下面这一行宏定义：
    ``` C
    #define SDS_TYPE_5_LEN(f) ((f)>>SDS_TYPE_BITS)
    ```
    - 回顾上面有关 sdshdr5 结构体的定义，我们不难发现，它的低 3 位存储的是这个字符串的类型信息 （一共就 8 位）
    - 而高 5 位存储的是它的长度信息
    - 所以这里我们传入 f，也就是这个结构体的开头指针，经过这个宏的处理，我们就拿到了它后面字符串的实际长度了
    ### 为什么 Redis 要特意定义这样一种很特殊的，并没有在内部单独保存长度的 sdshdr5 结构体？
    - 个人目前只能想到是为了精简一些很简短的数据项的表示，从而 <u>最大限度地减少存储字符串元信息的内存开销</u>

-- -
### 之后，是一组内联的功能函数定义
- 在看这部分之前，记住这句话：
``` C
typedef char* sds;
```
- 说明 sds 实际上就是字符指针的假名
- 我们简单看两个结构体定义，剩下的功能就简单列出
``` C
static inline size_t sdslen(const sds s) {
    unsigned char flags = s[-1];
    switch(flags&SDS_TYPE_MASK) {
        case SDS_TYPE_5:
            return SDS_TYPE_5_LEN(flags);
        case SDS_TYPE_8:
            return SDS_HDR(8,s)->len;
        case SDS_TYPE_16:
            return SDS_HDR(16,s)->len;
        case SDS_TYPE_32:
            return SDS_HDR(32,s)->len;
        case SDS_TYPE_64:
            return SDS_HDR(64,s)->len;
    }
    return 0;
}
```
- 上面这个函数用于获取 sds 代表的字符串对象 s （实际上是 char 指针指向的地址）内部存储的字符串长度 （注意，这个 s 是实际的字符串的开始地址，不是元信息的开始地址）
- s[-1] 执行指针运算，把对应的指针挪动到 flags 的开头
- **flags** 本身对应的是上面 5 种字符串 header 结构体中的 flags 对象，使用 SDS_TYPE_MASK 掩码进行处理后，它就可以获取到这个字节中低 3 位存储的类型信息了
- switch 语句根据 header 中保存的类型信息，执行对应的长度获取方法

``` C
static inline size_t sdsavail(const sds s)
{
    unsigned char flags = s[-1];
    switch(flags&SDS_TYPE_MASK) {
        case SDS_TYPE_5: {
            return 0;
        }
        case SDS_TYPE_8: {
            SDS_HDR_VAR(8, s);
            return sh->alloc - sh->len;
        }
        case SDS_TYPE_16: {
            SDS_HDR_VAR(16, s);
            return sh->alloc - sh->len;
        }
        case SDS_TYPE_32: {
            SDS_HDR_VAR(32, s);
            return sh->alloc - sh->len;
        }
        case SDS_TYPE_64: {
            SDS_HDR_VAR(64, s);
            return sh->alloc - sh->len;
        }
    }
    return 0;
}
``` 
- 有一定代码阅读能力的话，这段代码就不难看懂了
- 本质上是计算了 s 对应的 sds 对象的剩余可用空间大小

- 这里有很多结构类似的字符串处理函数，就不一一列举了
- 最后看一下这个位运算的部分：
``` C
static inline void sdsinclen(sds s, size_t inc) {
    unsigned char flags = s[-1];
    switch(flags&SDS_TYPE_MASK) {
        case SDS_TYPE_5:
            {
                // 注意一点就行：指针运算，移动的单位是指针指向的对象的大小，而不是字节
                // 这一步用于获取的依然是 sds 头部的指针，同时也是 flags 的指针
                unsigned char* fp = ((unsigned char*)s)-1;
                // 回忆上面对于 SDS_TYPE_5_LEN 的定义，它的总长度就一个字节，高 5 位是长度信息，低 3 位是 flags 类型信息
                unsigned char newlen = SDS_TYPE_5_LEN(flags)+inc;
                // 把 newlen 用按位与的方式赋值给 fp 指向的对象
                *fp = SDS_TYPE_5 | (newlen << SDS_TYPE_BITS);
            }
    }
}
```
- 提一点：个人认为，上面这里的 SDS_TYPE_5_LEN(flags)+inc 有点不安全，因为如果 inc 是一个很大的数，那么就会导致溢出，或者说高 5 位不足以存储更新后的值

** *
## 补充一个个人认为挺有必要的知识点：
- 有关 C 语言的参数列表 va_list, va_start, va_end, va_arg 的使用
- 这里简单总结一下：
    - va_list 是一个指向参数列表的指针
    - va_start 用于初始化这个指针，它的第一个参数是 va_list 类型的指针，第二个参数是可变列表前的最后一个固定参数
        - 注意，这里的最后一个固定参数是指，在可变参数列表之前的最后一个参数，而不是可变参数列表的最后一个参数
    - va_end 用于清理这个指针
    - va_arg 用于获取下一个参数，它的第一个参数是 va_list 类型的指针，第二个参数是参数的类型
- 测试代码详见 [TestValist.cc](../TestFiles/TestValist.cc)

### ### 编写文档时使用：回到 [RedisRio.md](./RedisRio.md)