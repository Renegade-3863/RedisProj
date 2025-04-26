# TCP Nagle 算法详解
### 这里的英文文本引用来自 RFC (Request For Comments) 官方文档
- RFC 文档来源：Congestion Control in IP/TCP Internetworks (RFC 896) [rfc896.html](http:rfc-editor.org/rfc/rfc896.html)

``` txt
This memo discusses some aspects of congestion control in IP/TCP internetworks. It is intended to stimulate thought and further discussion of this topic. While some specific suggestions are made for improved congestion control implementation, this memo does not specify any stadards.
```
- #### 序言，没什么有用的点，点明了这个文档是讨论 TCP 拥塞控制的，本文的前 60% 内容是讨论 Nagle 算法的，剩下的内容讨论的是基于 ICMP 的源抑制拥塞控制算法

``` txt
Introduction

Congestion control is a recognized problem in complex networks. We have discovered that the Department of Defense's Internet Protocol (IP), a pure datagram protocol, and Transmission Control Protocol (TCP), a transport layer protocol, when used together, are subject to unusual congestion problems caused by interactions between the transport and datagram layers. In particular, IP gateways are vulnerable to a phenomenon we call "congestion collapse", especially when such gateways connect gateways connect networks of widely different bandwidth. We have developed solutions that prevent congestion collapse.
```
- #### 背景信息介绍，引出了 TCP 拥塞控制的问题

``` txt
These problems are not generally recognized because these protocols are used most often on networks built on top of ARPANET TMP technology. ARPANET IMP based networks traditionally have uniform bandwidth and identical switching nodes, and are sized with substantial excess capacity. This excess capacity, and the ability of the IMP system to throttle the trnsmissions of hosts has for most IP/TCP hosts and networks been adquate to handle congeston. With the recent split of the ARPANET into two inter-connected networks and the growth of other networks with differing properties connected to the ARPANET, however, reliance on the benigh properties of the IMP system is no longer enough to allow hosts to communicate rapidly and reliably. Improved handling of congestion is now mandatory for successful network operation under load.
```

- #### 依旧是背景信息介绍

``` txt
Congestion Collapse

In heavliy loaded pure datagram networks with end to end retransmission, as switching nodes become congested, the round trip time through the net increases and the count of datagrams in transit within the net also increses. This is normal behavior under load. AS long as there is only one copy of each datagram in transit, congestion is under control. Once retransmission of datagrams not yet delivered begins, there is potential for serious trouble.

备注：Round Trip Time （RTT） 就是从发送端传递一个数据包到接收端，再从接收端返回一个确认 ACK 数据包所需的时间，这是网络性能的一个重要指标

Host TCP implementations are expected to retransmit packets several times at increasing time intervals until some upper limit on the retransmit interval is reached. Normally, this mechanism is enough to prevent serious congestion problems. Even with the better adaptive host retransmission algorithms, though, a sudden load on the net can cause the round-trip time to rise faster than the sending hosts measurements of round-trip time can be updated. Such a load occurs whtn a new bulk transfer, such a file transfer, begins and starts filling a large window. Should the round-trip time exceed the maximum retransmission interval for any host, that host will begin to introduce more and more copies of the same datagrams into the net. The network is now in serious trouble. Eventually all available buffers in the switching nodes will be full and packets must be dropped. The round-trip time for packets that are delivered is now at its maximum. Hosts are sending each packet several times, and eventually some copy of each packet arrives at its destination. This is congestion collapse.
```
- #### 这一段主要提及了拥塞崩溃的定义。本质上，这个问题就是在说：
    - 如果某个主机的最大重传间隔低于了它到接收主机之间的 RTT（数据包往返时间），那么接收主机的缓存就会逐渐被这个发送主机重传的数据包填满，最终导致整个网络的性能下降

``` txt
Adding additional memory to the gateways will not solve the problem. The more memory added, the longer round-trip time must become before packets are dropped. Thus, the onset of congestion collapse will be delayed but when collapse occurs an even larger fraction of the packets in the net will be duplicates and throughput will be even worse.
```
- #### 添加更多的网管内存并不难解决问题，这个问题的本质并不在于缓存容量的大小，它是 RTT 和重传间隔之间的关系
    - 增大缓存容量只能延缓拥塞崩溃的发生，但是这会导致发生拥塞崩溃时的 RTT 更大，网络传输效率反而更低

