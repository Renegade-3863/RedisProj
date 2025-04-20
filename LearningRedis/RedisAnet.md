# Redis anet.h 文件解析

### 这是一个定义了一组和 TCP 套接字通信相关的封装函数的头文件
### 同时也是 Redis 根头文件之一

- 先简单分析一下宏定义：
``` C
// 一组宏定义，用于表示不同的套接字状态
#define ANET_OK 0 // 成功码
#define ANET_ERR -1 // 错误码
#define ANET_ERR_LEN 256 // 错误信息的最大长度

/* Flags used with certain functions */
#define ANET_NONE 0 // 无标志
#define ANET_IP_ONLY (1<<0) // 只使用 IP 地址
#define ANET_PREFER_IPV4 (1<<1) // 优先使用 IPv4 地址
#define ANET_PREFER_IPV6 (1<<2) // 优先使用 IPv6 地址

// __AIX 是 AIX 操作系统的宏定义
// __sun 是 Sun Solaris 操作系统的宏定义
// 这两个宏定义用于判断当前操作系统是否是 AIX 或 Sun Solaris 操作系统
// 如果是，就将 AF_LOCAL 宏定义为 AF_UNIX
// 在不同的操作系统上，AF_LOCAL 的含义可能不同
#if defined(__sun) || defined(__AIX)
#define AF_LOCAL AF_UNIX
#endif

#ifdef _AIX
#undef ip_len
#endif
```

- 函数声明这里不多写，直接到 anet.c 文件中看即可
-- -
# Redis anet.c 文件解析
### 这是一个定义了一组和 TCP 套接字通信相关的函数的 C 文件

### 前置库：
- [anet.h](./RedisAnet.md)
- [config.h](./RedisConfig.md)
- [util.h](./RedisUtil.md)

-- -
### 如果要学习网络编程，那么这个函数库是很好的材料
#### 我们重点研究一下这份文件
``` C
// 用于格式化并打印错误信息的函数
static void anetSetError(char* err, const char* fmt, ...)
{
    va_list ap;

    if(!err) return;
    va_start(ap, fmt);
    // 注释：vsnprintf 的参数列表：
    // - err：指向一个字符数组的指针，用于存储格式化后的字符串
    // - ANET_ERR_LEN：err 数组的最大长度
    // - fmt：格式化字符串，用于指定输出的格式
    // - ap：可变参数列表，用于提供格式化字符串中的参数
    vsnprintf(err, ANET_ERR_LEN, fmt, ap);
    va_end(ap);
}
```
``` C
// 用于获取一个指定文件描述符 (通常是套接字) 的错误状态
int anetGetError(int fd)
{
    // sockerr 代表错误码 
    int sockerr = 0;

    socklen_t errlen = sizeof(sockerr);

    // SOL_SOCKET 全名为 Socket Option Level，是套接字选项的层级
    // SO_ERROR 是一个套接字选项，用于获取套接字的错误状态
    // 这个函数的作用是获取套接字 fd 的错误状态，并将其存储在 sockerr 中
    // 如果获取失败，就将 errno 的值存储在 sockerr 中
    if(getsockopt(fd, SOL_SOCKET, SO_ERROR, &sockerr, &errlen) == -1)
        sockerr = errno;
    return sockerr;
}
```

``` C
int anetSetBlock(char* err, int fd, int non_block)
{
    int flags;
    // 两个参数的 fcntl 函数用于获取 fd 的状态
    if((flags = fcntl(fd, F_GETFL)) == -1)
    {
        anetSetError(err, "fcntl(F_GETFL): %s", sterror(errno));
        return ANET_ERR;
    }

    // 如果已经设置了非阻塞模式，就直接返回，不再重复设置
    if(!!(flags & O_NONBLOCK) == !!non_block)
        return ANET_OK;
    // 如果需要设置为非阻塞模式，就将 O_NONBLOCK 选项添加到 flags 中
    if(non_block)
        flags |= O_NONBLOCK;
    else
        flags &= ~O_NONBLOCK;
    
    // fcntl 函数用于设置文件描述符的选项，这里用它来设置文件描述符 fd 的阻塞/非阻塞模式
    if(fcntl(fd, F_SETFL, flags) == -1)
    {
        anetSetError(err, "fcntl(F_SETFL, O_NONBLOCK): %s", strerror(errno));
        return ANET_ERR;
    }
    return ANET_OK;
}

int anetNonBlock(char* err, int fd)
{
    return anetSetBlock(err, fd, 1);
}

int anetBlock(char* err, int fd)
{
    return anetSetBlock(err, fd, 0);
}
```

