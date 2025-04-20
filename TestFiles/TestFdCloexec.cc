#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

int main(int argc, char* argv[])
{
    // 为文件描述符 1，也就是标准输出设置 CLOEXEC 标志
    int flags = fcntl(STDOUT_FILENO, F_GETFD);
    if(fcntl(STDOUT_FILENO, F_SETFD, flags |= FD_CLOEXEC) == -1)
    {
        perror("fcntl() ERROR:");
        return -1;
    }

    // 执行 ls -l argv[0] 命令
    // 这里 argv[0] 是当前程序的路径名
    // 关闭的文件描述符是没法被 ls 读取或写入的，因为被关闭的文件描述符会拒绝一切文件相关的操作
    printf("Executing ls -l %s\n", argv[0]);
    // 此时 TestFdCloexec 进程的标准输出流已经被关闭了，所以 ls 没法把结果输出到终端中
    execlp("ls", "ls", "-l", argv[0], (char*)NULL);
    return 0;
}