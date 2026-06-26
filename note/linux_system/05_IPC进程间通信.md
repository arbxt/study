# 5. IPC：管道、FIFO、System V IPC

## 5.1 知识层次

```text
层次：进程间通信 / 内核对象 / 同步与共享
关键词：pipe、FIFO、message queue、shared memory、semaphore、ftok、ipcs、ipcrm
```

## 5.2 主线

进程地址空间彼此隔离，因此进程间通信需要借助内核提供的机制。

常见 IPC 可以按用途分：

```text
传字节流：pipe / FIFO
传结构化消息：消息队列
共享大量数据：共享内存
做同步互斥：信号量
```

## 5.3 管道 pipe

```c
int pipe(int pipefd[2]);
```

返回两个 fd：

```text
pipefd[0]：读端
pipefd[1]：写端
```

数据流：

```text
写进程 -> fd[1] -> 内核管道缓冲区 -> fd[0] -> 读进程
```

关键点：

```text
匿名管道通常用于亲缘进程，因为 fork 会继承 fd。
管道是半双工字节流。
数据在内核缓冲区中，不落盘。
```

阻塞规则：

```text
管道满：write 阻塞。
管道空且仍有写端打开：read 阻塞。
所有写端关闭：read 返回 0，表示 EOF。
读端关闭后继续 write：可能收到 SIGPIPE。
```

## 5.4 FIFO 命名管道

```c
int mkfifo(const char *pathname, mode_t mode);
```

特点：

```text
有文件系统路径。
不要求进程有亲缘关系。
文件节点本身不存数据，数据仍在内核管道缓冲区。
open 只读或只写时可能阻塞，直到另一端打开。
```

## 5.5 System V IPC

三种机制：

```text
消息队列：传输少量结构化消息。
共享内存：最快，适合大量数据，但本身不提供同步。
信号量：用于进程同步与互斥。
```

共同入口：

```c
key_t ftok(const char *pathname, int proj_id);
```

调试命令：

```bash
ipcs
ipcrm
```

### 5.5.1 消息队列

```c
int msgget(key_t key, int msgflg);
int msgsnd(int msqid, const void *msgp, size_t msgsz, int msgflg);
ssize_t msgrcv(int msqid, void *msgp, size_t msgsz, long msgtyp, int msgflg);
int msgctl(int msqid, int cmd, struct msqid_ds *buf);
```

关键点：

```text
消息必须以 long mtype 开头。
msgsz 是正文大小，不包含 mtype。
IPC_RMID 用于删除消息队列。
```

### 5.5.2 共享内存

```c
int shmget(key_t key, size_t size, int shmflg);
void *shmat(int shmid, const void *shmaddr, int shmflg);
int shmdt(const void *shmaddr);
int shmctl(int shmid, int cmd, struct shmid_ds *buf);
```

关键点：

```text
共享内存最快，因为进程直接读写映射区域。
共享内存本身不负责同步。
通常需要配合信号量、互斥锁或其他同步机制。
IPC_RMID 是标记删除，所有进程 detach 后才真正释放。
```

### 5.5.3 信号量

```c
int semget(key_t key, int nsems, int semflg);
int semop(int semid, struct sembuf *sops, size_t nsops);
int semctl(int semid, int semnum, int cmd, ...);
```

典型 P/V 操作：

```text
P / Wait：sem_op = -1
V / Signal：sem_op = +1
```

## 5.6 易错点

```text
1. 忘记关闭管道不用的一端，导致 read 永远等不到 EOF。
2. 把 FIFO 文件节点误认为保存了数据。
3. 消息队列 msgsz 错把 mtype 算进去。
4. 共享内存只解决共享，不解决同步。
5. System V IPC 对象随内核存在，需要手动 ipcrm 或 IPC_RMID。
```

## 5.7 与服务器项目的关系

```text
管道可用于父子进程通信、日志进程通信、自唤醒事件循环。
共享内存有助于理解高性能进程间数据共享。
信号量帮助理解同步原语。
服务器主线中更常用线程同步和 socket，但 IPC 是系统编程基础。
```

## 5.8 实践任务

```text
1. pipe_parent_child：父进程写，子进程读。
2. pipe_eof_demo：关闭写端，观察 read 返回 0。
3. fifo_chat：两个无亲缘进程通过 FIFO 通信。
4. shm_counter：两个进程共享计数器，再观察没有同步时的问题。
5. sem_mutex_demo：用 System V 信号量保护共享内存计数器。
```

---