``` C
int anetCloexec(int fd)
{
    int r;
    int flags;

    // 引入 do while 循环，用于处理被信号中断的情况
    // 因为 fcntl 是一个系统调用，它可能会被信号中断
    // 如果被信号中断，就会返回 -1，并将 errno 设置为 EINTR
    // EINTR 为 Error Interrupt 
    do
    {
        // 获取文件描述符的状态
        r = fcntl(fd, F_GETFD);
    } while(r == -1 && errno == EINTR);

    // 如果已经设置了这个标记位，或者获取出错，就直接返回
    if(r == -1 || (r & FD_CLOEXEC))
        return r;
    
    // 掩码运算
    flags = r | FD_CLOEXEC;

    // 设置文件描述符的状态
    // do while 包裹，防系统信号中断
    do
    {
        r = fcntl(fd, F_SETFD, flags);
    } while(r == -1 && errno == EINTR);

    return r;
}
```
- 提问：这个 FD_CLOEXEC 是什么？
    - FD 代表 file descriptor
    - CLOEXEC 代表 close on exec，表示在执行 exec 系统调用时，在 exec 创建的新进程中，关闭这个（无用的）文件描述符
    - 应用场景如下：
        - 当一个进程调用 fork 函数创建一个子进程时，子进程会继承父进程的文件描述符（通常以写时复制的，COW 的方式进行）
        - 起初，基于 COW 机制，子进程和父进程共享同一个文件描述符
        - 而当子进程执行 exec 系统调用时，子进程会替换自己的代码段和数据段，此时这个子进程再保持打开父进程的文件描述符就没有用了
        - 所以这里的 close on exec 标记就是为了避免这种打开的文件描述符泄漏问题
    - 代码验证参考：
        - [TestFdCloexec.cc](../TestFiles/TestFdCloexec.cc)
    - 通过这样的方式，程序就避免了文件描述符泄漏的问题

``` C
// 设定 fd 这个文件描述符（通常为套接字）的 keepalive 状态，keep alive for interval
int anetKeepAlive(char* err, int fd, int interval)
{
    int enabled = 1;
    // setsockopt 配合 SOL_SOCKET 参数，用于修改套接字描述符选项，这里是修改为 SO_KEEPALIVE，也就是保持持续连接状态
    if(setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &enabled, sizeof(enabled)))
    {
        anetSetError(err, "setsockopt SO_KEEPALIVE: %s", strerror(errno));
        return ANET_ERR;
    }

    int idle;
    int intvl;
    int cnt;

// 这里的 UNUSED 宏是 (void)(x) 的别名，用于标记一个参数未使用，避免编译器警告
#if !(defined(__AIX) || defined(__APPLE__) || defined(__DragonFly__) || \
defined(__FreeBSD__) || defined(__illumos__) || defined(__linux__) \
defined(__netBSD__) || defined(__sun))
    UNUSED(interval);
    UNUSED(idle);
    UNUSED(intvl);
    UNUSED(cnt);
#endif

}
```
- 之后的一大段都是针对 Solaris 系统的具体适配代码，因为 Solaris 系统现在已经不常用，这里不再详细分析
- 我们跳到下一个 TCP_KEEPIDLE 定义这里：
``` C
#ifdef TCP_KEEPIDLE
    /**
     * Default settings are more or less garbage, with the keepalive time
     * set to 7200 by default on Linux and other Unix-like systems.
     * Modify settings to make the feature actually useful
     * */
    /* Send first probe after interval. */
    idle = interval;
    // 这里的 IPPROTO_TCP 代表修改的选项是 TCP 协议层级的选项
    if(setsockopt(fd, IPPROTO_TCP, TCP_KEEPIDLE, &idle, sizeof(idle)))
    {
        anetSetError(err, "setsockopt TCP_KEEPIDLE: %s\n", strerror(errno));
        return ANET_ERR;
    }
#elif defined(TCP_KEEPALIVE)
    /* Darwim/macOS uses TCP_KEEPALIVE in place of TCP_KEEPIDLE */
    idle = interval;
    if(setsockopt(fd, IPPROTO_TCP, TCP_KEEPALIVE, &idle, sizeof(idle)))
    {
        anetSetError(err, "setsockopt TCP_KEEPALIVE: %s\n", strerror(errno));
        return ANET_ERR;
    }
#endif
```
- 补充：上面两段代码，为什么分别在 SOL_SOCKET 层级和 IPPROTO_TCP 层级分别进行 keepalive 设置？
    - SOL_SOCKET 层级的 keepalive 设置代表在所有的 TCP 套接字连接上启用 keepalive 功能，但是并没有指定具体的 keepalive 探测包配置，所以如果不设置具体的 keepalive 探测包配置，就会使用系统默认的配置
    - IPPROTO_TCP 层级的 keepalive 设置指定了具体的 keep alive 时间间隔，也即上文对应的 interval 参数
