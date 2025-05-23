# Redis config.h 以及 config.c 文件分析
#### 这个文件定义了很多 redis 运行时用到的宏选项，它们用来决定 Redis 进程要用哪些函数来实现具体功能，因为不同平台对相同功能的实现可能不同
- 举一个比较有东西可讲的例子：
``` C
/* Test for proc filesyetem */
#ifdef __linux__
#define HAVE_PROC_STAT 1
#define HAVE_PROC_MAPS 1
#define HAVE_PROC_SMAPS 1
#define HAVE_PROC_SOMAXCONN 1
#define HAVE_PROC_OOM_SCORE_ADJ 1
#define HAVE_EVENT_FD 1
#endif
```
- 上面这些定义，如 redis 官方注释所说，是用来访问 linux 自带的 proc 日志信息的
- 列出一些常见的 /proc 文件：
    - /proc/cpuinfo 
        - CPU 信息，包括 CPU 型号、缓存大小、频率等
    - /proc/meminfo
        - 内存信息，包括内存总量、可用内存、缓存内存等
    - /proc/uptime
        - 系统运行时间，包括系统启动时间和当前时间
    - /proc/stat
        - 系统统计信息，包括 CPU 使用率、内存使用率等
    - /proc/net/dev
        - 网络设备信息，包括网络设备名称、接收字节数、发送字节数等
    - /proc/swaps
        - 交换分区信息，包括交换分区名称、交换分区大小、交换分区使用情况等
    - /proc/[pid]/oom_score_adj
        - 进程 OOM 分数调整值，用于调整进程的 OOM 优先级，也就是，当内存不足时，被 kill 的进程的优先级
    - /proc/sys/net/core/somaxconn (socket max connection)
        - 这个文件定义了一个整数值，表示系统中每个监听套接字允许的最大连接队列长度（回忆 socket 编程，这里的连接队列长度指的是 <u>**全连接队列**</u> 的长度，也就是 <u>accept</u> 函数对应的队列长度）

-- -

## 另一个想讲的点：大小端序

``` C
#if defined(linux) || defined(__linux__)
#include <endian.h>
#else 
// 小端序，一般的现代计算机都是基于小端字节序的
#define LITTLE_ENDIAN 1234 /* least-significant byte first (vax, pc) */
// 大端序
#define BIG_ENDIAN 4321 /* most-significant byte first (IBM, net) */
// 这个 pdp 计算机并不常用，这里不展开讲
#define PDP_ENDIAN 3412 /* LSB first in word, MSW first in long (pdp) */
```
- 这里想重点讲一下大小端序的意义
    - 计算机存储数据都是按位进行的，但是读取数据的单位并不是单个的位
    - 那么计算机如何读取一个字节这一点就很有关注的必要了
    - 假设有一串二进制位：
        - 0x0123456788765432 (64 位，每个数代表一个 4 位的字)
        - 我们假设这个数据保存在计算机一个内存地址 <u>**0x0000006133197512**</u> 处（笔者计算机是 Arm64 架构，所以这里读出来是一个 64 位的地址值
        - 往下看之前，先明确一点：<u>计算机读取地址时，是从低位到高位进行读取的</u>
        - 思考一下，计算机怎么存储这些数据？
            - 可能类似这种：
                ``` txt
                0x0000006133197512: 0x01
                0x0000006133197513: 0x23
                0x0000006133197514: 0x45
                0x0000006133197515: 0x67
                0x0000006133197516: 0x88
                0x0000006133197517: 0x76
                0x0000006133197518: 0x54
                0x0000006133197519: 0x32
                ```
                - 上面这种存储方式，是把人类看来，数值的高位字节，保存到内存地址的低地址处，数值的低位字节，保存到内存地址的高地址处
                - 这样计算机读取的时候，读到的内容就和我们人类看到的一样了
                - 这种符合正常人类认知的存储方式，就是大端序
            - 反之，也可能是这种：
                ``` txt
                0x0000006133197512: 0x32
                0x0000006133197513: 0x54
                0x0000006133197514: 0x76
                0x0000006133197515: 0x88
                0x0000006133197516: 0x67
                0x0000006133197517: 0x45
                0x0000006133197518: 0x23
                0x0000006133197519: 0x01
                ```
                - 这样读出来的结果是 0x3254768867452301，和我们写在纸上/代码里的内容正好是相反的
                - 这种反人类认知的存储方式，就是小端序

-- -

# config.c 
- 真的看了源码后发现，config.c 和 config.h 两个文件压根不是同一个逻辑结构下的，config.h 是配置系统相关宏的文件，而 config.c 是用来读取 redis.conf 文件的功能实现文件
- 那么这个文件等我们看完了前置代码块后再来写讲解（）
- 基本上可以认为这份文件是和 redis.conf 文件共同工作，来修改 redis 进程的运行时行为的


### 编写文档时使用：回到 [RedisServer.md](./RedisServer.md)
