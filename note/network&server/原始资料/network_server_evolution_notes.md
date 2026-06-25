# C++ / Linux 网络服务器学习整合笔记

> 来源：由原始复盘《C++ Linux 网络服务器学习复盘笔记》整合而成。  
> 用途：作为 `study` 项目中 `03_Network_网络编程与服务器`、`07_Project_项目设计与代码审查`、`02_Linux_系统编程与操作系统` 的长期维护笔记。  
> 当前主题：从阻塞式 TCP Echo Server 演进到 `select + threadpool` 服务器。

---

## 0. 这份笔记应该怎么用

这份笔记不是“代码流水账”，而是用于长期复习和项目推进的**主线笔记**。

建议用法：

1. 每完成一个服务器版本，就更新「模型演进」。
2. 每踩一个 bug，就补充到「问题清单」。
3. 每做一次重构，就更新「模块职责」。
4. 每准备进入新模型，例如 `poll` / `epoll`，先回看「设计口诀」和「fd 生命周期」。
5. 每次让 GPT 审查代码时，可以把相关章节作为上下文。

---

# 1. 学习主线总览

当前学习主线是：

```text
阻塞 TCP Echo Server
    -> 多进程服务器
    -> 多线程服务器
    -> 线程池服务器
    -> select I/O 复用服务器
    -> 模块化 select 服务器
    -> select + threadpool 服务器
    -> poll / epoll
    -> HTTP Server
    -> 带日志、配置、定时器、数据库/API 的后端服务
```

每一层都不是“替代上一层”，而是在解决上一层的问题，同时引入新的问题。

| 阶段 | 解决的问题 | 引入的新问题 |
|---|---|---|
| 单进程阻塞 | 理解 socket 基础流程 | 一次只能服务一个客户端 |
| 多进程 | 能并发处理多个连接 | 进程开销大，僵尸进程处理 |
| 多线程 | 并发成本低于多进程 | 共享 fd 表，线程安全问题 |
| 线程池 | 控制线程数量 | 阻塞 read 会占用 worker |
| select | 一个线程监听多个 fd | 需要维护 fd_set / max_fd / client_fds |
| select + threadpool | I/O 与业务分离 | fd 生命周期、任务边界、并发 write 风险 |
| poll / epoll | 改善事件通知机制 | fd 生命周期问题仍然存在 |
| HTTP server | 从 demo 走向后端服务 | 协议解析、状态管理、错误处理、工程化 |

---

# 2. 服务器模型演进

## 2.1 单进程阻塞 TCP Echo Server

最小流程：

```text
socket()
setsockopt(SO_REUSEADDR)
bind()
listen()
accept()
read()
write()
close()
```

核心阻塞点：

```text
accept() 阻塞等待新连接
read() 阻塞等待客户端数据
```

适合掌握：

- socket API 基础；
- TCP server 生命周期；
- `read` / `write` 的基本含义；
- C++ 如何封装 C 风格系统调用。

局限：

```text
一个连接阻塞在 read 时，服务器无法 accept 或处理其他客户端。
```

关键认识：

```text
socket API 属于 POSIX/Linux 系统编程，不是 C++ Primer 的核心内容。
C++ 在这里主要负责组织代码、封装资源、管理生命周期。
```

---

## 2.2 多进程阻塞服务器

模型：

```text
父进程：
    accept()
    fork()
    close(client_fd)
    继续 accept()

子进程：
    close(listen_fd)
    handle_client(client_fd)
    close(client_fd)
    exit(0)
```

关键点：

```text
fork 后，父子进程拥有各自的 fd 表副本。
父进程 close(client_fd) 不影响子进程继续使用该连接。
子进程必须 exit(0)，否则可能继续执行父进程的 accept 逻辑。
```

僵尸进程处理：

```cpp
signal(SIGCHLD, SIG_IGN);
```

更严谨做法：

```text
sigaction + waitpid(WNOHANG)
```

适合掌握：

- `fork` 后资源继承；
- 父子进程 fd 表关系；
- 僵尸进程；
- 进程隔离。

局限：

```text
每个连接一个进程，隔离性强，但创建和切换成本较高。
```

---

## 2.3 多线程阻塞服务器

模型：

```text
主线程：
    accept()
    std::thread(handle_client, client_fd).detach()

工作线程：
    read()
    write()
    close(client_fd)
```

与多进程的关键区别：