``` C
#ifdef TCP_KEEPINTVL
    /* Send next probe after the specified interval. Note that we set the 
    * delay as interval / 3, as we send three probes before detecting
    * an error (see the next setsockopt call). */
    intvl = interval/3;
    if(intvl == 0) intvl = 1;
    if(setsockopt(fd, IPPROTO_TCP, TCP_KEEPINTVL, &intvl, sizeof(intvl)))
    {
        anetSetError(err, "setsockopt TCP_KEEPINTVL: %s\n", strerror(errno));
        return ANET_ERR;
    }
#endif
```
- 这里是细分对 keepalive 机制的一些配置进行修改，例如发送下一个探测包的间隔时间 KEEPINTVL 等
- 作者把 intvl 修改为 interval/3，这是因为 TCP 协议规定，在连续重传三条报文后，如果仍然没有收到响应，就会认为连接已经断开
- 所以这里的 intvl 就代表了发送下一个探测包的间隔时间，也就是 interval/3

``` C
#ifdef TCP_KEEPCNT
    /* Consider the socket in error state after we send three ACK probes without getting a reply. */
    cnt = 3;
    if(setsockopt(fd, IPPROTO_TCP, TCP_KEEPCNT, &cnt, sizeof(cnt)))
    {
        anetSetError(err, "setsockopt TCP_KEEPCNT: %s\n", strerror(errno));
        return ANET_ERR;
    }
#endif
```

