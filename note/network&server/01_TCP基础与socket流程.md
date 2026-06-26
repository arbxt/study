# TCP 基础与 socket 流程

## 1. socket API 的层次

`socket` 编程属于：

```text
Linux/POSIX 系统调用
网络编程
操作系统资源管理
```

它不是 C++ 语言本身的内容。C++ 在这里主要负责：

```text
封装资源
组织模块
管理生命周期
处理异常/错误路径
提高可读性和可维护性
```

## 2. TCP Server 最小流程

```cpp
int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
bind(listen_fd, ...);
listen(listen_fd, backlog);

while (true) {
    int client_fd = accept(listen_fd, ...);
    read(client_fd, buffer, sizeof(buffer));
    write(client_fd, buffer, n);
    close(client_fd);
}
```

对应含义：

| API | 作用 |
|---|---|
| `socket` | 创建 socket 内核对象，返回 fd |
| `setsockopt` | 设置 socket 选项，例如端口复用 |
| `bind` | 绑定本地 IP 和端口 |
| `listen` | 进入监听状态 |
| `accept` | 从已完成连接队列取出一个连接 |
| `read/recv` | 从 socket 接收缓冲区拷贝数据到用户态 |
| `write/send` | 从用户态拷贝数据到 socket 发送缓冲区 |
| `close` | 关闭当前进程持有的 fd |

## 3. fd 的准确理解

`fd` 是当前进程 fd 表中的整数索引。

```text
fd -> 进程打开文件表项 -> 内核对象
```

对 socket 来说：

```text
client_fd -> socket 对象 -> TCP 状态 + 接收缓冲区 + 发送缓冲区
```

因此：

```text
fd 不是连接本身。
fd 是当前进程访问连接对应 socket 对象的句柄。
```

## 4. TCP 是字节流

TCP 特点：

```text
面向连接
可靠
有序
字节流
没有消息边界
```

所以应用层必须自己定义消息边界：

```text
固定长度
分隔符，例如 \n
长度字段 + 消息体
HTTP 头部 + body 长度
```

所谓“粘包/拆包”，更准确地说是：

```text
应用层没有正确处理 TCP 字节流的消息边界。
```

## 5. 当前 Echo Server 的简化假设

当前项目中一次 `recv` 后直接 echo：

```cpp
ssize_t bytes_read = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
std::string data(buffer, bytes_read);
```

这对 Echo 学习阶段可以接受，但不是完整协议解析。

后续 HTTP Server 必须引入：

```text
连接读缓冲区
请求解析状态
Content-Length / chunked 处理
半包与多包处理
```
