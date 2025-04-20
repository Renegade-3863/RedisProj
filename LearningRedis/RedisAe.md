# Redis ae.h 头文件解析
#### 全名为 Asynchronous Event，是一个异步事件库，用于处理 Redis 服务器的事件

### 前置库：
- [monotonic.h](./RedisMonotonic.md)
-- -
### 简单看一下内部定义的这些宏变量：
``` C
// AE_OK 和 AE_ERR 是两个宏变量，分别代表函数执行成功和失败的返回值
#define AE_OK 0
#define AE_ERR -1

// AE_NONE 是一个宏变量，代表事件的类型，没有事件发生
// AE_READABLE 和 AE_WRITABLE 是两个宏变量，代表事件的类型，分别为可读和可写事件
// AE_BARRIER 与 AE_WRITABLE 一起使用，如果在同一事件循环迭代中已经触发了 AE_READABLE 事件，则不会触发 AE_WRITABLE 事件
#define AE_NONE 0
#define AE_READABLE 1
#define AE_WRITABLE 2
#define AE_BARRIER 4

// AE_FILE_EVENTS 和 AE_TIME_EVENTS 是两个宏变量，代表事件的类型，分别为文件事件和时间事件
// AE_ALL_EVENTS 是一个宏变量，代表所有事件类型的位掩码
// AE_DONT_WAIT 是一个宏变量，代表事件循环不应该阻塞等待事件发生
// AE_CALL_BEFORE_SLEEP 和 AE_CALL_AFTER_SLEEP 是两个宏变量，代表事件循环在进入睡眠状态之前和之后应该调用的函数
// 这些宏变量用于在事件循环中进行事件处理时的控制
#define AE_FILE_EVENTS (1<<0)
#define AE_TIME_EVENTS (1<<1)
#define AE_ALL_EVENTS (AE_FILE_EVENTS|AE_TIME_EVENTS)
#define AE_DONT_WAIT (1<<2)
#define AE_CALL_BEFORE_SLEEP (1<<3)
#define AE_CALL_AFTER_SLEEP (1<<4)

// AE_NOMORE 是一个宏变量，代表没有更多的事件需要处理
// AE_DELETED_EVENT_ID 是一个宏变量，代表一个已经被删除的事件的 ID
#define AE_NOMORE -1
#define AE_DELETED_EVENT_ID -1
```
- 上面的宏定义基本把事件循环的功能写明了
    - 事件循环可以处理文件事件和时间事件
    - 事件循环可以阻塞等待事件，也可以不阻塞
    - 事件循环可以在进入睡眠状态之前和之后调用函数

- Redis 的事件循环包括如下几个结构
    - aeEventLoop：事件循环
    - aeFileEvent：文件事件
    - aeTimeEvent：时间事件
    - aeFiredEvent：已触发的事件
    - aeFileProc：文件事件处理函数类
    - aeTimeProc：时间事件处理函数类
    - aeEventFinalizerProc：事件循环结束时的清理函数类
    - aeBeforeSleepProc：事件循环进入睡眠状态之前的处理函数类

#### 补充一个语法知识点：
``` C
typedef void aeFileProc(struct aeEventLoop* eventLoop, int fd, void* clientData, int mask);
```
- typedef 有两种不同的用法，这里是类型定义的用法
    - aeFileProc 实际上是一个类型名，和 int 等类型名一样
    - 之后定义函数时，就可以用 aeFileProc 类型的函数指针来指向具有上面这种返回值和参数列表的函数了


#### 再来看一下处理文件事件和时间事件的结构体
``` C
typedef struct aeFileEvent 
{
    int mask; // 事件的类型，可读、可写等 (即上面的 AE_READABLE 等)
    aeFileProc* rfileProc; // 可读事件的处理函数
    aeFileProc* wfileProc; // 可写事件的处理函数
    void* clientData; // 客户端的数据
} aeFileEvent;
```
- 一个文件事件，可以是可读事件，也可以是可写事件，也可以同时是可读可写事件
- 每个文件事件都有一个 clientData 指针，用于指向客户端的数据
- 这里的客户端要么是可读的数据，要么是要写入的数据
``` C
typedef struct aeTimeEvent
{
    long long id; // 时间事件的 ID
    monotime when; // 时间事件的触发时间
    aeTimeProc* timeProc; // 时间事件的处理函数
    aeEventFinalizerProc* finalizerProc; // 时间事件的清理函数
    void* clientData; // 客户端的数据
    struct aeTimeEvent* prev; // 指向上一个时间事件的指针
    struct aeTimeEvent* next; // 指向下一个时间事件的指针
    int refcount; // 时间事件的引用计数，用于跟踪事件的引用次数
    // 这个 refcount 的作用似乎类似于 C++ 中的智能指针
    // 当尝试释放一个时间事件时，只当 refcount 为 0 时，才会真正释放这个时间事件
    // 这是为了避免在事件循环中使用这个时间事件时，这个时间事件被提前释放了
} aeTimeEvent;
```

``` C
typedef struct aeFiredEvent
{
    int fd; // 文件描述符
    int mask;   // 事件的类型，可读、可写等 (即上面的 AE_READABLE 等)
} aeFiredEvent;
```
- 这个结构体用于记录已经触发的事件
``` C
typedef struct aeEventLoop
{
    int maxfd; // 当前注册在这个事件循环中的最大文件描述符
    int setsize; // 最大可以跟踪的文件描述符个数
    long long timeEventNextId; // 下一个时间事件的 ID
    int nevents; // 已经注册在这个事件循环中的事件个数
    aeFileEvent* events; // 注册的文件事件链表
    aeFiredEvent* fired; // 已经触发的事件链表
    aeTimeEvent* timeEventHead; // 注册的时间事件链表
    int stop; // 事件循环是否停止的标志
    void* apidata; // 似乎是用于具体 API 的其它数据
    aeBeforeSleepProc* beforeSleep; // 事件循环进入睡眠状态之前的处理函数
    aeBeforeSleepProc* afterSleep; // 事件循环进入睡眠状态之后的处理函数
    int flags; // 事件循环的标志位
    void* privdata[2]; // 私有数据，单看结构体定义并不能完全理解它的作用
} aeEventLoop;
```

# Redis ae.c 文件解析
#### 是上面 ae.h 文件的具体实现
### 前置库：
- [anet.h](./RedisAnet.md)

-- -
### 编写文档时使用：回到 [RedisConnection.md](./RedisConnection.md)