#include <iostream>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <cstring>
#include <unistd.h>
#include <stdio.h>

int main()
{
    // 创建一个共享内存段
    // 生成一个唯一的键
    // 确保 shmfile 文件存在，我们需要使用 fopen 来新建这个文件
    FILE* file = fopen("shmfile", "w");
    if(file == NULL)
    {
        perror("无法创建 shmfile 文件");
        return 1;
    }
    key_t key = ftok("shmfile", 23);
    printf("%d", key);
    // 通过唯一键创建新的共享内存，ID 为 shmid
    int shmid = shmget(key, 1024, 0666 | IPC_CREAT);

    // 将共享内存附加到当前进程
    char* str = (char*)shmat(shmid, (void*)0, 0);

    // 写入数据到共享内存
    std::cout << "写入数据到共享内存: ";
    std::cin.getline(str, 1024);

    // 分离共享内存
    shmdt(str);

    // 读取数据
    // fork 一个子进程
    if(fork() == 0)
    {
        // 将共享内存附加到子进程
        str = (char*)shmat(shmid, (void*)0, 0);
        // 读取共享内存中的数据
        std::cout << "从共享内存读取的数据: " << str << std::endl;

        // 分离共享内存
        shmdt(str);

        // 标记共享内存段的状态，IPC_RMID 告诉内核，当没有进程附加时，就删除 shmid 代表的这个共享内存段
        shmctl(shmid, IPC_RMID, NULL);
    }
    return 0;
}