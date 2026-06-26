# 网络编程与服务器笔记

> 本文件夹用于整理 study 项目中的 Unix/Linux 网络编程、服务器模型、线程池、select、fd 生命周期、测试压测与后续 HTTP Server 演进。

## 阅读顺序

```text
00_网络编程与服务器总笔记.md
01_TCP基础与socket流程.md
02_阻塞服务器与线程池模型.md
03_select服务器模型.md
04_select与threadpool项目审阅.md
05_fd生命周期与连接管理.md
06_ThreadPool实现笔记.md
07_工程技巧与常用写法.md
08_测试压测与调试.md
09_演进路线_poll_epoll_HTTP.md
10_代码审阅记录.md
```

## 命名原则

- 中文为主，方便长期阅读与复习。
- 专业名词保留英文，例如 `TCP`、`socket`、`fd`、`select`、`threadpool`、`Reactor`、`HTTP`。
- 每个文件代表一个可独立维护的章节。

## 当前项目主线

```text
阻塞 TCP Echo Server
    -> 每连接一线程
    -> 线程池阻塞模型
    -> select 单线程 I/O 复用
    -> select + threadpool
    -> poll / epoll
    -> Reactor 风格 HTTP Server
```

## 当前最重要的判断能力

```text
谁负责 accept？
谁负责 read/recv？
谁负责 write/send？
谁负责 close？
谁拥有 client_fd 生命周期？
worker 能不能长期阻塞？
worker 能不能直接操作 socket？
```
