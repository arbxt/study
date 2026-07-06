# I/O 多路复用：select、poll 与 epoll

> 本篇目标：整理从 `select` 到 `poll`、`epoll` 的 I/O 多路复用主线。  
> 关注点：fd 就绪事件、事件循环、连接生命周期、I/O 线程与 ThreadPool 边界。  
> 暂不涉及 HTTP。

---

# 1. 为什么从线程池进入 I/O 多路复用

线程池阻塞模型的问题是：

```text
一个连接作为一个任务提交给 worker；
worker 在 handle_client 中 while(read)；
慢客户端会长期占用 worker；
worker 数量耗尽后，新任务只能排队。
```

I/O 多路复用的目标是：

```text
不要让多个 worker 分别阻塞在多个 client_fd 上；
而是让一个 I/O 线程阻塞在 select/poll/epoll 上，统一等待多个 fd 的就绪事件。
```

核心变化：

```text
从：每个连接占用一个执行流等待数据
到：一个事件循环管理多个连接的就绪状态
```

---

# 2. I/O 多路复用的基本概念

## 2.1 fd 就绪不是“已经处理完”

当一个 fd 被判定为可读，只表示：

```text
现在对它执行 read/recv 大概率不会阻塞；
可能读到数据；
可能读到 0，表示对端关闭；
也可能读出错误。
```

因此事件循环仍然要处理：

```text
read/recv 返回值；
errno；
连接关闭；
fd 集合更新；
业务任务提交。
```

## 2.2 listen_fd 与 client_fd 的可读含义不同

```text
listen_fd 可读：
    有新连接可以 accept。

client_fd 可读：
    有数据可读；
    或对端关闭；
    或 socket 出错。
```

## 2.3 I/O 多路复用不自动解决业务并发

`select/poll/epoll` 解决的是：

```text
如何等待多个 fd 的 I/O 事件。
```

它不直接解决：

```text
业务计算是否耗时；
多个响应如何排队；
worker 是否能直接 write；
连接状态如何保存；
协议消息边界如何解析。
```

---

# 3. select 模型

## 3.1 核心结构

```cpp
fd_set master_set;
fd_set read_set;
int max_fd;
std::vector<int> client_fds;
```

含义：

```text
master_set：长期监听集合。
read_set：本轮传给 select 的临时集合。
max_fd：select 需要的最大 fd。
client_fds：用于遍历所有客户端 fd。
```

## 3.2 基本循环

```text
初始化 master_set，加入 listen_fd
max_fd = listen_fd

while true:
    read_set = master_set
    ready_count = select(max_fd + 1, &read_set, ...)

    if listen_fd 可读:
        accept 新连接
        FD_SET(client_fd, master_set)
        更新 max_fd

    遍历 client_fds:
        if FD_ISSET(client_fd, read_set):
            recv 一次
            根据返回值处理数据/关闭/错误

    统一移除 closed_fds
```

## 3.3 为什么每轮都要 `read_set = master_set`

`select` 会修改传入的 `fd_set`，返回后 `read_set` 只表示本轮就绪结果。

因此：

```text
master_set 保存长期状态；
read_set 是本轮临时变量；
每轮调用 select 前必须重新复制。
```

## 3.4 select 为什么还要遍历

`select` 返回的是：

```text
本轮有多少个 fd 就绪。
```

但它不会直接告诉你具体是哪几个 fd。

所以仍然需要：

```cpp
FD_ISSET(fd, &read_set)
```

逐个检查。

## 3.5 ready_count 的意义

`ready_count` 不是负责告诉你具体 fd，而是告诉你：

```text
本轮还有多少就绪事件没有处理。
```

常见用法：

```text
处理 listen_fd 后 --ready_count；
处理一个 client_fd 后 --ready_count；
ready_count <= 0 时提前结束遍历。
```

它是一种优化，不改变 select 需要扫描 fd 集合的事实。

## 3.6 select 的局限

```text
fd_set 大小有限，常见 FD_SETSIZE 为 1024；
每轮都要复制 fd_set；
每轮都要从小到大扫描；
需要维护 max_fd；
删除最大 fd 后要 rebuild_max_fd。
```

---

# 4. select + ThreadPool 的边界

## 4.1 容易混淆的两个模型

