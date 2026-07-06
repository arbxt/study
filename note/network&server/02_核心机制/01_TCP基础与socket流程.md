# 01 单进程阻塞 TCP 服务器

## 1. 所属层次

```text
网络编程 / Linux 系统调用 / fd / TCP / 项目起点
```

## 2. 最小流程

```text
socket()
setsockopt(SO_REUSEADDR)
bind()
listen()
accept()
read()/recv()
write()/send()
close()
```

这是所有后续服务器版本的基础。即使它只能服务一个客户端，也必须认真整理，因为后续所有复杂模型都没有改变 TCP server 的基本生命周期。

## 3. 阻塞点

```text
accept() 阻塞等待新连接。
read()/recv() 阻塞等待客户端发送数据。
```

单进程阻塞模型的问题是：

```text
一旦服务器阻塞在某个 client_fd 的 read 上，就无法继续 accept 新连接，也无法处理其他客户端。
```

## 4. fd 视角

```text
listen_fd：监听 socket，用于 accept 新连接。
client_fd：连接 socket，用于和某个客户端通信。
```

两者虽然都是整数 fd，但含义不同：

```text
listen_fd 可读：说明有新连接可以 accept。
client_fd 可读：说明有数据、对端关闭或 socket 错误。
```

## 5. 必须掌握

```text
socket / bind / listen / accept 的顺序。
listen_fd 和 client_fd 的区别。
read 返回 0 表示对端关闭。
系统调用失败要检查返回值和 errno。
close 只是关闭当前进程中的 fd。
```

## 6. 常见误区

### 误区一：把 socket API 当成 C++ 语法

socket API 是操作系统接口。C++ 的作用是组织代码、封装资源、管理生命周期。

### 误区二：认为 read 一次就是一条消息

TCP 是字节流协议，没有消息边界。一次 read 可能读到半条消息，也可能读到多条消息。

### 误区三：忽略短写

`write` / `send` 不一定一次写完所有数据。学习阶段 echo 可以简化，但工程中应实现 `write_all` 或写缓冲区。

## 7. 小实验

```text
1. 写一个最小阻塞 echo server。
2. 用 nc 连接，观察 accept 和 read 的阻塞。
3. 同时开两个 nc，观察第二个连接为什么得不到响应。
4. 客户端 Ctrl+C 后观察服务端 read/recv 返回值。
```
