# 入手 Redis：从 Redis 服务器源码开始 (server.h, server.c)
- server.h 定义了与 Redis 服务器相关的各项宏参数以及数据结构和方法，这是整个 redis 服务器能运行起来的基础
- 所以，可以认为这部分是我们学习 redis 的最好起点

### 前置 redis 自定义库：
- [fmacros.h](./RedisFmacros.md)
- [config.h](./RedisConfig.md)
- [solarisfixes.h](./RedisSolarisfixes.md)
- [rio.h](./RedisRio.md)
- atomicvar.h
- commands.h
#### 有关上述几个自定义库的学习，参见对应的 markdown 文档

### 其它库为 C 标准或相关扩展库，这里先不做展开
