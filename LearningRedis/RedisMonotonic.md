# Redis monitonic.h 文件解析

## 根头文件之一，定义了一组和时间相关的函数

### 前置库：
- [fmacros.h](./RedisFmacros.md)

-- - 
### 先来看一下作者的注释：
``` C
/* 单调时钟是一种始终递增的时钟源。它与实际的时间无关，只应用于相对计时。
 * 单调时钟也不保证是精确的时间顺序；可能会有轻微的偏差/漂移。
 *
 * 根据系统架构的不同，单调时间的获取速度可能比普通时钟源快得多，
 * 这是通过使用 CPU 上的指令计数器实现的。例如，在 x86 架构上，
 * RDTSC 指令就是一个非常快速的时钟源。
 */
```
- 个人不是很理解这种单调时钟的定义：
    - 但是功能上来讲，可以认为它是一种只记录时间差的时钟源
    - 而且个人理解，这种细枝末节的模块，只需要明白它是用来获取时间的即可，实际开发中，这种细节一般不会有太大影响

- 比较重要的函数大致有两个，这里简单列明，具体实现在后面讲到 monotonic.c 的时候再说：
``` C
// 这是一个函数指针，指向一个声明为外部定义的函数 getMonotonicUs，返回值为 monotime (源码中为 uint64_t 类型的别名)
// 用于获取当前的单调时间值 （Us 代表单位为微妙）
extern monotime (*getMonotonicUs)(void);

// 用于初始化单调时钟的函数，检查过头文件后，作者发现实际上这里并没有具体的 "单调时钟" 对象，整个时钟信息的获取都是通过 getMonotonicUs 函数实现的
const char* monotonicInit(void);
```

# Redis monotonic.c 文件解析
## 是上面 monotonic.h 文件的具体实现

### 前置库：
- [monotonic.h](./RedisMonotonic.md)
-- -
### 一个新认识的 C 库：
- regex.h
    - 这个库是 C 语言中的正则表达式库，用于处理正则表达式的匹配和替换等操作
    - 全称为 regular expression
    - 详细使用案例参考对应的 test 文件：
        - [TestRegex.cc](../TestFiles/TestRegex.cc)

### 本文件实现了某种意义上的多态 （C++ 中，多态是用虚函数来实现的）
- 由于 Redis 源码由 C 语言写就，所以作者应用了传统的 C 语言函数指针封装的方法来实现多态
- 首先声明一个（不一定非要是静态，具体看需求）函数指针：
``` C
// 默认为 NULL，具体指向的函数地址视操作系统及 CPU 架构而定
// 这也是 C 多态模拟的基础
monotime (*getMonotonicUs)(void) = NULL;
```

- 之后，根据操作系统架构的不同，定义不同的时间获取函数，让这个上层的 getMonotonicUs 函数指针指向不同的函数，这就是最经典的封装实现了
-- -
``` C
static monotime getMonotonicUs_x86(void)
{
    // 使用 __rdtsc 函数，读取 x86 架构的 CPU 上的 Time Stamp Counter，也就是时间戳计数器的值
    // 这个 64 位寄存器记录了从处理器启动以来的时钟周期数
    // 这个值除以 CPU 的时钟频率，就可以得到当前的时间值（单位为微秒）
    return __rdtsc() / mono_ticksPerMicroSecond;
}
```
- 上面获取时间的函数再次印证了 “单调” 的概念
- __rdtsc() 获取的值只会递增，不会回退，这便是 “单调性”

-- -
- Redis 分别为 x86 的 CPU 和 ARM 架构的 CPU 定义了两组不同的时间获取函数，这里我们重点讨论一下 x86 的 CPU

