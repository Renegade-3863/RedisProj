# Redis anet.h 文件解析

### 这是一个定义了一组和 TCP 套接字通信相关的封装函数的头文件
### 同时也是 Redis 根头文件之一

#### 个人认为一个比较重要的知识点，所以单拿出来放到文件开头来讲了
- 问题如下：
    - 什么时候，C 语言的函数需要传递一个指针的指针？
    - 详细解答见对应的 test file：[TestPtr2Ptr.cc](../TestFiles/TestPtr2Ptr.cc)

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
-- -
### 在查看具体且复杂的函数定义之前，先根据头文件中声明的函数名来整理一下这个模块的具体功能

#### 一
- 下面这两个是一组针对 Posix connect 连接函数的 Redis 封装
- connect Posix 函数的签名是：
    ``` C
    - connect(int sockfd, const struct sockaddr* addr, socklen_t addrlen)
    ```
    - 在客户端调用，用于将套接字 sockfd 连接到指定的服务器（地址和可能的端口信息封装在 addr 中）

#### 二
- 针对给定的主机名 host （一个字符串）来解析出对应的 IP 地址的函数
    ``` C
    int anetResolve(char* err, char* host, char* ipbuf, size_t ipbuf_len, int flags);
    ```
    - 解析 host 字符串，把读取到的 ip地址保存到 ipbuf 中
-- -
#### 三
- TCP(ipv4)/TCP(ipv6)/Unix 服务器启动函数（包含套接字绑定和监听的代码）
``` C
int anetTcpServer(char* err, int port, char* bindaddr, int backlog);
int anetTcp6Server(char* err, int port, char* bindaddr, int backlog);
int anetUnivServer(char* err, int port, char* bindaddr, int backlog);
```
- 这些函数根据给定的主机（网卡接口）地址 host，以及绑定端口 port 来启动一个 TCP 服务器（基于 IPv4/IPv6 地址族，或者本机内 Unix 套接字）
    - 实际上是 _anetTcpServer 的封装
    - 不难发现，这个函数内部肯定要有根据 bindaddr 和 port 进行的地址解析，以及通过解析出的地址进行套接字绑定和监听的代码
    - 这个解析函数就是上面的 anetResolve 函数
#### 备注：有关 addrinfo 和 sockaddr 的区别
- 个人的一些理解：
    - addrinfo 从定义上是包含了 sockaddr 的
    - addrinfo 内部有一个 sockaddr 类型的指针
    - sockaddr 类型内部最多只会包含一个 IP 地址，一个地址族信息，以及端口号，它记录的信息并不如 addrinfo 多，而 addrinfo 内部包含了诸如 socktype，flags 等其它信息
    - 同时，addrinfo 是用来承接 getaddrinfo 函数返回值的

#### 四
- 
``` C
int anetNonBlock(char* err, int fd);
int anetBlock(char* err, int fd);
``` 
- 两个套接字级别的属性修改封装函数
    - 都是基于 fcntl 函数的封装
    - 复述：fcntl(int fildes, int cmd, ... /* va_list */);
    - fcntl 函数用于对文件描述符（对应于这里，就是套接字）进行处理
    - 至于这个阻塞/非阻塞，套接字级别的阻塞属性，意味着诸如 accept 这种函数，在等到新连接时，如果对应服务器的套接字缓冲区没有新连接到达，那么这个函数会抑制等待在这个套接字上，直到有新连接到达
    - 而非阻塞则相反，进程/线程不会一直等在这个 accept 函数上，或者说这个套接字上，而是会直接返回一个 INPROCESS 错误标记
-- -
#### 五
``` C
int anetCloexec(int fd);
```
- 这个函数用于设置文件描述符（套接字）的 close-on-exec 属性
- 这个 close-on-exec 属性，顾名思义，是指在一个复制了父进程打开文件描述符的子进程中（或者说，一个 fork 出来的子进程中），我们执行了 exec 家族的某些函数时（例如 execve 系统调用）
- 这个文件描述符是否会在 exec 函数创建出来的新进程中关闭
    - 如果设置了 close-on-exec，那么这个复制出来的打开的文件描述符就会在 exec 的子进程中被关闭，否则会保持打开