```text
多进程：父子进程有各自 fd 表。
多线程：同一进程内所有线程共享同一张 fd 表。
```

因此：

```text
主线程 accept 后不能 close(client_fd)，否则工作线程可能读到 Bad file descriptor。
```

常见错误：

```text
read 失败: Bad file descriptor
```

可能原因：

1. 主线程提前 `close(client_fd)`；
2. 工作线程重复 close 后继续 read；
3. thread 参数用引用传递，导致多个线程拿到错误 fd；
4. fd 被关闭后又被系统复用。

线程日志问题：

```text
多个线程同时 cout 会交叉输出。
```

教学阶段可以用：

```cpp
std::mutex cout_mutex;
std::lock_guard<std::mutex> lock(cout_mutex);
```

但工程上更适合：

```text
日志队列 + 单独日志线程
```

---

## 2.4 线程池阻塞服务器

线程池解决的问题：

```text
避免每个连接都创建一个新线程。
控制线程数量。
复用 worker。
```

典型模块结构：

```text
include/
    ThreadPool.h
    TcpServer.h
    ClientHandler.h

src/
    ThreadPool.cpp
    TcpServer.cpp
    ClientHandler.cpp
    main.cpp

Makefile
```

### ThreadPool 职责

```text
创建固定数量工作线程
维护任务队列
submit() 提交任务
worker_loop() 等待并执行任务
析构时通知停止并 join
```

核心成员：

```cpp
std::vector<std::thread> workers_;
std::queue<std::function<void()>> tasks_;
std::mutex mutex_;
std::condition_variable condition_;
bool stop_;
```

任务提交示例：

```cpp
pool.submit([client_fd]() {
    handle_client(client_fd);
});
```

### 线程池关键规则

```text
锁内只取任务。
锁外执行任务。
```

错误思路：

```text
持有任务队列锁时执行 task()
```

问题：

```text
如果 task 内部阻塞 read()，整个线程池任务队列都可能被卡住。
```

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

### 线程池阻塞模型的局限

```text
如果每个任务内部 while(read)，慢客户端会长期占用 worker。
连接数上升后，线程池可能被阻塞连接耗尽。
```

---

## 2.5 select I/O 复用服务器

核心思想：

```text
一个线程阻塞在 select()
等待多个 fd 中任意一个就绪。
```

不是：

```text
一个线程阻塞在某一个客户端 read()
```

select 关心：

```text
listen_fd 是否可读
client_fd 是否可读
```

含义：

```text
listen_fd 可读：
    有新连接可以 accept

client_fd 可读：
    有数据可 recv
    或对端关闭
    或 socket 出错
```

基本结构：

```cpp
fd_set master_set;
fd_set read_set;
int max_fd;
std::vector<int> client_fds;
```

核心循环：

```text
read_set = master_set
select(max_fd + 1, &read_set, ...)

如果 listen_fd 可读：
    accept 新连接

遍历 client_fds_：
    如果 client_fd 可读：
        recv / close / remove
```

重要规则：

```text
select 会修改传入的 fd_set。
所以每轮都要 read_set = master_set。
```

### select 的扫描成本

`select()` 返回的是：

```text
有多少个 fd 就绪
```

但不会直接告诉你：

```text
具体哪些 fd 就绪
```

所以需要：

```cpp
FD_ISSET(client_fd, &read_set_)
```

如果 500 个 fd 中只有最后一个就绪，仍然可能扫描前 499 个。

`ready_count` 的作用：

```text
记录本轮还有多少就绪 fd 没处理。
处理一个就 --ready_count。
ready_count <= 0 时可以提前结束扫描。
```

---

## 2.6 select + threadpool 服务器

这是当前项目的核心模型。

容易混淆的两个模型：

### 线程池阻塞模型

```text
main:
    accept()
    pool.submit(handle(client_fd))

handle:
    while(read)
    write
    close
```

### select + pool 模型

```text
main:
    创建 TcpServer
    创建 ThreadPool
    创建 Selector(server, pool)
    selector.loop()

Selector:
    select
    accept
    recv 一次
    判断关闭
    提交业务任务给 pool
    管理 client_fd 生命周期

ThreadPool:
    处理已经读到的数据
    简化版中可以 write 回去
```

关键结论：

```text
accept 应该由 Selector 调用。
select 应该只在 Selector::loop 中调用。
main 不应该再 while accept。
线程池任务不应该再 while(read)。
```

---

# 3. 核心概念整理

## 3.1 fd 的准确理解

