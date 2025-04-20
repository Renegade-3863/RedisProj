# solarisfixes.h 文件解析
## 根头文件之一，定义了一些和 Solaris 操作系统相关的

- 基本有两个编译指令可以学习
    - __extension\_\_
        - 这个指令是用在 GCC 编译器中的 （GCC 特有）用来抑制编译器对某些扩展的语法的警告信息
    - __builtin_expect()
        - 这个指令同样用于 GCC 编译器，它用于帮助编译器进行分支预测优化
        - 基本语法是：__builtin_expect(EXP, N)
        - 其中 EXP 是一个表达式，N 是一个常量
        - 这告诉编译器 EXP 的结果很可能是 N，这样编译器就可以根据这个信息进行优化

### 编写文档时使用：回到 [RedisServer.md](./RedisServer.md)