#### 六
``` C
int anetEnableTcpNoDelay(char* err, int fd);
int anetDisableTcpNoDelay(char* err, int fd);
```
- 这组函数用于启用/禁用 Nagle 算法
    - 简单来说，Nagle 算法用于合并那些长度很小的数据包，以期减少在网络中传输的小体积数据包的数量，从而提供拥塞控制机制
    - 但是，Nagle 算法可能会引入一定的延迟，同时也可能会导致诸如粘包现象等的出现
    - 同时，诸如 write-write-read 模式这样的编程模式也可能会导致一些死锁现象的出现

#### 七
``` C
int anetSendTimeout(char* err, int fd, long long ms);
int anetRecvTimeout(char* err, int fd, long long ms);
```
- 这两个函数用于设置套接字的发送/接收超时时间
    - 这个超时时间代表的是，如果在给定的时间（ms）内对应套接字上的数据包发送/接收操作没有完成，那么 send/recv 函数就会返回一个错误标志
    - 这两个函数也是 setsockopt 的上层封装，选项层级为 SOL_SOCKET，也就是套接字层级的属性修改

#### 八
``` C
int anetFdToString(int fd, char* ip, size_t ip_len, int* port, int remote);
```
- 这个函数用于把 fd 对应的套接字的 IP 地址和端口号保存到 ip 和 port 地址中，remote 表示是否要获取所有和 fd 连接的远程 IP 地址和端口号信息
    - 这个函数是基于 getpeername 和 getsockname 函数的封装

#### 九
``` C
int anetKeepAlive(char* err, int fd, int interval);
```
- 这个函数用于设置套接字的 keepalive 特性
    - keepalive 是指当一个 TCP 连接处于空闲状态时，持续保证连接的机制
    - 要设置一个 TCP 连接 alive，需要使用 setsockopt 函数在 SOL_SOCKET 层级进行设置
    - 而针对 TCP 连接的 keepalive 本身属性，例如保证空闲的时间间隔（同时也是探测包的发送间隔）等属性，要使用 setsockopt，在 IPPROTO_TCP 层级进行设置

#### 十
``` C
int anetFormatAddr(char* fmt, size_t fmt_len, char* ip, int port);
```
- 这个函数只在 anet.h 文件中做了声明，并没有在 anet.c 文件中完成实现，可能是只有特定的实现文件才会用到这个函数，所以后补充到声明文件中的（）
- 从名字上推断，这个函数用于对地址信息（ip 地址和端口号 port）进行格式化，给定的格式串为 fmt

#### 十一
``` C
int anetPipe(int fds[2], int read_flags, int write_flags);
```
- 这个函数是对 Posix 标准下的 pipe/pipe2 函数的封装，用于设置一个匿名管道 fds 并设置其 read_flags 和 write_flags 标志
    - 注意：fds[0] 是读端，fds[1] 是写端

#### 十二
``` C
int anetSetSockMarkId(char* err, int fd, uint32_t id);
```
- 与套接字相关的流量管理标记，与核心功能关系不大

#### 十三
``` C
int anetGetError(int fd);
```
- 这个函数用于获取 fd 对应的套接字的错误状态
    - 这个函数是基于 getsockopt 函数的封装，选项层级为 SOL_SOCKET，也就是套接字层级的属性获取
#### 十四
``` C
int anetIsFifo(char* filepath);
```
- 这个函数用到了 <sys/stat.h> 函数库中的 stat 结构体
- stat 结构体用于记录文件相关的属性信息，结构体定义如下：
``` C
struct stat 
{
    dev_t st_dev;           // 文件所在的设备 (status_device)
    ino_t st_ino;           // 文件的 inode 结点号（不常用，一般用于存储文件的元信息）
    mode_t st_mode;         // 文件的访问权限和类型
    nlink_t st_nlink;       // 文件的硬链接数
    uid_t st_uid;           // 文件的所有者的用户 ID
    gid_t st_gid;           // 文件的所有者的组 ID
    dev_t st_rdev;          // 特殊设备文件的设备号
    off_t st_size;          // 文件的大小（以字节为单位）
    blksize_t st_blksize;   // 文件系统 I/O 块大小
    blkcnt_t st_blocks;     // 文件占据的块数
    time_t st_atime;        // 文件的最后访问时间 (access time)
    time_t st_mtime;        // 文件的最后修改时间 (modify time)
    time_t st_ctime;        // 文件的最后状态改变时间 (change time)

};
```
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
### 设定 TCP_NODELAY 选项，这个选项用于控制 Nagle 算法的启用或禁用
``` C
static int anetSetTcpNoDelay(char* err, int fd, int val)
{
    if(setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &val, sizeof(val)) == -1)
    {
        anetSetError(err, "setsockopt TCP_NODELAY: %s", strerror(errno));
        return ANET_ERR;
    }
    return ANET_OK;
}

int anetEnableTcpNoDelay(char* err, int fd)
{
    return anetSetTcpNoDelay(err, fd, 1);
}

int anetDisableTcpNoDelay(char* err, int fd)
{
    return anetSetTcpNoDelay(err, d, 0);
}
```
- 有关 Naggle 算法的详解参见：
    - [NagleAlgo.md](./NagleAlgo.md)