-- -
### 之后是一组和 IP 地址相关的函数
``` C
int anetResolve(char* err, char* host, char* ipbuf, size_t ipbuf_len, int flags)
{
    /* 这里给出 struct addrinfo 在标准 Posix 库中的定义：*/
    /*
    struct addrinfo 
    {
        // ai 前缀代表 "address information"
        int ai_flags;   // 地址信息的标志
        int ai_family;  // 地址族：AF_INET 代表 IPv4 地址族，AF_INET6 代表 IPv6 地址族
        int ai_socktype; // 套接字类型，例如 SOCK_STREAM 代表 TCP 套接字，SOCK_DGRAM 代表 UDP 套接字
        int ai_protocol; // 协议类型，例如 IPPROTO_TCP 代表 TCP 协议，IPPROTO_UDP 代表 UDP 协议
        size_t ai_addrlen;  // 地址信息的长度
        struct sockaddr* ai_addr;   // 地址信息的指针
        char* ai_canonname; // 规范的主机名
        struct addrinfo* ai_next;  // 指向下一个地址信息的指针
    };
    */
    /* 同时给出 sockaddr 结构体类型的定义 */
    /* 
    struct sockaddr
    {
        sa_family_t sa_family; // 地址族，例如 AF_INET 代表 IPv4 地址族，AF_INET6 代表 IPv6 地址族，sa_family_t 为 unsigned short 类型的别名
        char sa_data[14]; // 地址信息，长度为 14 字节
    };
    大小为 16 字节
    */
    struct addrinfo hints, *info;
    // 一个 random variable
    int rv;

    memset(&hints, 0, sizeof(hints));
    // AI_NUMERICHOST 代表只解析 IP 地址，不解析域名，换句话说，localhost 这种文字表示的域名，就不会被解析了
    if(flags & ANET_IP_ONLY) hints.ai_flags = AI_NUMERICHOST;
    // AF_UNSPEC 代表任意地址族，即 address family unspecified
    hints.ai_family = AF_UNSPEC;
    if(flags & ANET_PREFER_IPV4 && !(flags & ANET_PREFER_IPV6))
    {
        // 如果设定了优先考虑 IPv4 地址，就将 hints.ai_family 设置为 AF_INET
        hints.ai_family = AF_INET;
    } else if(flags & ANET_PREFER_IPV6 && !(flags & ANET_PREFER_IPV4))
    {
        hints.ai_family = AF_INET6;
    }
    // 设定套接字类型为 TCP 流式类型
    hints.ai_socktype = SOCK_STREAM;

    // 注释：getaddrinfo 参数只是提供给 getaddrinfo 函数的一个提示信息（like its name suggested）
    // 如果解析成功，结果会被存储到 info 指针中
    rv = getaddrinfo(host, NULL, &hints, &info);
    if(rv != 0 && hints.ai_family != AF_UNSPEC)
    {
        /* Try the other IP version. */
        hints.ai_family = (hints.ai_family == AF_INET) ? AF_INET6 : AF_INET;
        rv = getaddrinfo(host, NULL, &hints, &info);
    }
    if(rv != 0)
    {
        anetSetError(err, "%s", gai_strerror(rv));
        return ANET_ERR;        
    }
    if(info->ai_family == AF_INET)
    {
        // 解析出来的是一个 IPV4 地址
        /* 提供 一下 sockaddr_int 的类型定义，这是处理 ipv4 地址的结构体 */
        /*
        struct sockaddr_in
        {
            sa_family_t sin_family;  // 地址族，通常为 AF_INET，unsigned short 类型
            in_port_t sin_port;  // 端口号，unsigned short 类型
            struct in_addr sin_addr;  // IP 地址，封装了一个 unsigned long 类型的数值
            char sin_zero[8];   // 填充字节，用于与 struct sockaddr 对齐
        };
        大小共为 16 字节
        */
        struct sockaddr_in* sa = (struct sockaddr_in*)info->ai_addr;
        // 这里使用 inet_ntop 函数将 IP 地址转换为字符串形式
        // ntop 是 network to presentation 的缩写，即网络字节序（一般为大端序）二进制序列转换为可读的字符串形式的函数
        inet_ntop(AF_INET, &(sa->sin_addr), ipbuf, ipbuf_len);
    }
    else 
    {
        // 解析出来的是一个 IPV6 地址
        struct sockaddr_in6* sa = (struct sockaddr_in6*)info->ai_addr;
        inet_ntop(AF_INET6, &(sa->sin6_addr), ipbuf, ipbuf_len);
    }
    
    // 释放内存，Posix 网络库内置的函数
    freeaddrinfo(info);
    return ANET_OK;
}
```
- 总的来说，上面这个函数展示了如何从一个给定的 IP 地址字符串中解析出对应的 IP 地址，解析出来的 IP 地址会被存储到 ipbuf 中
-- -

``` C
static int anetSetReuseAddr(char* err, int fd)
{
    int yes = 1;
    /* Make sure connection-intensive things like the redis benchmark 
     * will be able to close/open sockets a zillion of times */
    if(setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) == -1)
    {
        anetSetError(err, "setsockopt SO_REUSEADDR: %s", strerror(errno));
        return ANET_ERR;
    }
    return ANET_OK;
}
```
- 设置 IP 地址复用的函数，如果不设置这个选项，那么在套接字被关闭后，该套接字的 IP 地址无法立即被重用，这可能会导致效率问题

