#include <iostream>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#define SOCKET_PATH "/tmp/test_socket"

int main() {
    int client_fd;
    struct sockaddr_un server_addr;

    // 创建套接字
    client_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (client_fd < 0) {
        std::cerr << "Socket creation failed" << std::endl;
        return 1;
    }

    // 设置服务器地址
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sun_family = AF_UNIX;
    strncpy(server_addr.sun_path, SOCKET_PATH, sizeof(server_addr.sun_path) - 1);

    // 连接服务器
    if (connect(client_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        std::cerr << "Socket connect failed" << std::endl;
        close(client_fd);
        return 1;
    }

    std::cout << "Connected to server" << std::endl;

    // 发送数据
    const char* message = "Hello, Server!";
    write(client_fd, message, strlen(message));

    // 关闭连接
    close(client_fd);

    return 0;
}