`fd` 是：

```text
当前进程 fd 表中的整数索引。
```

它不是文件本身，也不是 socket 本身。

进程 fd 表示意：

```text
fd 0 -> stdin
fd 1 -> stdout
fd 2 -> stderr
fd 3 -> 普通文件
fd 4 -> listen socket
fd 5 -> client socket
fd 6 -> pipe
```

普通文件：

```text
fd -> 打开的文件对象 -> inode -> 磁盘文件
```

socket：

```text
fd -> socket 对象 -> TCP 状态 + 接收缓冲区 + 发送缓冲区
```

因此：

```text
socket fd 是访问内核 socket 对象的句柄。
```

`recv` 的含义：

```text
通过 fd 找到 socket 对象。
从该 socket 的内核接收缓冲区拷贝数据到用户态 buffer。
```

`send` 的含义：

```text
通过 fd 找到 socket 对象。
把用户态数据拷贝到 socket 的内核发送缓冲区。
再由 TCP 协议栈发送出去。
```

---

## 3.2 TCP / UDP / 应用层协议

### TCP

特点：

```text
面向连接
可靠
有序
字节流
没有消息边界
```

服务器典型流程：

```text
socket
bind
listen
accept
read/write
close
```

### UDP

特点：

```text
无连接
数据报
有消息边界
不保证可靠
不保证有序
不保证不重复
```

UDP 不只是改 server，还要改 handler。

TCP handler：

```text
handle_client(client_fd)
循环 read/write 一个连接
```

UDP handler：

```text
handle_datagram(socket_fd, data, client_addr)
一次处理一个数据报
```

### 应用层协议

TCP 只提供字节流，应用层需要定义消息边界。

常见方案：

1. 固定长度；
2. 分隔符，例如 `\n`；
3. 长度字段 + 消息体。

关于“粘包”更准确的表述：

```text
TCP 本身没有“包”边界。
所谓粘包/拆包，本质是应用层协议没有正确定义或解析消息边界。
```

接收端应：

```text
维护缓冲区。
每次 read 后 append。
循环解析完整消息。
不够一条完整消息就等待下一次 read。
```

---

## 3.3 recv 返回值

典型处理：

```cpp
ssize_t bytes_read = recv(client_fd, buffer, sizeof(buffer) - 1, 0);

if (bytes_read > 0) {
    std::string data(buffer, bytes_read);
    pool_.submit([client_fd, data] {
        handle_business(client_fd, data);
    });
    return;
}

if (bytes_read == 0) {
    closed_fds_.push_back(client_fd);
    return;
}

if (errno == EINTR) {
    return;
}

closed_fds_.push_back(client_fd);
```

含义：

| 返回值 | 含义 | 处理 |
|---|---|---|
| `> 0` | 读到数据 | 提交业务任务，保留连接 |
| `== 0` | 对端有序关闭 | 标记关闭 |
| `< 0 && errno == EINTR` | 被信号中断 | 本轮忽略 |
| `< 0 && errno != EINTR` | 出错 | 标记关闭 |

注意：

```text
客户端关闭不是“写入 0”。
客户端发送 FIN 后，服务端 socket 被 select 判定为可读。
服务端 recv 返回 0。
```

---

## 3.4 MSG_PEEK

示例：

```cpp
char buf;
ssize_t n = recv(client_fd, &buf, 1, MSG_PEEK);
```

作用：

```text
偷看 socket 接收缓冲区中的 1 字节，但不取走。
```

返回值：

```text
n > 0：有数据
n == 0：对端关闭
n < 0：出错
```

当前推荐模型中：

```text
Selector 自己 recv 数据，然后把 data 交给 pool。
这种情况下不需要 MSG_PEEK。
```

`MSG_PEEK` 更适合：

```text
Selector 只探测 fd，真正 read 由 worker 完成。
```

但这会带来风险：

```text
同一个 fd 可能被重复提交给多个 worker。
多个线程可能同时 read 同一个 socket。
```

当前阶段不推荐。

---

# 4. 模块职责与边界

## 4.1 判断什么时候设计为类

适合类化的判断标准：

```text
有状态
围绕状态有一组操作
需要初始化/析构资源
存在状态一致性规则
```

适合类化：

```text
ThreadPool：
    有线程、任务队列、mutex、condition_variable、stop 标志

TcpServer：
    有 listen_fd、port，负责 socket 生命周期

Selector：
    有 fd_set、max_fd、client_fds_，负责事件循环和 fd 集合
```