``` C
static int anetCreateSocket(char* err, int domain)
{
    int s;
    if((s = socket(domain, SOCK_STREAM, 0)) == -1)
    {
        anetSetError(err, "creating socket: %s", strerror(errno));
        return ANET_ERR;
    }
    /* Make sure connection-intensive things like the redis benchmark 
    * will be able to close/open sockets a zillion of times */
    if(anetSetReuseAddr(err,s) == ANET_ERR)
    {
        // 如果无法设置 IP 地址复用，就关闭套接字并返回错误
        close(s);
        return ANET_ERR;
    }
    return s;
}
```
-- -
### 接下来是分 TCP 和 Unix 两种通信模式的功能函数（Redis 似乎并没有支持 UDP 通信）
``` C
#define ANET_CONNECT_NONE 0
#define ANET_CONNECT_NONBLOCK 1
// BE 是 best effort 
#define ANET_CONNECT_BE_BINDING 2 
static int anet
```

``` C
// 人话翻译一下函数签名：
// 给定错误信息存储地址 err，服务器端地址字符串 addr，服务进程端口号 port，客户端地址字符串 source_addr，连接标志 flags，来实现 addr:port 与 source_addr 之间的普通（generic）TCP 连接
static int anetTcpGenericConnect(char* err, const char* addr, int port, const char* source_addr, int flags)
{
    int s = ANET_ERR, rv;
    char portstr[6];
    struct addrinfo hints, * servinfo, *bservinfo, *p, *b;

    // 一种新奇的将整数转换为字符串的方法
    snprintf(portstr, sizeof(portstr), "%d", port);
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if((rv = getaddrinfo(addr,portstr,&hints,&servinfo)) != 0)
    {
        anetSetError(err, "%s", gai_strerror(rv));
        return ANET_ERR;
    }
    // 回顾前面 addrinfo 的定义，每一个 addrinfo 实例中都有一个 ai_next 指针，指向下一个 addrinfo 实例
    // 而这里遍历这个链表，就是为了找出所有可用的客户端地址进行连接处理
    for(p = servinfo; p != NULL; p = p->ai_next)
    {
        /* Try to create the socket and to connect it. 
         * If we fail in the socket() call, or on connect(), we retry with
         * the next entry in servinfo. */
        // 如果创建套接字失败，就继续检查下一个 addrinfo 实例
        if((s = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) 
            continue;
        // 这里第一次见到源码中使用 goto （）
        // Redis 希望所有的连接都是可重用的，如果无法设置成可复用，直接跳转到 error 部分
        if(anetSetReuseAddr(err,s) == ANET_ERR) goto error;
        // 这里如果 ANET_CONNECT_NONBLOCK 宏为真，那么进程尝试将这个套接字设置为非阻塞模式，失败直接跳转 error 处理模块
        if(flags & ANET_CONNECT_NONBLOCK && anetNonBlock(err,s) != ANET_OK)
            goto error;
        if(source_addr)
        {
            int bound = 0;
            // 上面 hints 的预设很简单，只设置了 ai_family 为 AF_UNSPEC，ai_socktype 为 SOCK_STREAM
            // 这里作者也声明了，使用 getaddrinfo 偷了个懒
            // 使用 bservinfo 来存储客户端的地址信息，也就是 source_addr 中可以解析出来的信息
            if((rv = getaddrinfo(source_addr, NULL, &hints, &bservinfo)) != 0)
            {
                anetSetError(err, "%s", gai_strerror(rv));
                goto error;
            }
            for(b = bservinfo; b != NULL; b = b->ai_next)
            {
                if(bind(s,b->ai_addr,b->ai_addrlen) != -1)
                {
                    bound = 1;
                    break;
                }
            }
            freeaddrinfo(bservinfo);
            if(!bound)
            {
                anetSetError(err, "bind: %s", strerror(errno));
                goto error; 
            }
        }
    }
}
```
-- -
### 编写文档时使用：回到 [RedieAe.md](./RedisAe.md)