#include <hiredis/hiredis.h>
#include <sw/redis++/redis++.h>
#include <stdio.h>
#include <stdlib.h>

int main() {
    // 链接到Redis服务器，IP 为本机回环地址，端口为6379，6379 是 redis 的默认端口
    // 回环地址的意思是，网络通信的源和目标地址都是本机，这样就可以在本机上测试网络通信是否正常，通常用来检查本机的网卡等硬件设备是否正常工作
    redisContext *c = redisConnect("127.0.0.1", 6379);
    // 如果链接失败，则打印错误信息并退出程序
    if (c == NULL || c->err) {
        if (c) {
            printf("Error: %s\n", c->errstr);
            redisFree(c);
        } else {
            printf("Can't allocate redis context\n");
        }
        // exit c 库函数用于终止程序的执行，参数 1 表示程序执行失败，参数 0 表示程序执行成功
        exit(1);
    }

    redisReply *reply = (redisReply *)redisCommand(c, "SET %s %s", "foo", "hello world");
    printf("SET: %s\n", reply->str);
    freeReplyObject(reply);

    reply = (redisReply *)redisCommand(c, "GET %s", "foo");
    printf("GET foo: %s\n", reply->str);
    freeReplyObject(reply);

    redisFree(c);
    return 0;
}