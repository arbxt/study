# Selector 与 Reactor 边界

## 1. 为什么需要 Selector

在线程池阻塞模型中，worker 可能长期阻塞在某个 `client_fd` 的 `read` 上。进入 I/O 多路复用后，需要一个专门的 I/O 线程统一等待 fd 就绪事件。

```text
Selector / I/O 线程：
    等待 fd 就绪
    accept 新连接
    recv/read 数据
    判断关闭
    维护 fd 集合
    提交业务任务
```

```text
ThreadPool：
    不负责 accept
    不负责 select/poll/epoll_wait
    不负责 fd 集合
    不负责 client_fd 生命周期
    只处理已经读到的数据或较耗时业务
```

## 2. 当前阶段设计口诀

```text
I/O 状态归 Selector
业务耗时归 Pool
资源生命周期归创建者
main 只负责组装
```

## 3. 当前简化模型与标准 Reactor 的差别

当前学习阶段可能采用：

```text
Selector: recv 数据 -> submit 给 pool
Worker: 处理业务 -> 直接 write(fd)
```

这个模型能帮助理解 I/O 与业务分离，但不是最终工程形态。风险包括：

```text
worker 写 fd 时，fd 可能已被 Selector 关闭；
fd 数字可能被系统复用；
多个 worker 可能并发 write 同一个 fd；
write 本身也可能阻塞。
```

更标准的 Reactor 方向是：

```text
I/O 线程：负责 socket read/write，维护读缓冲和写缓冲。
ThreadPool：只处理业务，生成 response，把 response 放回连接写缓冲。
```

## 4. 模块职责参考

# 08 模块职责与工程边界

## 1. 判断什么时候设计为类

适合类化的标准：

```text
有状态。
围绕状态有一组操作。
需要初始化 / 析构资源。
存在状态一致性规则。
```

## 2. TcpServer

职责：

```text
socket
setsockopt
bind
listen
accept
管理 listen_fd 生命周期
```

不负责：

```text
线程池
业务处理
select
client_fds_ 集合
```

推荐接口：

```cpp
bool start();
int accept_client();
int listen_fd() const;
```

## 3. ThreadPool

职责：

```text
管理 worker 线程。
管理任务队列。
提供 submit()。
析构时停止并 join。
```

不负责：

```text
accept
select
fd_set
client_fd 生命周期
read/write 细节
```

## 4. Selector

职责：

```text
管理 select loop。
管理 fd 集合。
维护 max_fd_。
处理 listen_fd/client_fd 可读事件。
recv 数据。
判断连接关闭。
调用 pool.submit()。
移除 client_fd。
```

Selector 持有：

```cpp
TcpServer& tcp_server_;
ThreadPool& pool_;
```

原因：

```text
Selector 运行过程中需要调用 server.accept_client()。
Selector 运行过程中需要调用 pool.submit()。
```

## 5. BusinessHandler

当前阶段应保持轻量：

```text
输入：client_fd + 已读到的 data
输出：response 或直接 echo
不 read
不 close
```

## 6. 设计口诀

```text
I/O 状态归 Selector
业务耗时归 Pool
资源生命周期归创建者
main 只负责组装
```

另一个判断方法：

```text
1. 先画数据流
2. 找事件入口
3. 找资源归属
4. 找阻塞点
5. 决定谁调谁
6. 再设计模块边界
```
