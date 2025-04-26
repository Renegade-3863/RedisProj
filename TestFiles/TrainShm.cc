#include <sys/ipc.h>
#include <sys/shm.h>
#include <cstring>
#include <unistd.h> 
#include <iostream>

int main()
{
    // 思考：如何使用共享内存进行进程间通信
    // 首先，我们需要根据一个 key 值来创建一个共享内存段
    // 这个 key 值应该是一个随机的整数
    // 常见的生成随机整数的方法是使用 ftok 函数
    // 根据 man 文档，这个函数生成的随机 key 值，只要第二个参数是一样的，那么生成的 key 值就是一样的
    int key = ftok("shmfile", 65);
    // 用 key 值创建一个共享内存段
    // 这个函数的第二个参数是共享内存的大小，这里我们设置为 1024 字节，也就是 1KB
    // 第三个参数是共享内存的权限，这里我们设置为 0666，也就是任何人都可以读写这个共享内存段
    // IPC_CREAT 表示如果共享内存段原先不存在，那么我们就新建一个内存段来使用
    // 注意：这个返回的 shmid 不是文件描述符，而是一个在操作系统范围内唯一的 “共享内存标识符”
    // 我们可以使用这个标识符来操作共享内存段
    int shmid = shmget(key, 1024, 0666 | IPC_CREAT);
    // 将共享内存段附加到当前进程
    // 说是附加，其实这个函数的真正作用是，把操作系统中的这个共享内存块（上面定义的这个大小为 1024 字节的内存块）
    // 映射到当前进程的虚拟地址空间中
    // 这里第二个参数就是要映射到的进程内虚拟地址的开头，第三个参数可以用来设置进程对这段内存地址的一些操作权限
    // 例如 SHM_RDONLY 用于标识这段内存地址对于这个进程只读
    char* str = (char*)shmat(shmid, (char*)NULL, 0);
    // 之后就可以尝试对 str 进行写入，写入给 str 的内容就会保存到 shmid 对应的共享内存段中
    std::cout << "Please enter the content you want to deliver: ";
    std::cin.getline(str, 1024);
    // 到这里，读取完成，我们可以解绑当前进程和这个共享内存段
    shmdt(str);

    // 这里，我们新建一个子进程，让这个子进程也绑定到 shmid 对应的这个共享内存段上，然后读取刚刚写入到共享内存段上的这段数据
    if(fork() == 0)
    {
        // 如果这个 context 是子进程，那么我们就可以对 shmid 进行绑定和读取了
        char* str = (char*)shmat(shmid, (char*)NULL, 0);
        
        // 输出可视化结果
        std::cout << "Contents delivered through shared memory: " << str << std::endl;

        // 输出完成，可以解绑 shmid
        shmdt(str);

        // 但是共享内存还没有被删除，为了让它自动删除，需要使用 shmctl 函数进行设定
        shmctl(shmid, IPC_RMID, NULL);
    }
    return 0;
}