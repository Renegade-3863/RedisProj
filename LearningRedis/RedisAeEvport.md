# Redis ae_evport.c 文件解析
### event_port 模型？这个模型倒是第一次见
#### 简单梳理一下整体的文件结构，这似乎是一个根文件
-- -
- 从 include 语句来看，这个模型似乎用到了 port 库和 poll 库
- 根据个人理解，poll 本身是不如 event_poll，也就是 epoll 高效的，我们看一下这个 evport 是怎么实现的

- 首先是一段注释：

``` C
/* 
 * This file implements the ae API using event ports, present on Solaris-based
 * systems since Solaris 10. Using the event port interface, we associate file 
 * descriptors with the port. Each association also includes the set of poll(2)
 * events that the consumer is interested in (e.g., POLLIN and POLLOUT).
 * 
 * There's one tricky place to this implementation: when we return events via 
 * aeApiPoll, the corresponding file descriptors become dissociated from 
 * the port. This is necessary because poll events are level-triggered, so if the fd didn't become dissociated, it would immediately fire another event since
 * the underlying state hasn't changed yet.
 * 
 * 上面这一段提到了一些和水平触发有关的问题
 * 水平触发 (level-triggered)，简单来说就是，只要某个文件描述符当前是可读的，那么就会一致触发可读事件
 * 与边缘触发 (edge-triggered) 不同，边缘触发只会在文件描述符状态发生变化时触发一次事件
 * 所以这里作者才提到，poll 函数会在返回事件时，把文件描述符从端口中分离出来
 * 
 * We must re-associate the file descriptor, but only after we know that our caller has actually read from it.
 * The ae API does not tell us exactly when that happens, but we do know that
 * it must happen by the time aeApiPoll is called again. Our solution is to keep track of the last fds returned by aeApiPoll and re-associate them next time aeApiPoll is invoked.
 * 
 * 这里，作者提到他们的解决方案：
 * 我们跟踪由 aeApiPoll 最后一次返回的那些文件描述符 fds，并在下次调用 aeApiPoll 时重新关联它们（还以为是什么高深的东西，这里其实在学 select, poll 和 epoll 的时候应该都是学过的。。）
 * 
 * To summarize, in this module, each fd association is EITHER (a) represented
 * only via the in-kernel association OR (b) represented by pending_fds and pending_masks. 
 * (b) is only true for the last fds we returned from aeApiPoll,
 * and only until we enter aeApiPoll again (at which point we restore the in-kernel association)
 * 
 * 这一段也没什么，只是明确说出了如何实现上面要求的机制
 * 使用 pending_fds 和 pending_masks 来保存最后一次 aeApiPoll 返回的文件描述符和事件类型，来实现后续重新关联的机制
 * */
```
-- -
- 上面一大段注释后，紧跟着的就是包含了 pending_fds 和 pending_masks 的一个封装结构体
``` C
typedef struct aeApiState 
{
    int portfd;                             // 事件端口，代表这些保存的文件描述符应该恢复到哪个端口
    uint_t npending;                        // 保存的文件描述符的数量
    int pending_fds[MAX_EVENT_BATCHSZ];     // 保存的文件描述符数组
    int pending_masks[MAX_EVENT_BATCHSZ];   // 掩码数组，只看到这里用途尚不明确
} aeApiState;
```

-- -
``` C
static int aeApiCreate(exEventLoop* eventLoop)
{
    int i;
    // 这里的 zmalloc 不用想太多，把它当成 C 语言自带的 malloc 函数就行
    // Redis 这里对 malloc 加了一层复用和错误处理的封装
    asApiState* state = zmalloc(sizeof(aeApiState));
    if(!state) return -1;

    // 简单查阅了一下 AI，这个 port_create 函数是 Solaris 操作系统特有的函数，用于创建一个事件端口，返回的是这个端口的文件描述符
    // 后续可以用 port_accociate 函数来关联文件描述符和端口
    state->portfd = port_create();
    if(state->portfd == -1)
    {
        // 创建事件端口失败，释放内存并返回 -1
        zfree(state);
        return -1;
    }
    // Redis 真的很喜欢这个 close-on-exec 机制（）
    anetCloexec(state->portfd);

    /* 对 state 的内部元素进行初始化 */
    state->npending = 0;
    for(i = 0; i < MAX_EVENT_BATCHSZ; i++)
    {
        state->pending_fds[i] = -1;
        state->pending_masks[i] = AE_NONE;
    }

    // 将这个 api 数据交给事件循环管理
    eventLoop->apidata = state;
    return 0;
}
```
- 这个函数用于创建一个事件端口，并将其关联到运行中的事件循环中
- 注意，这个函数，甚至于这整个 ae_evport.c 文件，都必须在 Solaris 10 及以上的操作系统上才能使用