``` txt
终于到了正题。。。

The two problems

Two key problems with the engineering of TCP implementations have been observed; we call these the small-packet problem and the source-quench problem. The second is being addressed by several implementors; the first is generally beliebed (incorrectly) to be solved. We have discovered that once the small-packet problem has been solved, the source-quench problem becomes much more tractable. We thus present the small-packet problem and our solution to it first.

Source Quench problem：源抑制问题
```

``` txt
The small-packet problem (小数据包问题)

There is a special problem associated with small packets. When TCP is used for the transmission of single-character messages originating at a keyboard, the typical result is that 41 byte packets (one byte of data, 40 bytes of header) are transmitted for each byte of useful data. This 4000% overhead is annoying but tolerable on lightly loaded networks. On heavily loaded networks, however, the congestion resulting from this overhead can result in lost datagrams and retransmissions, as well as excessive propagation time caused by congestion in switching nodes and gateways. In practice, throughput may drop so low that TCP connections are aborted.
```
- #### 这一段主要是想强调传统 TCP 在传输少于 40 字节（TCP 头部长度）的有效数据时造成的过大 overhead
    - 这在网络畅通时可能可以忍受
    - 但是在网络负载较大时，这个 overhead 可能会直接导致拥塞崩溃问题

``` txt
This classic problem is well-known and was first addressed in the Tymnet network in the late 1960s. The solution used there was to impose a limit on the count of datagrams generated per unit time. This limit was enforced by delaying transmission of small packets until a short (200-500ms) time had elapsed, in hope that another character or two would become available for addition to the same packet before the timer ran out. An additional feature to enhance user accepability was to inhibit the time delay when a control character, such a carriage return, was received.
```

- #### 讲了 Tymnet 网络解决这个问题的一种方案：
    - 在一定的时间间隔内，限制发送的数据包的数量，这是通过延迟发送小数据包来实现的
    - 但是作者提到：
        - 这种方案的普适性较差，很难决定一个普遍适用的延迟时间间隔，同时满足大 RTT （例如 5 秒 RTT 的网络）和小 RTT 的网络（例如 10Mbps 的 Ethernet 网络）的需求

``` txt
The solution is to inhibit the sending of new TCP segments when new outgoing data arrives from the user if any previously transmitted data on the connection reamins unacknowledged. This inhibition is unconditional; no timers, tests for size of data received, or other conditions are required. 
```

- #### 思想：当一个新的 TCP 数据段从用户到达发送缓存时（注意，还未发送），如果任何先前发送的数据段还未被确认（unacknowledged），那么就无条件抑制 （inhibit）这个新数据段的发送

``` txt
At first glance, this solution seems to imply drastic changes in the behavior of TCP. This is not so. It all works out right in the end. Let us see why this is so.

When a user process writes to a TCP connection, TCP receives some data. It may hold that data for future sending or may send a packet immediately. If it refreains from sending now, it will typically send the data later when an incoming packet arrives and changes the state of the system.

上面这一段结合了一些 TCP 的特性：
    首先，TCP 在收到来自用户的数据时，可能会先保留这些数据 （在发送窗口中）等待后续发送，也可能会立即发送这些数据，如果它不立即发送，那么可能会在后续某些数据抵达本机后发生了状态变化时再发送这些数据。
    有两种可能的状态变化情况：
        1. 来自远端主机的 ACK 确认了一段数据已经抵达该远端主机
        2. 定时传输的控制信息告知本机远端主机有缓存空间可以接收新数据

Each time data arrives on a connection, TCP must reexamine its current state and perhaps send some packets out. Thus when we omit sending data on arrival from the user, we are simply deferring its transmission until the next message arrives from the distant host. A message must always arrive soon unless the connection was previously idle or communications with the other end have been lost. In he first case, the idle connection, our scheme will result in a packet being sent whenever the user writes to the TCP connection. Thus we do not deadlock in the idle condition. In the second case, where the distant host has failed, sending more data is futile anyway. Note that we have done nothing to inhibit normal TCP retransmission logic, so lost messages are not a problem.

```