### 线程池阻塞模型

```text
main:
    accept()
    pool.submit(handle_client(client_fd))

worker:
    while(read)
    write
    close
```

### select + pool 模型

```text
I/O 线程 / Selector:
    select
    accept
    recv 一次
    判断关闭
    提交已经读到的数据给 pool
    管理 client_fd 生命周期

ThreadPool:
    处理业务
    当前简化版可以 write
    不 read
    不 close
```

## 4.2 关键原则

```text
accept 归 Selector；
select 归 Selector；
client_fd 集合归 Selector；
recv 与关闭判断归 Selector；
耗时业务归 ThreadPool；
main 只负责组装。
```

可以压缩成一句话：

```text
I/O 状态归 Selector，业务耗时归 Pool，资源生命周期归创建者。
```

## 4.3 worker 直接 write 的阶段性风险

当前学习阶段可能写成：

```text
Selector recv 数据；
worker 处理业务后直接 write(client_fd)。
```

风险：

```text
worker 写 fd 时，Selector 可能已经 close 了该 fd；
fd 数字可能被系统复用；
多个 worker 可能同时 write 同一个 fd；
write 本身也可能阻塞。
```

更标准的 Reactor 方向：

```text
worker 只生成 response；
I/O 线程维护写缓冲；
I/O 线程负责实际 write；
需要写时关注 EPOLLOUT。
```

---

# 5. fd 生命周期与关闭管理

## 5.1 fd 不是 socket 本身

```text
fd 是当前进程 fd 表中的整数索引。
它指向内核中的打开文件对象或 socket 对象。
```

对于 socket：

```text
fd -> socket 对象 -> TCP 状态 + 接收缓冲区 + 发送缓冲区
```

## 5.2 recv 返回值

```cpp
ssize_t n = recv(client_fd, buffer, sizeof(buffer), 0);
```

含义：

| 返回值 | 含义 | 处理 |
|---|---|---|
| `n > 0` | 读到数据 | 处理或提交业务 |
| `n == 0` | 对端有序关闭 | 移除连接 |
| `n < 0 && errno == EINTR` | 被信号中断 | 本轮可重试或忽略 |
| `n < 0 && errno == EAGAIN/EWOULDBLOCK` | 非阻塞下暂无数据 | 不关闭 |
| `n < 0` 其他错误 | 出错 | 移除连接 |

## 5.3 select 中 remove_client 的完整职责

```text
close(client_fd)
FD_CLR(client_fd, &master_set)
从 client_fds 删除
如果 client_fd == max_fd，rebuild_max_fd
```

不能只 `close`，否则 `master_set` 中可能残留无效 fd，下一轮 `select` 可能返回 `EBADF`。

## 5.4 closed_fds 延迟删除

遍历 `client_fds` 时不建议直接 erase。

常见结构：

```text
遍历 client_fds，发现需要关闭就加入 closed_fds；
遍历结束后统一 remove_client；
最后 clear closed_fds。
```

`closed_fds` 用完必须清空，否则可能重复关闭旧 fd；如果 fd 数字被复用，甚至可能误关新连接。

---

# 6. poll 模型

## 6.1 poll 解决了 select 的哪些问题

`poll` 不再使用 `fd_set` 和 `max_fd`，而是使用数组：

```cpp
struct pollfd {
    int fd;
    short events;
    short revents;
};
```

核心结构：

```cpp
std::vector<pollfd> fds;
```

每个元素同时记录：

```text
fd：监听哪个 fd；
events：关心什么事件；
revents：本轮实际发生了什么事件。
```

## 6.2 poll 基本循环

```text
fds[0] = listen_fd, POLLIN

while true:
    ready_count = poll(fds.data(), fds.size(), timeout)

    if listen_fd 的 revents 有 POLLIN:
        accept 新连接
        push_back({client_fd, POLLIN, 0})

    遍历 fds 中的 client_fd:
        if revents 有 POLLIN:
            recv 一次
            处理数据/关闭/错误

    删除关闭连接对应的 pollfd
```

## 6.3 poll 是否还需要遍历

需要。

`poll` 返回的也是：

```text
本轮有多少 fd 有事件。
```

具体哪个 fd 发生事件，需要遍历 `pollfd` 数组并检查 `revents`。