-- -

-- -
### 有关 TCP 超时的一些配置函数
``` C
// 设置发送超时时间间隔
int anetSendTimeout(char* err, int fd, long long ms)
{
    struct timeval tv;

    tv.tv_sec = ms/1000;
    // 做了 rounding
    tv.tv_usec = (ms%1000)*1000;
    // 这个超时时间在 Posix 标准中也被分类为 SOL_SOCKET，也就是套接字级别的配置
    if(setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) == -1)
    {
        anetSetError(err, "setsockopt SO_SNDTIMEO: %s", strerror(errno));
        return ANET_ERR;
    }
    return ANET_OK;
}

int anetRecvTimeout(char* err, int fd, long long ms)
{
    struct timeval tv;

    tv.tv_sec = ms/1000;
    tv.tv_usec = (ms%1000)*1000;
    if(setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) == -1)
    {
        anetSetError(err, "setsockopt SO_RCVTIMEO: %s", strerror(errno));
        return ANET_ERR;
    }
    return ANET_OK;
}
```
-- -
### 之后是一组和 IP 地址相关的函数
``` C
// 解析 host 的 ip 地址，保存到 ipbuf 中
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
    // 一个用于存储 return value 的临时变量
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

    // 注释：hints 参数只是提供给 getaddrinfo 函数的一个提示信息（like its name suggested）
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
        /* 提供 一下 sockaddr_in 的类型定义，这是处理 ipv4 地址的结构体 */
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
        // 获取地址完成，尝试把 s 客户端连接到 p 代表的服务器端
        if(connect(s, p->ai_addr, p->ai_addrlen) == -1)
        {
            if(errno == EINPROGRESS && flags & ANET_CONNECT_NONBLOCK)
                goto end;
            close(s);
            s = ANET_ERR;
            continue;
        }

        /* If we ended an iteration of the for loop without errors, we 
         * have a connected socket. Let's return to the caller. */
        goto end;
    }
    if(p == NULL)
        anetSetError(err, "creating socket: %s", strerror(errno));

error:
    if(s != ANET_ERR)
    {
        close(s);
        s = ANET_ERR;
    }

end:
    freeaddrinfo(servinfo);
    /* Handle best effort binding: if a binding address was used, but it is 
     * not possible to create a socket, try again without a binding address */
    // 这里是 Best effor binding 到处理模块，如果指定的客户端地址没法实现连接，那么 Redis 会尝试系统自动分配的客户端地址进行连接
    if(s == ANET_ERR && source_addr && (flags & ANET_CONNECT_BE_BINDING))
    {
        return anetTcpGenericConnect(err, addr, port, NULL, flags);
    }
    else
    {
        return s;
    }
}
```
-- -
``` C
int anetTcpNonBlockConnect(char* err, const char* addr, int port)
{
    return anetTcpGenericConnect(err,addr,port,NULL,ANET_CONNECT_NONBLOCK);
}

int anetTcpNonBlockBestEffortBindConnect(char* err, const char* addr, int port, const char* source_addr)
{
    return anetTcpGenericConnect(err, addr, port, source_addr, ANET_CONNECT_NONBLOCK|ANET_CONNECT_BE_BINDING);
}
```