``` C
static void monotomicInit_x86linux(void)
{
    const int bufflen = 256;
    char buf[bufflen];
    // 不熟悉正则表达式的可以往上翻，有对应的部分讲解
    // 这里 cpiGhzRegex 用于匹配 CPU 的频率（Ghz 说明了这一点）
    // constTscRegex 用于匹配 CPU 的 Time Stamp Counter 的值
    reget_t cpuGhzRegex, constTscRegex;
    const size_t nmatch = 2;
    regmatch_t pmatch[nmatch];
    int constantTsc = 0;
    int rc;

    // 编译正则表达式，这个表达式的含义是：
    // 1. 开头必须为 "model name"
    // 2. "+" 代表匹配一个或多个 '\s' 字符，也就是空白字符
    // 3. ":" 匹配一个冒号
    // 4. ".*" 匹配任意数量的任意字符
    // 5. "@" 匹配一个 "@" 符号
    // 6. "([0-9.]+)" 需要递归分析
    // 6.1 "[0-9.]" 代表匹配一个数字/小数点
    // 6.2 "[0-9.]+" 代表匹配一个或多个数字/小数点
    // 7. "GHz" 匹配 "GHz" 字符串
    // 综上，这个正则表达式的作用是匹配 CPU 的型号和频率
    // 例如："model name : Intel(R) Core(TM) i7-10700 CPU @ 2.90GHz" 就会匹配到 "2.90"
    rc = regcomp(&cpuGhzRegex, "^model name\\s+:.*@ ([0-9.]+)GHz", REG_EXTENDED);
    // 这里的 assert 是 Redis 自己实现的断言函数，用于在调试时检查条件是否满足
    assert(rc == 0);
    
    rc = regcomp(&constTscRegex, "^flags\\s+:.* constant_tsc", REG_EXTENDED);

    // 此时两个正则表达式的匹配信息都处理完成了
    // FILE* 是 C 语言中的文件指针，用于指向一个文件
    // 这里 cpuinfo 指针指向打开的 cpuinfo 文件，以只读模式打开
    FILE* cpuinfo = fopen("/proc/cpuinfo", "r");
    // 开始逐行读取 cpuinfo 文件的内容
    if(cpuinfo != NULL)
    {
        // fgets 函数用于从文件中读取一行内容，返回值为指向该行内容开头的指针
        while(fgets(buf, bufflen, cpuinfo) != NULL)
        {
            // 代码后详解一下这里的 nmatch 和 pmatch，有点难理解
            if(regexec(&cpuGhzRegex, buf, nmatch, pmatch, 0) == 0)
            {
                buf[pmatch[1].rm_eo] = '\0';
                // atof 是 argument to float 的缩写，用于将字符串转换为浮点数
                // 这里 pmatch[1].rm_so 存储的是上文 GHz 前的数字的起始位置，在 buf 中索引即可定位到这个数字开头的字符
                // 再取地址给 atof 即可
                double ghz = atof(&buf[pmatch[1].rm_so]);
                // 1 GHz = 1000000000 ticks / second
                // 也就是 1000000000000 ticks / microsecond
                // 所以要乘 1000
                mono_tickPerMicrosecond = (long)(ghz * 1000);
                break;
            }
        }
        while(fgets(buf, bufflen, cpuinfo) != NULL)
        {
            if(regexec(&constTscRegex, buf, nmatch, pmatch, 0) == 0)
            {
                constantTsc = 1;
                break;
            }
        }

        fclose(cpuinfo);
    }
    regfree(&cpuGhzRegex);
    regfree(&constTscRegex);

    if(mono_ticksPerMicrosecond == 0)
    {
        fprintf(stderr, "monotonic: x86 linux, unable to determine clock rate");
        break;
    }
    if(!constantTsc)
    {
        fprintf(stderr, "monotonic: x86 linux, 'constant_tsc' flag not present");
        return;
    }

    snprintf(monotonic_info_string, sizeof(monotonic_info_string), "X86 TSC @ %ld ticks/us", mono_ticksPerMicrosecond);
    // 定位封装的函数指针
    getMonotonicUs = getMonotonicUs_x86;
}
```
### 编写文档时使用：回到 [RedisAe.md](./RedisAe.md)