不一定一开始类化：

```text
简单 handle_client 函数
简单 echo 业务函数
```

---

## 4.2 TcpServer

职责：

```text
socket / bind / listen / accept
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

关于 `listen_fd_`：

```text
不要直接 public。
不要随便 friend。
用 public getter：int listen_fd() const;
```

---

## 4.3 ThreadPool

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

在当前模型中：

```text
ThreadPool 是 Selector 使用的业务执行器。
```

不是：

```text
pool 调用 select。
pool 包含 select。
pool 是 select 的子集。
```

---

## 4.4 Selector

职责：

```text
管理 select loop。
管理 master_set_ / read_set_。
管理 client_fds_。
维护 max_fd_。
处理 listen_fd 可读。
处理 client_fd 可读。
执行 recv。
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

TcpServer 不需要知道 Selector。  
ThreadPool 也不需要知道 Selector。

---

## 4.5 BusinessHandler

旧版阻塞模型：

```cpp
void handle(int client_fd)
{
    while (true) {
        read(client_fd, ...);
        write(client_fd, ...);
    }

    close(client_fd);
}
```

适合：

```text
一个 client_fd 交给一个 worker。
worker 长期负责这个连接。
```

不适合 select + pool：

1. `while(read)` 会让 worker 长期阻塞；
2. `close(client_fd)` 会破坏 Selector 的 fd_set 管理；
3. handle 同时负责 read/write/close/业务，职责过重。

新模型应拆成：

```text
Selector::handle_client_event(client_fd)
    recv 一次
    判断关闭
    n > 0 时 submit 到 pool

handle_business(client_fd, data)
    处理已经读到的数据
    简化版 write 回去
    不 read
    不 close
```

关键规则：

```text
handle_business 不 read。
handle_business 不 close。
```

---

# 5. fd 生命周期管理

## 5.1 长连接中的 client_fd

重要结论：

```text
client_fd 可以长期打开。
不是每次处理完业务就 close。
```

“打开 fd”表示：

```text
进程仍然持有这个 fd，还没有 close()。
```

不是：

```text
正在 read/write。
```

长连接流程：

```text
accept 得到 client_fd
FD_SET 加入监听
select 等待可读
recv 一次
提交业务
保留 fd
等待下一次可读
直到 recv 返回 0 或不可恢复错误
remove_client
```

## 5.2 什么时候关闭 fd

应该关闭的情况：

1. `recv` 返回 0；
2. `recv` 返回不可恢复错误；
3. 协议规定短连接处理完即关闭；
4. 超时；
5. 服务器主动关闭；
6. 程序退出。

不应该因为：

```text
这次业务处理完了。
```

就立刻关闭长连接 fd。

---

## 5.3 remove_client 的完整职责

关闭 client_fd 时必须做：

```text
1. close(client_fd)
2. FD_CLR(client_fd, &master_set_)
3. 从 client_fds_ 删除
4. 如果 client_fd == max_fd_，rebuild_max_fd()
```

否则可能：

```text
master_set_ 中残留已经 close 的 fd。
下一轮 select 检查到无效 fd，返回 EBADF。
```

示例：

```cpp
void Selector::remove_client(int client_fd)
{
    close(client_fd);
    FD_CLR(client_fd, &master_set_);

    client_fds_.erase(
        std::remove(client_fds_.begin(), client_fds_.end(), client_fd),
        client_fds_.end()
    );

    if (client_fd == max_fd_) {
        rebuild_max_fd();
    }
}
```

## 5.4 rebuild_max_fd

正确写法：

```cpp
void Selector::rebuild_max_fd()
{
    max_fd_ = tcp_server_.listen_fd();

    for (int client_fd : client_fds_) {
        if (client_fd > max_fd_) {
            max_fd_ = client_fd;
        }
    }
}
```

关键：

```text
必须先 max_fd_ = listen_fd。
不能在旧 max_fd_ 基础上继续比较。
```

调用时机：

```text
只在删除的 fd 正好等于当前 max_fd_ 时调用。
```

新增连接时：

```cpp
if (client_fd > max_fd_) {
    max_fd_ = client_fd;
}
```

不建议：

```text
每轮 loop 末尾无条件 rebuild_max_fd。
```

---

## 5.5 closed_fds_

用途：

```text
本轮 select 中记录需要关闭的 fd。
等遍历结束后统一 remove。
```