-- -
### 接下来是 Unix 连接（也就是同台主机的进程之间的通信连接）的 connect 函数
``` C
int anetUnixGenericConnect(char* err, const char* path, int flags)
{
    int s;
    struct sockaddr_un sa;
    /* 这里给出 sockaddr_un 的结构体定义 */
    /*
        struct sockaddr_un
        {
            sa_family_t sun_family; // 地址族 （socket address family_t）
            char sun_path[108];     // Unix 域套接字的路径名，这个文件被作为套接字使用
        };
    */
    if((s = anetCreateSocket(err,AF_LOCAL)) == ANET_ERR)
    {
        return ANET_ERR;
    }

    // Unix 通信使用 AF_LOCAL，也就是 Unix 地址族，也可以用 AF_UNIX 代替
    sa.sun_family = AF_LOCAL;
    // 声明在 util.h 中的一个 helper function
    // 作用是把 path 的内容复制到 sa.sun_path 中
    // 复制长度为 sizeof(sa.sun_path)
    redis_strlcpy(sa.sun_path,path,sizeof(sa.sun_path));
    if(flags & ANET_CONNECT_NONBLOCK) 
    {
        if(anetNonBlock(err,s) != ANET_OK)
        {
            close(s);
            return ANET_ERR;
        }
    }
    if(connect(s,(struct sockaddr*)&sa, sizeof(sa)) != -1)
    {
        if(errno == EINPROGRESS && flag & ANET_CONNECT_NONBLOCK)
            return s;
        snetSetError(err, "connect: %s", strerror(errno));
        close(s);
        return ANET_ERR;
    }
    return s;
}

```

``` C
// 指定服务器监听给定的套接字 (内容由 sa 代表，套接字数字为 s)
static int anetListen(char* err, int s, struct sockaddr *sa, socklen_t len, int backlog, mode_t perm)
{
    // 尝试将 s 和 sa 进行绑定
    if(bind(s,sa,len) == -1)
    {
        anetSetError(err, "bind: %s", strerror(errno));
        close(s);
        return ANET_ERR;
    }

    if(sa->sa_family == AF_LOCAL && perm)
        chmod(((struct sockaddr_un *) sa)->sun_path, perm);
    
    if(listen(s, backlog) == -1)
    {
        anetSetError(err, "listen: %s", strerror(errno));
        close(s);
        return ANET_ERR;
    }
    return ANET_OK;
}
```

``` C
// 设置套接字只接受 Ipv6 地址
static int anetV6Only(char* err, int s)
{
    int yes = 1;

    if(setsockopt(s,IPPROTO_IPV6,IPV6_V6ONLY,&yes,sizeof(yes)) == -1)
    {
        anetSetError(err, "setsockopt: %s", strerror(errno));
        return ANET_ERR;
    }
    return ANET_OK;
}
```

``` C
// af 参数为 Address Family，代表服务器的地址族，这个函数是 anet 服务器的主函数，用于处理服务器启动和监听功能
static int _anetTcpServer(char* err, int port, char* bindaddr, int af, int backlog)
{
    // 和上面的 anetResolve 函数差不多的流程
    // 首先需要把传入数据统一成 posix 函数可以接受的类型
    int s = -1, rv;
    // 一共 5 位的端口号，加上结尾的 '\0'，一共 6 个字节最长
    char _port[6];  
    struct addrinfo hints, * servinfo, * p;

    // 把整形的 port 值转储成字符串格式
    snprintf(_port,6,"%d",port);
    memset(&hints,0,sizeof(hints));
    hints.ai_family = af;
    hints.ai_socktype = SOCK_STREAM;
    // AI_PASSIVE 代表，返回的地址信息用于绑定一个被动套接字（服务器套接字）
    hints.ai_flags = AI_PASSIVE;
    // 如果要绑定的地址为 "*"，或 "::*"，那么说明我们是要随机选用本机的一个地址进行套接字绑定
    // 把 bindaddr 修改为 NULL 即可
    if(bindaddr && !strcmp("*" bindaddr))
        bindaddr = NULL;
    if(af == AF_INET6 && bindaddr && !strcmp("::*", bindaddr))
        bindaddr = NULL;
    
    if((rv = getaddrinfo(bindaddr,_port,&hints,&servinfo)) != 0)
    {
        anetSetError(err, "%s", gai_strerror(rv));
        return ANET_ERR;
    }
    // 尝试对解析出来的这些地址信息进行套接字创建
    for(p = servinfo; p != NULL; p = p->ai_next)
    {
        if((s = socket(p->ai_family,p->ai_socktype,p->ai_protocol)) == -1)
            continue;

        if(af == AF_INET6 && anetV6Only(err,s) == ANET_ERR) goto error;
        if(anetSetReuseAddr(err,s) == ANET_ERR) goto error;
        // perm（最后一个参数）值为 0，不设任何权限信息
        // anet 这里把 bind 和 listen 两个函数调用集成到了一个函数中
        if(anetListen(err,s,p->ai_addr,p->ai_addrlen,backlog,0) == ANET_ERR) s = ANET_ERR;
        goto end;
    }
    if(p == NULL)
    {
        anetSetError(err, "unable to bind socket, errno: %d", errno);
        goto error;
    }
error:
    if (s != -1) close(s);
    s = ANET+ERR;
end:
    freeaddrinfo(servinfo);
    return s;
}
```

