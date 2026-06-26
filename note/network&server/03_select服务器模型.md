# select 服务器模型

## 1. select 解决什么问题

`select` 让一个线程可以同时等待多个 fd 的 I/O 就绪事件。

它解决的问题是：

```text
不要让线程阻塞在某一个客户端的 read 上。
```

而是：

```text
阻塞在 select 上，等待任意 fd 就绪。
```

## 2. select 监听哪些 fd

在 TCP Server 中，通常关心两类 fd：

```text
listen_fd 可读：有新连接可以 accept
client_fd 可读：有数据可读，或对端关闭，或 socket 出错
```

## 3. master_set 与 read_set

典型结构：

```cpp
fd_set master_set;
fd_set read_set;

FD_ZERO(&master_set);
FD_SET(listen_fd, &master_set);

while (true) {
    read_set = master_set;
    int ready_count = select(max_fd + 1, &read_set, nullptr, nullptr, nullptr);
}
```

关键点：

```text
select 会修改传入的 fd_set。
所以 master_set 保存长期监听集合。
read_set 是每轮临时副本。
```

## 4. max_fd 的意义

`select` 的第一个参数是：

```cpp
max_fd + 1
```

含义：

```text
让内核检查 [0, max_fd] 范围内的 fd。
```

所以新增连接时：

```cpp
if (client_fd > max_fd_) {
    max_fd_ = client_fd;
}
```

删除最大 fd 时需要重建：

```cpp
max_fd_ = listen_fd;
for (int fd : client_fds_) {
    max_fd_ = std::max(max_fd_, fd);
}
```

## 5. ready_count 的意义

`select` 返回：

```text
本轮有多少 fd 就绪。
```

但它不会直接告诉你具体是哪几个 fd，需要用：

```cpp
FD_ISSET(fd, &read_set)
```

`ready_count` 可以用于提前结束扫描：

```cpp
--ready_count;
if (ready_count <= 0) break;
```

## 6. 当前项目中的 Selector 流程

目录：

```text
project/server/select_pool
```

核心流程：

```text
read_set_ = master_set_
select(max_fd_ + 1, &read_set_, ...)

if listen_fd ready:
    handle_new_connection()

for client_fd in client_fds_:
    if FD_ISSET(client_fd):
        handle_client_event(client_fd)

for fd in closed_fds_:
    remove_client(fd)
closed_fds_.clear()
```

这是当前项目中最重要的服务器主循环。

## 7. select 的限制

```text
fd_set 有 FD_SETSIZE 限制。
每轮需要从 0 到 max_fd 进行内核检查。
应用层还要遍历 client_fds_ 找就绪 fd。
fd_set 每轮要复制。
```

所以后续会学习 `poll` / `epoll`。

但注意：

```text
poll/epoll 只改变事件通知机制。
不会自动解决 fd 生命周期、任务边界、连接状态管理问题。
```