- 这一段论述了作者提出的解决方案并没有过多影响到 TCP 原有思想的观点
- 作者认为：原本的 TCP 架构也 **基本上都是当发送端接收到来自对端响应数据后才会发送新的数据**，<u>除非原本网络链路就是空闲的，或者连接已经丢失</u>。所以这里抑制新数据发送的方案其实并没有对 TCP 原本的架构产生很大的影响，至于上面提到的两种例外情况，作者写到自己会特殊处理这两种情况下的数据传输。

``` txt
Examination of the behavior of this scheme under various conditions demonstrates that the scheme does work in all cases. The first case to examine is the one we wanted to solve, that of the character-oriented Telnet connection. Let us suppose that the user is sending TCP a new character every 200 ms, and that the connection is via an Ethernet with a round-trip time including software processing of 50ms. Without any mechanism to prevent small-packet congestion, one packet will be sent for each character, and response will be optimal. Overhead will be 4000%, but this is acceptable on an Ethernet. The classic timer scheme, with a limit of 2 packets per second, will cause two or three characters to be sent per packet. Response will thus be degraded even though on a high-bandwidth Ethernet this is unnecessary. Overhead will drop to 1500%, but on an Ethernet this is a bad tradeoff. With our scheme, every character the user types will find TCP with an idle connection, and the character will be sent at once, just at in the no-control case. The user will see no visible delay. Thus our scheme performs as well as the no-control scheme and provides better responsiveness than the timer scheme.
```
- #### 这段主要是在对比在 Telnet 这种 "输入一个字符，TCP 就尝试发送一个字符" 的 "字符导向 TCP 网络" 下，之前 Tymnet 的计时器解决方案和自己提出的 Nagle 方案之间的效率差异
- #### 个人认为，如果按照这里写明的逻辑，那么 Nagle 算法在连接空闲的时候，效率和不用 Nagle 算法的 TCP 是完全一致的，而之前的计时器算法反而会导致出现延迟（给定题目中的高速 Ethernet）

``` txt
The second case to examine is the same Telnet test but over a long-haul link with a 5-second round trip time. Without any mechanism to prevent small-packet congestion, 25 new packets would be sent in 5 seconds. Overhead here is 4000%. With the classic timer scheme, and the same limit of 2 packets per second, there would still be 10 packets outstanding and contributing the congestion.

这里依然用 Telnet 为例，测试计时器方案和 Nagle 方案在长 RTT 网络（例：5s RTT）上的性能差异
Simple Math：
    5 秒 RTT，在接收端的 ACK 传回来之前，计时器方案（假设抑制源为每秒最多两个数据包）会发送 10 个（可能导致拥塞）的数据包，而这些数据包同样有可能是 overhead 很高的

Round-Trip time will not be improved by sending many packets, of course; in general it will be worse since the packets will contend for line time. Overhead now drops to 1500%.

思考一下这个 1500% 的来历
个人认为，可以这样计算：
使用计时器算法的话，上面的 10 个数据包，平均每一个包内就包含 25 / 10 = 2.5 个字符（字节），那么就意味着，每个包的 overhead 为 40/2.5 = 1600%，这个数值是接近上文提出的 1500% 的

With our scheme, however, the first character from the user would find an idle TCP connection and would be sent immediately. The next 24 characters, arriving from the user at 200ms intervals, would be held pending a message from the distant host. When an ACK arrived for the first packet at the end of 5 seconds, a single packet with the 25 queued characters would be sent. Our scheme thus results in an overhead reduction to 320% with no penalty in response time.

这里 320% 的由来：
一共发送了 105 个字符（字节），其中 80 个字节是 TCP 控制头（overhead），只有 25 个字节是有效字符，所以 overhead 率是 80/25 = 320%，原本不用任何控制机制的 TCP 连接也在这样的时间间隔内发送了 25 个有效字符，但是引入了 4000%，也就是 1000 个字节的 overhead，相比之下，Nagle 算法的确做出了很大的优化。

Response time will usually be improved with our scheme because packet overhead is reduced, here by a factor of 4.7 over the classic timer scheme. 

这里 4.7 的由来：
作者假设的 timer 架构的 overhead 率：1500% / Nagle 算法的 overhead 率：320% = 4.7 （round）

Congestion will be reduced by this factor and round-trip delay will decrease sharply. For this case, our scheme has a striking advantage over either of the other approaches.
```
- #### 上面两段主要讨论了 Nagle 算法和传统的 timer 算法在 Telnet 这种 character-oriented 的网络上的表现性能