``` C
static int aeApiResize(aeEventLoop* eventLoop, int setsize)
{
    (void) eventLoop;
    (void) setsize;
    /* Nothing to resize here. */
    return 0;
}
```
- 这个函数什么也没做（），虽然它名字叫做：ApiResize，似乎是想调整 aeApiState 的大小
- 好吧，它还是做了一点事情，它把 eventLoop 和 setsize 标记为了未使用（）典型的脱裤子放 p

``` C
static void aeApiFree(aeEventLoop* eventLoop)
{
    aeApiState* state = eventLoop->apidata;
    
    // 关闭这个事件端口，就和关闭文件描述符一样，因为它本质上还是个文件
    close(state->portfd);
    // 释放 state 元信息结构体占用的内存
    zfree(state);
}

// 这个函数用于从 ApiState 中查找 fd 对应的索引
// 如果找到了，返回索引，否则返回 -1
static int aeApiLookupPending(aeApiState* state, int fd)
{
    uint_t i;

    for(i = 0; i < state->npending; i++)
    {
        if(state->pending_fds[i] == fd)
            return (i);
    }

    return (-1);
}
```
- 清除 API 数据

``` C
// 注释写了，为了封装 port_associate 函数，包了这盘 aeApiAssociate 饺子
static int aeApiAssociate(const char* where, int portfd, int fd, int mask)
{
    int events = 0;
    int rv, err;

    if(mask & AE_READABLE)
        events |= POLLIN;
    if(mask & AE_WRITABLE)
        events |= POLLOUT;

    // evport_debug 标记用于控制运行模式
    // 如果 evport_debug 为 true，代表程序运行在 debug 模式下
    // 这里会输出相关的调试信息
    if(evport_debug)
        fprintf(stderr, "%s: port_associate(%d, 0x%x) = ", where, fd, events);
    
    // 查阅了 AI 回复，这里的 mask 参数是一个标签参数，相当于给这个文件描述符+端口配对打了一个标签
    rv = port_associate(portfd, PORT_SOURCE_FD, fd, events, (void*)(uintptr_t)mask);
    err = errno;

    if(evport_debug)
        fprintf(stderr, "%d (%s)\n", rv, rv == 0 ? "no error" : strerror(err));
    
    if(rv == -1)
    {
        fprintf(stderr, "%s port_associate: %s\n", where, strerror(err));
        if(err == EAGAIN)
            fprintf(stderr, "aeApiAssociate: event port limit exceeded.");
    }

    return rv;
}
```
-- -
``` C
static int aeApiAddEvent(aeEventLoop* eventLoop, int fd, int mask)
{
    aeApiState* state = eventLoop->apidata;
    int fullmask, pfd;

    if(evport_debug)
        fprintf(stderr, "aeApiAddEvent: fd %d mask 0x%s\n", fd, mask);
    
    /* 
     * Since port_associate's "events" argument replaces any existing events, we
     * must be sure to include whatever events are already associated when 
     * we call port_associate() again. 
     */
    fullmask = mask | eventLoop->events[fd].mask;
    // 从当前事件循环保存的 Apistate 中寻找 fd 对应的索引
    pfd = asApiLookupPending(state, fd);
    if(pfd != -1)
    {
        // 如果 fd 已经在 pending_fds 中，那么我们只需要修改它的 mask 即可
        if(evport_debug)
            fprintf(stderr, "aeApiAddEvent: adding to pending fd %d\n", fd);
        state->pending_masks[pfd] |= fullmask;
        return 0;
    }

    // 否则，至少 fd 这个文件描述符不是最近才由 aeApiPoll 函数返回的
    // 我们可以假设，即使这个文件描述符上有事件，它也已经被处理完成了
    // 所以这里可以直接调用 aeApiAssociate 函数来关联文件描述符和端口
    return (asApiAssociate("aeApiAddEvent", state->portfd, fd, fullmask));
}
```
- 这个函数用于将一个新的事件和（保存在 ApiState 中的）端口文件描述符进行关联
-- -
``` C
static void aeApiDelEvent(aeEventLoop* eventLoop, int fd, int mask)
{
    aeApiState* state = eventLoop->apidata;
    int fullmask, pfd;

    if(evport_debug)
        fprintf(stderr, "del fd %d mask 0x%x\n", fd, mask);
    
    pfd = aeApiLookupPending(state, fd);

    if(pfd != -1)
    {
        if(evport_debug)
            fprintf(stderr, "deleting event from pending fd %d\n", fd);

        /* 
         * This fd has just returned from aeApiPoll, so it's not currently
         * associated with the port. All we need to do is update 
         * pending_mask appropriately
        */
        // 如果在 ApiState 结构体中发现了这个 fd
        // 那么它就是之前最后一次 aeApiPoll 调用返回的文件描述符之一
        // 这也意味着它没有绑定到任何一个端口上，所以我们只需要对 pending_mask 进行更新即可
        // 不需要进行其它操作
        state->pending_masks[pfd] &= ~mask;

        if(state->pending_masks[pfd] == AE_NONE)
            state->pending_fds[pfd] = -1;
        
        return;
    }
    /*
     * The fd is currently associated with the port. Like with the add case
     * above, we must lock at the full mask for the descriptor before
     * updating that association, we don't have a good way of knowing what the 
     * events are without looking into the eventLoop state directly. We rely on 
     * the fact that our caller has already updated the mask in the eventloop
    */
    fullmask = eventLoop->events[fd].mask;
    if(fullmask == AE_NONE)
    {
        /*
         * We're removing "all" events, so use port_dissociate to remove the 
         * association completely. Failure here indicates a bug
        */
        if(evport_debug)
            fprintf(stderr, "aeApiDelEvent: port_dissociate(%d)\n", fd);
        
        if(port_dissociate(state->portfd, PORT_SOURCE_FD, fd) != 0)
        {
            perror("aeApiDelEvent: port_dissociate");
            abort();
        }
    } else if(aeApiAssociate("aeApiDelEvent", state->portfd, fd, fullmask) != 0)
    {
        abort();
    }
}
```
-- -
### 重点看一下这个核心代码块，它用来封装 port_getn 函数
``` C
static int aeApiPoll(aeEventLoop* eventLoop, struct timeval* tvp)
{
    aeApiState* state = eventLoop->apidata;
    struct timespec timeout, * tsp;
    uint_t mask, i;
    uint_t nevents;
    port_event_t event[MAX_EVENT_BATCHSZ];

    /*
     * If we've returned fd events before, we must re-associate them with the
     * port now, before calling port_get(). See the block comment at the top of 
     * this file for an explanation of why
    */
   for(i = 0; i < state->npending; i++)
    {
        // 如果这个文件描述符已经被删除了，那么也就没必要尝试进行绑定了
        if(state->pending_fds[i] == -1)
            /* This fd has since been deleted */
            continue;
        
        // 尝试重新进行绑定
        if(aeApiAssociate("aeApiPoll", state->portfd, state->pending_fds[i], state->pending_masks[i]) != 0)
        {
            /* See aeApiDelEvent for why this case is fatal */
            abort();
        }

        // 事件端口重新完成了绑定，所以在结构体中不再需要记录这个 fd 和对应的监控事件了，我们进行清空处理
        state->pending_masks[i] = AE_NONE;
        state->pending_fds[i] = -1;
    }

    // 依然是修改相关的记录信息
    state->npending = 0;

    if(tvp != NULL)
    {
        timeout.tv_sec = tvp->tv_sec;
        timeout.tv_nsec = tvp->tv_usec * 1000;
        tsp = &timeout;
    }
    else
    {
        tsp = NULL;
    }

    /* 
     * port_getn can return with errno == ETIME having returned some events (!)
     * So if we get ETIME, we check nevents, too.
     * */
    nevents = 1;
    if(port_getn(state->portfd, event, MAX_EVENT_BATCHSZ, &nevents, tsp) == -1 && (errno != ETIME || nevents == 0))
    {
        if(errno == ETIME || errno == EINTR)
            return 0;
        
        /* Any other error indicates a bug */
        panic("aeApiPoll: port_getn, %s", strerror(errno));
    }
    state->npending = nevent;

    for(i = 0; i < nevents; i++)
    {
        mask = 0;
        if(event[i].portev_events & POLLIN)
            mask |= AE_READABLE;
        if(event[i].portev_events & POLLOUT)
            mask |= AE_WRITABLE;
        
        // 记录一个发生了的事件，注册到事件循环中
        eventLoop->fired[i].fd = event[i].portev_object;
        eventLoop->fired[i].mask = mask;

        if(evport_debug)
            fprintf(stderr, "aeApiPoll: fd %d mask 0x%x\n", (int)event[i].portev_object, mask);
        
        state->pending_fds[i] = event[i].portev_object;
        state->pending_masks[i] = (uintptr_t)event[i].portev_user;
    }

    return nevents;
}

static char* aeApiName(void)
{
    return "evport";
}
```