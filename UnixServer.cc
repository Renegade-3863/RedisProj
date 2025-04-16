#include <iostream>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#define SOCKET_PATH "/tmp/test_socket"

int main() {
    int server_fd, client_fd;
    struct sockaddr_un server_addr;

    // 创建套接字
    server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (server_fd < 0) {
        std::cerr << "Socket creation failed" << std::endl;
        return 1;
    }

    // 设置服务器地址
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sun_family = AF_UNIX;
    strncpy(server_addr.sun_path, SOCKET_PATH, sizeof(server_addr.sun_path) - 1);

    // 绑定套接字
    unlink(SOCKET_PATH);
    if (bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        std::cerr << "Socket bind failed" << std::endl;
        close(server_fd);
        return 1;
    }

    // 监听连接
    if (listen(server_fd, 5) < 0) {
        std::cerr << "Socket listen failed" << std::endl;
        close(server_fd);
        return 1;
    }

    std::cout << "Server is listening on " << SOCKET_PATH << std::endl;

    // 接受客户端连接
    client_fd = accept(server_fd, NULL, NULL);
    if (client_fd < 0) {
        std::cerr << "Socket accept failed" << std::endl;
        close(server_fd);
        return 1;
    }

    std::cout << "Client connected" << std::endl;

    // 接收数据
    char buffer[256];
    int bytes_read = read(client_fd, buffer, sizeof(buffer) - 1);
    if (bytes_read > 0) {
        buffer[bytes_read] = '\0';
        std::cout << "Received: " << buffer << std::endl;
    }

    // 关闭连接
    close(client_fd);
    close(server_fd);
    unlink(SOCKET_PATH);

    return 0;
}