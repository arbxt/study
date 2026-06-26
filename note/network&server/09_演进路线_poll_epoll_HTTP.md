# 演进路线：poll、epoll 与 HTTP Server

## 1. 当前不要急着跳 epoll

原因：

```text
poll/epoll 改变的是事件通知机制。
不会自动解决 fd 生命周期、连接状态、任务边界、缓冲区管理。
```

当前应该先把 select + threadpool 中的这些问题弄清楚：

```text
client_fd 谁拥有？
worker 能不能 close？
worker 能不能 write？
如何避免 fd 复用风险？
如何处理半包、多包？
如何保存连接状态？
```

## 2. poll 版本目标

把：

```text
fd_set master_set_
fd_set read_set_
max_fd_
FD_SET / FD_CLR / FD_ISSET
```

替换为：

```cpp
std::vector<pollfd> fds_;
poll(fds_.data(), fds_.size(), timeout);
```

重点理解：

```text
poll 没有 FD_SETSIZE 限制。
pollfd 数组直接保存 fd 和事件。
仍然需要手动删除关闭的 fd。
```

## 3. epoll 单线程 Echo 目标

先不要一上来 epoll + threadpool。

建议先写：

```text
epoll 单线程 Echo Server
```

掌握：

```text
epoll_create1
epoll_ctl ADD/MOD/DEL
epoll_wait
epoll_event.data.fd
```

## 4. epoll + threadpool 目标

不要让 worker 直接长期 read。

更合理模型：

```text
I/O 线程：
    epoll_wait
    read 到连接读缓冲
    解析完整请求
    提交业务任务

worker：
    处理请求
    生成响应
    把响应交回 I/O 线程或连接写缓冲

I/O 线程：
    监听 EPOLLOUT
    发送写缓冲内容
```

## 5. HTTP Server 目标

Echo Server 到 HTTP Server 的关键变化：

```text
字节流 -> HTTP 请求解析
简单 echo -> response 生成
client_fd -> Connection 对象
一次 recv -> read_buffer 累积
一次 write -> write_buffer + 可写事件
裸日志 -> 日志模块
硬编码端口 -> 配置文件
手动测试 -> 压测与回归测试
```

## 6. 推荐阶段路线

```text
阶段 1：修正 select_pool 注释和小问题
阶段 2：补充测试和压测记录
阶段 3：poll 改写
阶段 4：epoll 单线程 Echo
阶段 5：引入 Connection 对象
阶段 6：epoll + threadpool，worker 不直接 write
阶段 7：HTTP 请求解析
阶段 8：静态文件 + 简单 API
阶段 9：日志、配置、定时器
阶段 10：数据库/缓存接入
```

## 7. 每阶段验收标准

### select_pool 稳定版

```text
多客户端 echo 正常。
客户端断开能正确 remove。
select 不出现 EBADF。
注释与模型一致。
```

### poll 版

```text
去掉 fd_set 和 max_fd。
使用 vector<pollfd> 管理连接。
断开删除正确。
```

### epoll 单线程版

```text
epoll_ctl ADD/DEL 正确。
epoll_wait 能处理 listen_fd 和 client_fd。
```

### HTTP 初版

```text
浏览器访问能返回固定 HTTP 响应。
curl 能看到正确状态行和 Header。
```
