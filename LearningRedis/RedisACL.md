# 浅析 Redis 的 ACL (Access Control List) 机制

#### 前置1：源码中的实现代码块：acl.c
#### 前置2：使用的相关头文件：server.h, cluster.h, sha256.h, fcntl.h, ctype.h
- 有关这几个前置库的学习，参见对应的 markdown 文档