原因：

```text
遍历 vector 时直接删除元素，容易导致迭代器或索引混乱。
```

常见模式：

```cpp
for (int client_fd : client_fds_) {
    if (FD_ISSET(client_fd, &read_set_)) {
        handle_client_event(client_fd);
    }
}

for (int fd : closed_fds_) {
    remove_client(fd);
}

closed_fds_.clear();
```

重要坑：

```text
closed_fds_ 用完必须 clear。
```

否则：

```text
下一轮重复 remove 同一个 fd。
如果 fd 数字被系统复用，甚至可能误关新连接。
```

---

# 6. 当前 select + pool 简化模型的风险

当前简化版：

```text
Selector:
    recv
    submit data

Worker:
    handle_business
    write(fd)
```

潜在风险：

1. worker 写 fd 时，fd 可能已经被 Selector close；
2. fd 数字可能被系统复用，worker 可能写到新连接；
3. 同一个 fd 多次可读，可能被提交多个任务；
4. 多个 worker 可能同时 write 同一个 fd；
5. write 本身也可能阻塞。

当前阶段：

```text
学习阶段可以接受。
但必须知道这不是最终工程模型。
```

更标准的 Reactor：

```text
I/O 线程：
    负责所有 socket read/write
    维护读缓冲和写缓冲
    监听 EPOLLIN / EPOLLOUT

ThreadPool：
    只处理业务
    生成 response
    把 response 放回连接写缓冲
```

当前项目后续应逐渐向这个方向演进：

```text
worker 不直接 write fd。
worker 返回 response。
Selector / I/O 线程负责实际发送。
```

---

# 7. 调试与测试记录

## 7.1 nc 关闭测试

现象：

```text
普通 nc 连接后，输入有 echo。
按 Ctrl+D 后，服务端没有立即打印关闭。
后续输入无 echo。
Ctrl+C 时服务端显示关闭。
```

原因：

```text
Ctrl+D 只是让 nc 的 stdin EOF。
普通 nc 不一定立刻关闭 TCP socket。
服务端收不到 FIN，recv 不返回 0。
```

正确测试：

```bash
nc -N 127.0.0.1 8080
```

使用 `nc -N` 后：

```text
Ctrl+D 会关闭网络写方向。
服务端能检测到关闭。
```

结论：

```text
这是 nc 行为差异，不一定是服务端关闭检测错误。
```

---

## 7.2 线程数和上下文切换观察

观察线程数：

```bash
ls /proc/$(pidof server)/task | wc -l
ps -T -p PID
```

观察上下文切换：

```bash
pidstat -w -p PID 1
```

压测时应比较：

```text
线程数量
内存占用
CPU 使用率
上下文切换
QPS
平均延迟
P95 / P99 延迟
失败数
```

对比结论：

```text
每连接一线程：
    连接越多，线程越多，资源和上下文切换压力上升。

线程池：
    线程数固定，资源可控。
    但如果连接长期阻塞 read，会占用 worker。
```

---

# 8. 当前已发现问题与修正建议

## 8.1 closed_fds_ 没有 clear

错误：

```cpp
for (int client_fd : closed_fds_) {
    remove_client(client_fd);
}
rebuild_max_fd();
```

缺少：

```cpp
closed_fds_.clear();
```

修正：

```cpp
for (int client_fd : closed_fds_) {
    remove_client(client_fd);
}
closed_fds_.clear();
```

---

## 8.2 loop 末尾不应无条件 rebuild_max_fd

如果 `remove_client()` 中已经：

```cpp
if (client_fd == max_fd_) {
    rebuild_max_fd();
}
```

那么 loop 末尾不需要无条件 rebuild。

原因：

```text
rebuild_max_fd 是维护 max_fd 的修正操作。
它应该只在必要时发生。
```

---

## 8.3 select 出错应处理 EINTR

建议：

```cpp
if (ready_count == -1) {
    if (errno == EINTR) {
        continue;
    }

    std::cerr << "select 失败: "
              << std::strerror(errno)
              << std::endl;
    continue;
}
```

---

## 8.4 main 注释过时

旧注释：

```text
main 负责 accept 并提交线程池
```

现在应改为：

```text
main 负责创建 TcpServer、ThreadPool、Selector，并启动 selector.loop()
```

---

# 9. 当前最容易犯的错误清单

## 错误一：main 继续 accept

错误：