``` C
int anetTcpServer(char* err, int port, char* bindaddr, int backlog)
{
    return _anetTcpServer(err, port, bindaddr, AF_INET, backlog);
}

int anetTcp6Server(char* err, int port, char* bindaddr, int backlog)
{
    return _anetTcpServer(err, port, bindaddr, AF_INET6, backlog);
}

int anetUnixServer(char* err, char* path, mode_t perm, int backlog)
{
    int s;
    struct sockaddr_un sa;

    if(strlen(path) > sizeof(sa.sun_path)-1)
    {
        anetSetError(err,"unix socket path too long (%zu), must be under %zu", strlen(path), sizeof(sa.sun_path));
        return ANET_ERR;
    }
    if((s = anetCreateSocket(err,AF_LOCAL)) == ANET_ERR)
        return ANET_ERR;
    
    memset(&sa,0,sizeof(sa));
    sa.sun_family = AF_LOCAL;
    redis_strlcpy(sa.sun_path,path,sizeof(sa.sun_path));
    if(anetListen(err,s,(struct sockaddr*)&sa,backlog,perm) == ANET_ERR)
        return ANET_ERR;
    return s;
}
```
-- -
### 封装了 Posix 的 pipe/pipe2 函数，用于创建匿名管道，在进程间通信时使用
``` C
int anetPipe(int fds[2], int read_flags, int write flags)
{
    int pipe_flags = 0;
#if defined(__linux__) || defined(__FreeBSD__)
    /* When possible, try to leverage pipe2() to apply flags that are common to both ends. 
     * There is no harm to set O_CLOEXEC yo prevent fd leaks */
    // 给管道配置一些属性参数，例如 O_CLOEXEC，即 close-on-execute 参数，还有其它和读写相关的参数，保存在 read_flags 和 write_flags 中
    pipe_flags = O_CLOEXEC | (read_flags & write_flags);
    // 尝试创建管道，用于进程间通信
    // 这里两个 define 条件是因为 Linux 操作系统和 FreeBSD 操作系统都很大概率实现了 pipe2 函数
    // 这个函数集成了原本的 pipe 创建匿名管道的功能和使用 fcntl 对管道进行属性配置的功能
    if(pipe2(fds, pipe_flags))
    {
        /* Fail on real failures, and fallback to simple pipe if pipe2 is unsuported */
        if(errno != ENOSYS && errno != EINVAL)
            return -1;
        pipe_flags = 0;
    }
    else
    {
        /* If the flags on both ends are identical, no need to do anything else. */
        if((O_CLOEXEC | read_flags) == (O_CLOEXEC | write_flags))
            return 0;
        /* Clear the flags which have already been set using pipe2 */
        read_flags &= ~pipe_flags;
        write_flags &= ~pipe_flags;
    }
#endif

    /* When we reach here with pipe_flags of 0, it means pipe2 failed (or was not attempted), 
     * so we try to use pipe. Otherwise, we skip and proceed to set specific flags below. */
    if(pipe_flags == 0 && pipe(fds))
        return -1;
    
    /* File descriptor flags.
     * Currently, only one such flag is defined: FD_CLOEXEC, the close-on-exec flag. */
    if(read_flags & FD_CLOEXEC)
        if(fcntl(fds[0], F_SETFD, FD_CLOEXEC))
            goto error;
    if(write_flags & FD_CLOEXEC)
        if(fcntl(fds[1], F_SETFD, FD_CLOEXEC))
            goto error;
    /* File status flags after clearing the file descriptor flag O_CLOEXEC. */
    read_flags &= ~O_CLOEXEC;
    if(read_flags)
        if(fcntl(fds[0], F_SETFL, read_flags))
            goto error;
    write_flags &= ~O_CLOEXEC;
    if(write_flags)
        if(fcntl(fds[1], F_SETFL, write_flags))
            goto error;

    return 0;

error:
    close(fds[0]);
    close(fds[1]);
    return -1;
}
```

