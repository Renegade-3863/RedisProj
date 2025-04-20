# Redis fmacros.h 头文件阅读

#### 这个头文件没有太多与实际的实现相关的代码，更多的是 #define 宏定义，所以这里不做过于复杂的介绍
- 简单来说，这个头文件定义了一组规则，用于实现跨平台的服务器运行
- 以下面的宏定义为例：
``` C
#if defined(__linux__)
#define _GNU_SOURCE
#define _DEFALUT_SOURCE
#endif
```
- 上面的宏定义表示，如果当前的操作系统是 Linux，那么就定义 _GNU_SOURCE 和 _DEFALUT_SOURCE 这两个宏
- 还有一些其它类似的宏定义，这里不再展开

-- -

### 一个有趣的 attribute 宏定义：
``` C
/** 
 * 弃用不安全的 (deprecated) 函数
 * 
 * 注意：我们不使用 poison pragma 编译选项，因为它会在文件中的 stdlib 定义上出错
*/
#if (__GNUC__ && __GNUC__ >= 4) && !defined __APPLE__
int sprintf(char* str, const char* format, ...) __attribute__((deprecated("please avoid use of unsafe C functions. prefer use of snprintf instead")));
// 剩下两种函数的处理不再重写。。。
#endif
```
**解释一下上面 redis 官方给出的注释：**
- 为什么不使用 pragma poison 编译选项？
    - ``` C
      #pragma GCC posion sprintf strcpy strcat
      ```
    - 类似这样的代码块会禁用拥有 sprintf, strcpy, strcat 这三个函数名的库函数，在检测到代码内部使用了这几种函数名的时候，程序会报错从而无法编译通过
    - 而 redis 实际上用到了 stdlib 库，所以为了避免上面的编译报错，官方没有使用这个编译选项，而是选择了自己对过时的这几种函数进行 deprecated 信息处理

**简单分析一下列出的第一个 deprecated attribute** :
- 如果不熟悉 attribute 宏，这里简单说明一下：
    - **attribute 宏是 C 语言的一个扩展，用于为函数、变量、类型等添加额外的属性**
- 上面的代码块中，attribute 宏的作用是为 sprintf 函数添加一个 deprecated 属性，这个属性的含义是：这个函数是不安全的，不建议使用，而是应该使用 snprintf 函数代替
    - 注意：这个属性检查的是严格拥有对应函数签名的函数，对于重载的其它函数（尽管 C 语言本身并不支持重载，写到这里突然想到 C++ 里这个宏是否只会 deprecate 这一种函数签名的函数）
    - 经测试 **(测试代码参考 /TestFiles/TestDeprName.cc)** ，这个宏只会在同时定义和使用了它处理的函数的时候才会在编译时发出警告，**单独对函数进行定义并不会在编译时发出警告**

Redis 还 deprecate 了 strcpy 函数和 strcat 函数
- 练习：尝试编写 deprecate 这两个函数的宏定义：
``` C
char* strcpy(char* restrict dest, const char* src) __attribute__((deprecated("please avoid use of unsafe C functions. prefer use of redis_strlcpy instead")))
```
``` C
char* strcat(char* restrict dest, const char* restrict src) __attribute__((deprecated("please avoid use of unsafe C functions. prefer use of redis_strlcat instead")))
```
-- -
### 其它内容
- redis 在这里引入了 linux 自带的 features.h 头文件来处理其它在 linux 平台上可能有用的兼容性控制 


### 编写文档时使用：回到 [RedisServer.md](./RedisServer.md)
### 编写文档是使用：回到 [RedisMonotonic.md](./RedisMonotonic.md)