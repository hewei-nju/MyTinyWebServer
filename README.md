# MyTinyWebServer

## 1. 简介

#### 1. 前言

`MyTinyWebServer`是我在学习《TCP/IP网络编程》后，想要做的一个小的尝试！也可以说是我第一次尝试一个还算完整的项目！

课程大作业中其实也都有涉及很多内容了，但是始终没有那种完整自己学习一项东西，从头到尾进行简单的设计，编码，调试的过程，或者说自己心里所期待的获得感。

然后我在认真学习了`tinyhttpd`这个小项目后，觉得它很适合我做这样一个东西！编码量不大，同时完全原生的Linux C编码！最最重要的是它完成了一个HTTP 服务器所应有的内容，虽然不够详尽，但却是了解HTTP Web服务器工作原理的很好的项目！

这个项目大量参考了[tinyhttpd](http://tinyhttpd.sourceforge.net/)与[MyPoorWebServer](https://github.com/forthespada/MyPoorWebServer)，参考的主要内容是：

* 代码框架；
* 实现风格；
* 内在思想；

此外：代码的编写和BUG调试均是基于理解后，个人独立完成的！

其中：有很多的不懂的内容向 [libhv](https://github.com/ithewei/libhv) 社区(群友)请教的，感谢群友们的热心解答！

#### 2. 主要实现的功能

* GET请求和特殊的POST请求的应答；
* 文件上传和下载；
* 条件编译；

## 2. 环境参考

**OS**：CentOS7.2；

**IDE**：Remote Development；

**ssh**：Putty；

**Remote File Transfer**：pscp；

**gcc**：4.8.5；

**g++**：4.8.5；

**gdb**：8.3.1；

**Python**：3.7.3；

## 3. 构建

1. 通过Makefile：
    * `make clean`: 删除编译好的二进制文件；
    * `make all` 或 `make webserver`: 编译webserver.c生成可执行文件；
2. 通过CMakeLists.txt：

```cmake
mkdir build
cd build
cmake ..
cmake --build .
```

## 4. 体验

### 1. 脚本一键启动

运行脚本./start.sh:

```sh
#!/usr/bin/env sh

# 下载编译
git clone https://github.com/hewei-nju/MyTinyWebServer.git
cd MyTinyWebServer
make all

# 默认在8080端口运行httpd服务
echo "Usage: "
echo "1. ./webserver <IP> <Port>"
echo "2. ./webserver <Port>"
./webserver 8080

# 展示可访问的链接
echo "http://localhost:8080"
echo "http://localhost:8080/post.html"
```

### 2. 手动启动



## 5. 工作流程图

![mytinywebserver-01](https://cdn.jsdelivr.net/gh/hewei-nju/PictureBed@main/img/mytinywebserver-01.svg)

**说明**：

* in、out、env均为子进程和父进程传递消息的pipe通道；env专为multipart/form方式编译时，传递换边变量`CONTENT_LENGTH`；
* 上图中黑色箭头均为**单向**箭头；

## 6. 源代码部分解读

### 1. 系统自带宏

```c
// In my computer, glibc's version is 2.17
#define  _GNU_SOURCE
#ifdef __STDC_ALLOC_LIB__
#define __STDC_WANT_LIB_EXT2__ 1
#else
#define _POSIX_C_SOURCE 200809L
#endif
#define __USE_XOPEN2K8
```

目的：

* 使用头文件<stdio.h>中的动态内存函数getline；
* 避免stat函数报错；

### 2. 自定义宏

```c
// Use the Micro with condition compile
#define APPLICATION_X_WWW_URLENCODED 0
#define MULTIPART_FORM_DATA 1
```

**目的**：根据自定义html中post表单数据格式enctype实现条件编译；二者保持统一即可。

其余部分源代码注释很清晰了。

## 7. 项目进行中的一些收获

### 1. 编码风格和注释

这个小项目代码量非常小！

但是由于对http server工作原理的理解不够透彻，以及对于linux c中原生API和函数的使用不够了解，在调试过程中出现了很多烦人的问题！

但是现在想想，似乎也并不困难！只是当时确实没想到！然后这部分内容我都记录在了注释中！我在其中发现：正是因为这些注释和还算ok的编码风格，使得后续的调试相对顺利！

### 2. 调试

调试中，主要的3个问题：

**remote debug**：

* 原先的remote的debug需要gdbserver，现在似乎已经不需要了！(我调试过程中并未用到)
* 远程调试：值得注意的是，路径问题！调试过程中，我是用相对路径，并不能很好的执行总会出现意想不到的问题！使用绝对路径则能很好的解决相关问题！

**multithreading debug**：

* 由于代码初始完成阶段，我并未解决好多线程资源抢占问题，因为在调试时程序会出现“不确定性“！好在CLion的调试器还算智能，可以慢慢发现是多线程资源抢占导致的问题，于是采用了相关方法解决了资源抢占问题！
* 但是对于多线程调试其实我理解的并不多！

**multi-processes debug**：

* [使用CLION调试子进程](https://stackoverflow.com/questions/36221038/how-to-debug-a-forked-child-process-using-clion)

```gdb
set detach-on-fork off
set follow-fork-mode child
```

* [pipes broken问题的可能原因](https://blog.csdn.net/u010419967/article/details/24236873)

### 3. 查API

这里主要是我自己的问题：印象最深刻的是`<stdio.h>`的getline函数；

* 我查完API，仅仅只是看了一下用法，便去使用了，最后导致了几小时的debug！我还一直认为这里没有问题！其实API说的很详细了，只是我并未仔细看，仅仅看了用法便去使用了。

### 4. 需要注意的一些点

* fork后的子进程与父进程不会共享同一环境变量，二者独立；这也是为什么需要一个env pipe来传递环境变量；
* fdopen的FILE *指针，进行fclose操作后，一同关联的fd也被close了；
* http是通过tcp socket来进行数据传输，因此可能发生阻塞！在代码里，我才用了非阻塞的套接字来进行传输！详见源代码和注释；
* python指定执行的解释器：`#!/usr/bin/python`!而我格式化过一次为`# !/usr/bin/python`，从而导致找不到对应的解释器位置，而不能正确执行！
* **源代码还有些bug，运行不够稳定，不具有很好的鲁棒性！不过作为学习项目，是足够了！**

## 8. 参考资料

* 《TCP/IP网络编程》：https://github.com/hewei-nju/TCP-IP-Network-Programming；

* C语言cgi解析上传文件的本地测试：https://www.codenong.com/cs105157913/

* Tinyhttpd精读解析：https://www.cnblogs.com/nengm1988/p/7816618.html；