所以：

```text
poll 去掉了 fd_set/max_fd/rebuild_max_fd；
但没有去掉遍历数组的成本。
```

## 6.4 poll 相对 select 的好处

```text
不需要 FD_SET / FD_CLR；
不需要 max_fd；
删除最大 fd 后不需要 rebuild_max_fd；
fd 数量不受 FD_SETSIZE 这种固定限制影响；
事件和结果分离为 events/revents，结构更清晰。
```

## 6.5 poll 的局限

```text
仍然需要线性遍历 vector<pollfd>；
连接多但活跃少时，扫描成本仍然明显；
删除元素时仍要维护数组结构；
事件处理和 fd 生命周期仍然需要自己设计。
```

---

# 7. epoll 模型

## 7.1 epoll 与 select/poll 的关键差异

`select/poll`：

```text
用户每轮把监听集合传给内核；
内核检查后返回数量；
用户再扫描集合找具体就绪 fd。
```

`epoll`：

```text
先用 epoll_ctl 把 fd 注册到内核维护的 epoll 实例；
epoll_wait 直接返回就绪事件数组；
用户遍历的是就绪事件，而不是全部连接。
```

## 7.2 epoll 基本 API

```cpp
int epfd = epoll_create1(0);

struct epoll_event ev;
ev.events = EPOLLIN;
ev.data.fd = listen_fd;
epoll_ctl(epfd, EPOLL_CTL_ADD, listen_fd, &ev);

while (true) {
    int n = epoll_wait(epfd, events, maxevents, timeout);
    for (int i = 0; i < n; ++i) {
        int fd = events[i].data.fd;
        uint32_t evs = events[i].events;
        // 处理 fd 事件
    }
}
```

## 7.3 listen_fd 与 client_fd 事件处理

```text
如果 fd == listen_fd：
    accept 新连接；
    设置非阻塞；
    epoll_ctl ADD client_fd。

否则：
    根据 events 判断 EPOLLIN / EPOLLRDHUP / EPOLLERR / EPOLLHUP；
    recv 数据或关闭连接。
```

## 7.4 epoll 仍然不能替你处理的事

```text
fd 什么时候 close；
close 前是否 epoll_ctl DEL；
读缓冲如何维护；
一次 recv 是否读完；
半包/粘包如何处理；
worker 是否能直接 write；
同一个连接是否重复提交任务；
业务结果如何回到 I/O 线程。
```

所以：

```text
epoll 只是更高效的事件通知机制，不是完整服务器架构。
```

## 7.5 LT 与 ET 的阶段建议

当前建议：

```text
先使用 LT（水平触发）。
```

因为 LT 下：

```text
只要 fd 还有数据没读完，下次 epoll_wait 仍会通知。
```

ET（边沿触发）要求更严格：

```text
fd 必须设置非阻塞；
每次读到 EAGAIN/EWOULDBLOCK 为止；
否则可能遗漏事件。
```

当前阶段重点是先稳定理解：

```text
epoll_create1 / epoll_ctl / epoll_wait；
ADD / MOD / DEL；
EPOLLIN / EPOLLOUT / EPOLLERR / EPOLLHUP / EPOLLRDHUP；
连接生命周期。
```

---

# 8. select / poll / epoll 对比

| 模型 | 监听集合在哪里 | 返回什么 | 是否还要遍历全部连接 | 主要维护成本 |
|---|---|---|---|---|
| select | 用户态 fd_set | 就绪数量 | 是 | fd_set、max_fd、FD_CLR、rebuild_max_fd |
| poll | 用户态 pollfd 数组 | 就绪数量 | 是 | pollfd 数组增删、revents 清理 |
| epoll | 内核 epoll 实例 | 就绪事件数组 | 否，只遍历就绪事件 | epoll_ctl ADD/MOD/DEL、连接状态 |

核心理解：

```text
select/poll 是“我给你一批 fd，你帮我看看哪些就绪”。
epoll 是“我先把 fd 注册给你，以后你直接告诉我哪些就绪”。
```

---

# 9. 与 ThreadPool 的关系

## 9.1 不推荐的方向

```text
把 client_fd 扔给 worker；
worker 自己 while(read)；
worker 自己 close；
I/O 线程还保留该 fd。
```

