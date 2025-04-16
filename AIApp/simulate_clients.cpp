#include <iostream>
#include <thread>
#include <vector>
#include <chrono>
#include <hiredis/hiredis.h>

// 发送消息的函数
void sendMessage(int clientId, int numMessages) {
    redisContext* context = redisConnect("127.0.0.1", 6379);
    if (context == NULL || context->err) {
        if (context) {
            std::cerr << "Error: " << context->errstr << std::endl;
            redisFree(context);
        } else {
            std::cerr << "Can't allocate redis context" << std::endl;
        }
        return;
    }
    
    for (int i = 0; i < numMessages; ++i) {
        std::string message = "Client " + std::to_string(clientId) + " Message " + std::to_string(i);
        redisCommand(context, "PUBLISH chat %s", message.c_str());
    }

    redisFree(context);
}

int main() {
    int numClients = 100; // 并发客户端数量
    int numMessages = 2000; // 每个客户端发送的消息数量

    std::vector<std::thread> threads;

    // 记录开始时间
    auto start = std::chrono::high_resolution_clock::now();

    // 创建并启动多个线程，每个线程模拟一个客户端发送消息
    for (int i = 0; i < numClients; ++i) {
        threads.emplace_back(sendMessage, i, numMessages);
    }

    // 等待所有线程完成
    for (auto& thread : threads) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    // 记录结束时间
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> duration = end - start;

    std::cout << "All messages sent in " << duration.count() << " seconds." << std::endl;

    return 0;
}