```cpp
selector.loop();

while (true) {
    int client_fd = server.accept_client();
    pool.submit(...);
}
```

问题：

```text
selector.loop() 通常不会返回。
后面的代码执行不到。
main 和 Selector 都 accept 会造成模型混乱。
```

---

## 错误二：Selector 里 submit 旧版 handle

错误：

```cpp
pool_.submit([client_fd] {
    handle(client_fd);
});
```

如果 `handle` 内部仍然：

```text
while(read)
close
```

就退回阻塞线程池模型。

---

## 错误三：worker close fd

错误：

```cpp
void handle_business(...) {
    close(client_fd);
}
```

问题：

```text
Selector 的 master_set_ 和 client_fds_ 不知道 fd 已关闭。
下一轮 select 可能 EBADF。
```

---

## 错误四：关闭 fd 后忘记 FD_CLR

错误：

```cpp
close(client_fd);
```

但没有：

```cpp
FD_CLR(client_fd, &master_set_);
```

后果：

```text
select 可能返回 EBADF。
```

---

## 错误五：closed_fds_ 不 clear

后果：

```text
重复 remove 旧 fd。
fd 被复用后可能误关新连接。
```

---

## 错误六：rebuild_max_fd 不重置

错误：

```cpp
for (int fd : client_fds_) {
    if (fd > max_fd_) max_fd_ = fd;
}
```

正确：

```cpp
max_fd_ = tcp_server_.listen_fd();
for (...) {
    ...
}
```

---

# 10. 推荐注释风格

关键函数建议写职责注释，而不是只写“做什么”。

```cpp
// Selector 线程调用：处理一次 client_fd 可读事件。
// 负责 recv 和关闭判断；不做耗时业务。
void Selector::handle_client_event(int client_fd);
```

```cpp
// 工作线程调用：处理已经读到的数据。
// 不负责 read，不负责 close。
void handle_business(int client_fd, const std::string& data);
```

```cpp
// Selector 线程调用：移除连接。
// 负责 close、FD_CLR、client_fds_ 删除、必要时重建 max_fd_。
void Selector::remove_client(int client_fd);
```

作用：

```text
防止后续把 read / write / close / business 的职责重新写混。
```

---

# 11. Makefile 与运行便利性

当前生成目标：

```makefile
TARGET = bin/server
```

运行：

```bash
./bin/server
```

建议添加：

```makefile
run: $(TARGET)
	./$(TARGET)

rebuild: clean all

.PHONY: all clean run rebuild
```

以后可以：

```bash
make run
```

如果在上级目录：

```bash
make -C select_pool run
```

目录名建议：

```text
select_pool
```

比：

```text
select+pool
```

更稳妥。

---

# 12. 设计原则沉淀

## 12.1 自上而下设计，自下而上实现

设计时：

```text
先看目标
再拆模块
再确定职责
再确定调用关系
```

实现时：

```text
先实现基础模块
再组合上层模块
最后跑通完整流程
```

当前项目：

```text
目标：
    select + threadpool echo server

模块：
    TcpServer
    ThreadPool
    Selector
    BusinessHandler

实现顺序：
    TcpServer
    ThreadPool
    Handler
    Selector
    main 组装
```

判断谁作为谁的成员：

```text
谁在运行过程中需要主动调用谁？
```

所以：

```text
Selector 需要调用 TcpServer。
Selector 需要调用 ThreadPool。
Selector 持有 TcpServer& 和 ThreadPool&。
```

---

## 12.2 模块化的真正意义

误区：

```text
模块化后就应该不用改代码。
```

修正：

```text
模块化不是保证永远不用改。
模块化是让变化集中在应该变化的地方。
```

从线程池阻塞模型改成 select + threadpool，是核心并发模型变化，所以必然改：

```text
main
Selector
handle
```

但基本不需要改：

```text
ThreadPool 的任务队列、worker_loop、condition_variable
TcpServer 的 socket/bind/listen 逻辑
```

这说明模块化已经起作用：

```text
稳定模块少改。
变化集中在模型相关模块。
```

---

## 12.3 AI 时代下自己重构的意义

如果目标只是得到 demo：

```text
AI 生成更快。
```

如果目标是成长：

```text
自己重构仍然非常有意义。
```

真正重要的能力：

```text
知道为什么这么设计。
知道哪里可能出 bug。
知道出了问题怎么定位。
知道如何把需求改进去。
能判断 AI 生成代码是否合理。
```

