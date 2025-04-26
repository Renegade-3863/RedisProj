## Redis 核心源码组件之：异步 I/O 多路复用 ae_epoll.c

### 这个封装个人认为是所有异步 I/O 多路复用封装中效率最高，实用性也最高的一个
### 有关 epoll 机制的解析，可以参考另一份 markdown 文档：[epoll详解](./LinuxEpoll.md)
### ae_evport 那个只能用在 Solaris 操作系统上，泛用性很差

### 前置库：
- Redis 根文件，include 了 Linux 的 epoll 库，即 <sys/epoll.h>

``` C
typedef struct aeApiState
{
    int epfd;
    // epoll_event 是一个用于描述事件内容的结构体
    // 这里给出内部结构：
    /*
    struct epoll_event
    {
        uint32_t events;    // epoll 事件类型，如 EPOLLIN(可读)、EPOLLOUT(可写) 等
        epoll_data_t data;  // 用户数据
    };
    */
    // 后文使用这个指针是以数组的形式，因为 epoll_wait 函数接受和返回的 epoll_event 参数都是数组
    struct epoll_event* events;
} aeApiState;
```
- 看到这里，基本能明确每个 eventLoop 中包含的这个 aeApiState 的作用了，它基本上就是用于处理 I/O 多路复用机制所需的数据的集合

``` C
static int aeApiCreate(aeEventLoop* eventLoop)
{
    aeApiState* state = zmalloc(sizeof(aeApiState));

    if(!state) return -1;
    // eventLoop->setsize : max number of file descriptors tracked by this event loop
    // 所以这里我们给 state 的 events 成员指定可以监控的最大文件描述符个数对应的内存空间
    state->events = zmalloc(sizeof(struct epoll_event)*eventLoop->setsize);
    if(!state->events)
    {
        zfree(state);
        return -1;
    }
    // 现在 1024 这个建议参数已经不会对内核实际的操作有任何影响了
    state->epfd = epoll_create(1024);  /* 1024 is just a hint for the kernel */
}
```