这会导致：

```text
I/O 线程和 worker 争夺同一个 fd 的生命周期；
可能重复 read；
可能 worker close 后 I/O 线程仍监听；
可能 select/epoll 出现无效 fd。
```

## 9.2 当前阶段可接受的简化

```text
I/O 线程 recv 一次；
把 data 交给 worker；
worker 处理后 write 回 fd；
fd 仍由 I/O 线程负责关闭。
```

但必须知道风险：

```text
worker 写时 fd 可能已关闭或复用；
多个 worker 可能并发写同一个 fd；
write 可能阻塞。
```

## 9.3 更标准的 Reactor 方向

```text
I/O 线程：
    管理所有 fd；
    负责 read；
    负责 write；
    维护 Connection、read_buffer、write_buffer；
    监听 EPOLLIN / EPOLLOUT。

ThreadPool：
    只处理业务；
    输入 request/data；
    输出 response；
    不直接 read/write/close fd。
```

---

# 10. 常见错误清单

## 10.1 select 相关

```text
忘记 read_set = master_set；
close 后忘记 FD_CLR；
删除 max_fd 后不 rebuild；
closed_fds 用完不 clear；
select 返回 -1 时不处理 EINTR；
ready_count 理解成“具体 fd”。
```

## 10.2 poll 相关

```text
以为 poll 不需要遍历；
删除 pollfd 时迭代器/下标混乱；
没有区分 events 和 revents；
关闭 fd 后 pollfd 仍留在 vector 中。
```

## 10.3 epoll 相关

```text
以为 epoll 自动管理 fd 生命周期；
close 前后没有想清楚是否需要 DEL；
ET 模式下没有读到 EAGAIN；
没有设置非阻塞却使用 ET；
把 EPOLLHUP / EPOLLERR 当作普通可读处理；
重复把同一个连接提交给多个 worker。
```

## 10.4 ThreadPool 边界相关

```text
worker 里 while(read)；
worker close fd；
worker 和 I/O 线程同时读同一个 fd；
worker 直接 write 但没有理解风险；
业务处理和连接生命周期混在一起。
```

---

# 11. 推荐实验顺序

```text
实验 1：select 单线程 echo。
    目标：理解 fd_set、max_fd、read_set = master_set。

实验 2：select + threadpool 简化版。
    目标：I/O 线程 recv，worker 做业务。

实验 3：poll 改写 select。
    目标：用 vector<pollfd> 替换 fd_set/max_fd。

实验 4：epoll 单线程 echo。
    目标：理解 epoll_create1 / epoll_ctl / epoll_wait。

实验 5：epoll + threadpool 简化版。
    目标：业务下沉到线程池，但 fd 生命周期仍归 I/O 线程。

实验 6：Connection 对象。
    目标：为每个连接维护 read_buffer、write_buffer、状态和关闭标记。
```

---

# 12. 自测问题

1. 为什么线程池阻塞模型仍然会被慢连接拖住？
2. `select` 为什么每轮都要复制 `fd_set`？
3. `select` 返回 ready_count 后，为什么还要遍历 fd？
4. `poll` 相比 `select` 少维护了什么？
5. `poll` 为什么仍然需要遍历 pollfd 数组？
6. `epoll_wait` 返回的是什么？和 `select/poll` 有什么不同？
7. 为什么 I/O 线程和 worker 不能同时随意 read/close 同一个 fd？
8. `recv` 返回 0 表示什么？
9. `EPOLLHUP`、`EPOLLERR`、`EPOLLRDHUP` 分别应该引起什么警觉？
10. Reactor 中 ThreadPool 应该负责什么，不应该负责什么？

---

# 13. 阶段结论

```text
select/poll/epoll 的差异主要在“事件通知机制”；
服务器真正困难的部分仍然是：
    fd 生命周期；
    连接状态；
    缓冲区；
    I/O 与业务边界；
    错误处理；
    关闭时机；
    并发安全。
```

当前最重要的学习目标不是“马上写复杂 Reactor”，而是：

```text
能清楚解释 select / poll / epoll 各自如何等待事件；
能说明 fd 由谁管理、何时关闭；
能避免 worker 把 I/O 生命周期写乱；
能从 select 平滑迁移到 poll，再迁移到 epoll。
```
