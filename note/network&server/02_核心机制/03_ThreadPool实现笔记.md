# ThreadPool 实现笔记

> 本文关注 ThreadPool 作为“并发执行器”的职责，不把它和 `accept/select/fd 生命周期` 混在一起。

# 5. 线程池阻塞服务器

## 5.1 为什么需要线程池

每个连接创建一个线程的问题：

```text
连接数上升时线程数无限增长；
线程创建/销毁开销大；
上下文切换变多；
系统资源不可控。
```

线程池解决：

```text
固定 worker 数量；
复用线程；
通过任务队列提交连接处理任务。
```

## 5.2 基本模型

```text
main thread:
    创建 TcpServer
    创建 ThreadPool
    while true:
        client_fd = accept()
        pool.submit([client_fd] {
            handle_client(client_fd);
        })

worker thread:
    从任务队列取任务
    执行 handle_client(client_fd)
```

## 5.3 ThreadPool 核心组成

```cpp
std::vector<std::thread> workers_;
std::queue<std::function<void()>> tasks_;
std::mutex mutex_;
std::condition_variable condition_;
bool stop_;
```

职责：

```text
创建 worker；
维护任务队列；
submit() 添加任务；
worker_loop() 等待并执行任务；
析构时通知停止并 join。
```

## 5.4 关键规则：锁内取任务，锁外执行任务

正确结构：

```cpp
while (true) {
    std::function<void()> task;

    {
        std::unique_lock<std::mutex> lock(mutex_);
        condition_.wait(lock, [this] {
            return stop_ || !tasks_.empty();
        });

        if (stop_ && tasks_.empty()) {
            return;
        }

        task = std::move(tasks_.front());
        tasks_.pop();
    }

    task();
}
```

重点：

```text
锁只保护任务队列。
任务可能执行很久，甚至阻塞 read。
不能持有队列锁执行 task。
```

## 5.5 线程池阻塞模型的局限

如果任务是：

```cpp
void handle_client(int client_fd) {
    while (true) {
        read(client_fd, ...);
        write(client_fd, ...);
    }
    close(client_fd);
}
```

那么一个慢客户端就会长期占用一个 worker。

当连接数超过 worker 数量时：

```text
worker 全部阻塞在 read；
新连接任务排队；
线程池虽然控制了线程数，但并没有解决 I/O 阻塞占用 worker 的问题。
```

这正是后来进入 `select / poll / epoll` 的原因。

---

# 6. 这一篇的核心总结

```text
单进程阻塞：理解最小 TCP 流程。
多进程：理解 fork、fd 表副本、僵尸进程。
多线程：理解线程共享 fd 表、并发输出、参数传递。
线程池：理解任务队列、条件变量、锁内取任务锁外执行。
```

最重要的阶段结论：

```text
线程池能控制线程数量，但如果任务内部仍然阻塞 read，worker 仍然会被慢连接占用。
```

所以后续要引入 I/O 多路复用：

```text
不要让 worker 阻塞等待某个客户端数据；
让一个 I/O 线程统一等待多个 fd 的就绪事件。
```

---

# 7. 自测问题

1. 单进程阻塞服务器有哪两个主要阻塞点？
2. fork 后父进程 close(client_fd)，为什么子进程还能使用该连接？
3. 多线程模型中主线程为什么不能 close(client_fd)？
4. `std::thread(handle, client_fd)` 和传引用有什么区别？
5. 为什么线程池 worker 不能持有任务队列锁执行 task？
6. 线程池为什么仍然不能很好处理大量慢连接？
7. 从线程池阻塞模型进入 I/O 多路复用，真正要解决的是什么？

---

# 8. 推荐实验

```text
实验 1：写单进程阻塞 echo server。
实验 2：改成 fork 版本，并观察子进程退出。
实验 3：故意不 wait，观察僵尸进程。
实验 4：改成 std::thread 版本，验证主线程提前 close 的错误。
实验 5：实现 ThreadPool，验证 worker 数量固定。
实验 6：用 slow client 占住 worker，观察线程池阻塞模型的局限。
```