更适合的 AI 使用方式：

```text
自己写核心逻辑。
AI 做审查、解释、辅助定位。
外围代码如 Makefile、测试脚本、README 可以让 AI 辅助。
```

目标不是：

```text
我比 AI 写得快。
```

而是：

```text
我能带着 AI 写出正确的东西。
我能发现 AI 写错的地方。
我能把 AI 生成代码改造成自己的架构。
```

---

# 13. 当前阶段设计口诀

最重要的口诀：

```text
I/O 状态归 Selector
业务耗时归 Pool
资源生命周期归创建者
main 只负责组装
```

套到当前项目：

```text
TcpServer:
    管 listen_fd

Selector:
    管 select
    管 client_fd 集合
    管 read
    管 remove_client

ThreadPool:
    管线程
    管任务队列

BusinessHandler:
    管 data -> response
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

---

# 14. 下一步学习顺序

当前不建议马上跳 epoll。

推荐顺序：

```text
1. 修正当前 select + pool 的 fd 生命周期问题
2. 做多客户端和断开测试
3. 改 poll
4. 写 epoll 单线程 echo
5. 再做 epoll + pool
6. 最后演进 HTTP server
```

原因：

```text
poll/epoll 只是事件通知机制变化。
fd 生命周期、资源归属、任务边界问题不会自动消失。
```

当前优先解决：

```text
client_fd 谁拥有？
什么时候 close？
worker 能不能直接 write？
fd 关闭后任务队列中旧 fd 怎么办？
同一个 fd 能不能重复提交给线程池？
```

阶段路线：

```text
当前 select + pool：
    修 fd 生命周期和任务边界

poll + pool：
    用 vector<pollfd> 替换 fd_set/max_fd/rebuild_max_fd

epoll 单线程 echo：
    理解 epoll_ctl / epoll_wait

epoll + pool：
    pool 只做业务
    I/O 尽量回到 epoll 线程

HTTP server：
    解析请求
    生成响应
    支持静态文件和简单 API
```

---

# 15. 当前阶段成果总结

你已经从：

```text
能写一个阻塞 echo server
```

推进到：

```text
能理解多种并发模型的调用关系
能区分线程池阻塞模型和 select + pool 模型
能分析 fd 生命周期
能定位 select EBADF 风险
能判断 handle 是否职责过重
能理解 worker 直接 write 的风险
能通过 nc -N 验证客户端关闭
```

当前最重要的收获不是某一份代码，而是这套判断能力：

```text
模型边界
模块职责
资源归属
阻塞点
fd 生命周期
任务边界
```

这些判断能力进入 `poll`、`epoll`、HTTP server 后仍然适用。

---

# 16. 后续可追加章节

建议这份笔记后续继续追加：

```text
17. poll 版本设计笔记
18. epoll 单线程 echo 笔记
19. epoll + pool 笔记
20. HTTP 请求解析笔记
21. 连接状态与缓冲区设计
22. 日志模块设计
23. 定时器与超时连接清理
24. 压测记录与性能分析
25. Reactor 模型总结
```

---

# 17. 复习问题清单

可以定期用这些问题自测：

1. 为什么多线程模型中主线程不能随便 close client_fd？
2. 线程池 worker 为什么不能持有任务队列锁执行 task？
3. select 为什么每轮都要 `read_set = master_set`？
4. `select` 返回 ready_count 后，为什么还要遍历 fd？
5. `recv` 返回 0 表示什么？
6. 为什么 worker 不应该在当前模型中 close fd？
7. `closed_fds_` 为什么用完必须 clear？
8. 删除最大 fd 后为什么要 rebuild_max_fd？
9. `handle_client` 为什么要拆成 I/O 处理和 business 处理？
10. 当前 select + pool 简化模型有哪些工程风险？
11. 为什么 poll/epoll 不能自动解决 fd 生命周期问题？
12. 标准 Reactor 中，worker 应该负责什么，不应该负责什么？

---

# 18. 当前最小行动项

建议下一次项目推进只做这些：

```text
1. 修 closed_fds_.clear()
2. 删除 loop 末尾无条件 rebuild_max_fd()
3. select 出错时处理 EINTR
4. 更新 main 注释
5. 写 3 个测试：
   - 单客户端 echo
   - 多客户端并发 echo
   - nc -N 断开检测
6. 在代码中补充关键职责注释
```

完成后，再进入：

```text
poll 改写
```

而不是直接跳 epoll。