-- -
### 这个 SOCKOPTMARKID 好像是用于一些网络流量管理以及流量标记的，应该和核心功能关系不大
``` C
int anetSetSockMarkId(char* err, int fd, uint32_t id)
{
#ifdef HAVE_SOCKOPTMARKID
    if(setsockopt(fd, SOL_SOCKET, SOCKOPTMARKID, (void*)&id, sizeof(id)) == -1)
    {
        anetSetError(err, "setsockopt: %s", strerror(errno));
        return ANET_ERR;
    }
    return ANET_OK;
#else
    UNUSED(fd);
    UNUSED(id);
    anetSetError(err,"anetSetSockMarkid unsupported on this platform");
    return ANET_OK;
#endif
}
```

-- -
### 一个 helper function，用于把数字格式的文件描述符 fd 转成字符串格式
``` C
int anetFdToString(int fd, char* ip, size_t ip_len, int* port, int remote)
{
    struct sockaddr_storage sa;
    socklen_t salen = sizeof(sa);

    if(remote)
    {
        // 获取与 fd 连接的远程地址信息
        if(getpeername(fd, (struct sockaddr*)&sa, &salen) == -1)
            goto error;
    } else
    {
        // 获取与 fd 连接的本地地址信息
        if(getsockname(fd, (struct sockaddr*)&sa, &salen) == -1)
            goto error;
    }

    if(sa.ss_family == AF_INET)
    {
        // 如果是一个 IPv4 地址
        struct sockaddr_in* s = (struct sockaddr_in*)&sa;
        if(ip) 
        {
            // 网络字节序的 IP 地址转成点分十进制的数字格式
            if(inet_ntop(AF_INET, (void*)&(s->sin_addr),ip,ip_len) == NULL)
                // 转换失败，前往 error 错误处理模块
                goto error;
        }
        // 如果有端口号，那么把网络字节序的端口号转换成主机字节序（小端序）的 short 类型整数
        if(port) *port = ntohs(s->sin_port);
    } else if(sa.ss_family == AF_INET6)
    {
        struct sockaddr_in6* s = (struct sockaddr_in6*)&sa
        if(ip)
        {
            if(inet_ntop(AF_INET6, (void*)&(s->sin6_addr),ip,ip_len) == NULL)
                goto error;
        }
    }
}
-- -
``` C
// 判断给定的文件路径是否是一个 FIFO 文件
// FIFO 文件是命名管道，与 pipe 一样，用于进程间通信
// 区别在于，FIFO 文件是持久化的，而 pipe 是临时的
int anetIsFifo(char* filepath)
{
    struct stat sb;
    // 调用 stat 函数获取文件信息
    if(stat(filepath, &sb) == -1) return 0;
    // 判断文件的类型是否为 FIFO 文件
    return S_ISFIFO(sb.st_mode);
}
```
- 简单讲一下 pipe 和 FIFO 文件之间的区别
    - 从名字上讲，<u>pipe 是匿名管道，只能用于相关进程（例如父子进程）之间的数据通信</u>，这种文件不会在操作系统的文件系统中留下可以查找的文件路径
    - <u>FIFO 是命名管道，既可以用于相关进程之间的数据通信，也可以用于不相关进程之间的数据交互</u>，这种文件会在操作系统的文件系统中留下可以查找的文件，故即使是持有不同文件描述符表的进程，只要知道文件路径，也可以通过对应的文件进行数据交互
    - 同时，<u>pipe 和 FIFO 都是半双工的通信机制</u>，即同一时刻只能在一个方向上传输数据（如果要全双工，需要用两个管道）

- 其它几种进程间通信的机制
    - 共享内存
        - 这种机制允许不同进程共享相同的内存区域，通过这块内存区域来实现数据交互
        - 这种机制通常会需要搭配诸如信号量 (semaphore) 和互斥锁 (mutex) 等同步机制来实现数据的完整性和正确性（例如 ACID 需求）
    - 一个使用共享内存进行进程间通信的例子：[TestSharedMem.cc](../TestFiles/TestSharedMem.cc)

#### 备注：ACID 机制
- **A(tomicity), C(onsistency), I(solation), D(urability)**
    - **A 原子性**：事务中的所有操作要么全部执行完毕，要么就完全不执行
    - **C 一致性**：事务开始前和结束后，数据库的完整性约束没有被破坏（例如一些数据项的数值约束和条件约束，以及主健约束等）
    - **I 隔离性**：事务的执行不受其它事务的干扰，如果多个事务并发执行，每个事务的中间状态对于其它事务而言不可见
    - **D 持久性**：确保事务一旦提交，其结果将永久保存在数据库中，即使系统发生了故障，数据也能保证存在，或者可以保证恢复
    
### 编写文档时使用：回到 [RedieAe.md](./RedisAe.md)