``` txt
We use our scheme for all TCP connections, not just Telnet connections. Let us see what happens for a file transfer data connection using our technique. The two extreme cases will again be considered.

As before, we first consider the Ethernet case. The user is now writing data to TCP 512 byte blocks as fast as TCP will accept them. The user's first write to TCP will start things going; our first datagram will be 512+40 bytes or 552 bytes long. The user's second write to TCP will not cause a send but will cause the block to be buffered. Assume that the user fills up TCP's outgoing buffer area before the first ACK comes back. Then when the ACK comes in, all queued data up to the window size will be sent. From then on, the window will be kept full, as each ACK initiates a sending cycle and queued data is sent out. Thus, after a one round-trip time initial period when only one block is sent, our scheme settles down into a maximum-throughput condition. The delay in startup is only 50ms on the Ethernet, so the startup transient is insignificant. All three schemes provide equivalent performance for this case. 

这里说实话没看懂（），先看下后面的内容

Finally, let us look at a file transfer over the 5-second round trip time connection. Again, only one packet will be sent until the first ACK comes back; the window will then be filled and kept full. Since the round-trip time is 5 seconds, only 512 bytes of data are transmitted in the first 5 seconds. Assuming a 2K window, once the first ACK comes in, 2K of data will be sent and a steady rate of 2K per 5 seconds will be maintained thereafter. Only for this case is our scheme inferior to the timer scheme, and the difference is only in the startup transient; steady-state throughput is identical. The naive scheme and the timer scheme would both take 250 seconds to transmit a 100K byte file under the above conditions and our scheme would take 254 seconds, a difference of 1.6%.

这里好像有点难懂，不过仔细思考一下，说的也没问题：
因为窗口最大就 2K，那么 5 秒，用户理论上可以写 2.5 K 的数据到发送窗口，而发送窗口只能承接 2K 的数据，所以总的来说，还是每秒发送 2K 的数据，从这一点上来讲，ACK-triggered 的 Nagle 算法在发送窗口总是满的情况下，稳态效率和计时器算法确实是一样的
补充解释一下：
计时器算法的方案是等待一个较短的时间间隔再发送，但是这种方案基本只适用于小数据包，对于此时这种 512 字节的较大数据包，timer 算法可以认为和原来的 TCP 结构完全一致
```
- #### 以上是有关 Nagle 算法在 Telnet 这种 character-oriented 的网络，以及在常规的长数据报 Ethernet 上的性能分析
-- -
RFC 896 中剩下部分的内容讨论的是基于 ICMP 协议的源抑制算法，不再是 Nagle 算法的内容，这里不再深入探讨，有兴趣的可以自行研究

-- - 
总结一下个人对 Nagle 算法的理解：
- 首先，这是一个还算优秀的拥塞控制辅助机制，但并不完美
- 个人认为的关键 defect 在于：
    - 这个控制机制是基于接收端的 ACK 的，所以接收端的 ACK 传输的效率直接决定的发送端发送数据的效率
    - 这里引用知乎用户：Java 有关 Nagle 算法讨论的文章 [网络编程中Nagle算法和Delayed ACK的测试](https://www.zhihu.com/column/p/27039961) 中提到的可能情况
        - 某些 TCP 实现采用 <u>write-write-read</u> 机制，也就是 head 数据和 body 数据**分开传输**，发送端采用 Nagle 算法，那么在发送完 head 之后，发送端会抑制对于 body 的传输（它认为这是两段数据），而接收端在收到了 head 数据之后，认为这个包不完整，故并不会返回 ACK 报文，从而导致发送端和接收端都在等待，出现类似 deadlock 的情况
        - Delayed ACK 是另一种解决拥塞问题的方案，它允许接收端用小于 1个ACK/1个接收数据段 的速率来返回 ACK
            - 在这种机制下，一个实现了 Nagle 算法的发送端和一个实现了 Delayed ACK 的接收端可以引发很尴尬的后果（）