# Linux 系统编程笔记 v2：文件、进程、内存、链接、调试、IPC、信号

> 来源：由 `文件操作.md`、`进程.md`、`内存管理.md`、`信号.md` 与 `笔记1.docx` 重组整理。  
> 用途：作为 study 项目中 `linux_system_notes.md` 的第二版，补入编译链接、动态库、GDB、errno、readelf/objdump 与 malloc 相关内容。  
> 目标：不再按 API 零散记忆，而是围绕“进程如何通过系统调用使用内核资源”建立知识地图。

---

## 1. 总主线

Linux 系统编程可以先抓住一句话：

```text
进程通过文件描述符、虚拟地址空间、信号和 IPC 机制，与内核管理的资源交互。
```

这句话可以拆成五条主线：

```text
程序构建：预处理 / 编译 / 汇编 / 链接 / 装载 / 调试
文件系统：路径 / inode / 文件描述符 / read-write
进程控制：fork / wait / exec / exit / 环境变量
内存管理：虚拟地址空间 / 页表 / mmap / malloc / 生命周期
IPC：pipe / FIFO / 消息队列 / 共享内存 / 信号量
信号：异步通知 / 阻塞与未决 / 可重入 / 会话与守护进程
```

这些知识最终都服务于你的服务器项目：

```text
socket 也是 fd
select/poll/epoll 管理 fd
多进程服务器涉及 fork/wait/SIGCHLD
线程池服务器涉及共享地址空间和同步
HTTP server 会涉及文件读取、mmap、日志、配置、守护进程化
```

---


# 1. 程序构建、链接、调试与错误处理

## 1.1 知识层次

```text
层次：C/C++ 工具链 / ELF 文件 / 链接装载 / 调试 / Linux 错误处理
关键词：static library、shared library、gcc、ar、-fPIC、-shared、ldd、dlopen、gdb、errno、readelf、objdump
```

## 1.2 主线

C/C++ 程序从源代码到运行中的进程，大致经历：

```text
源文件 .c/.cpp
    -> 预处理
    -> 编译成汇编
    -> 汇编成目标文件 .o
    -> 链接成可执行文件
    -> 装载到进程虚拟地址空间
    -> 执行、调试、处理错误
```

这部分知识连接了三个方向：

```text
语言层：函数、全局变量、静态变量、extern、头文件声明
系统层：ELF、段、符号、重定位、动态链接器、虚拟内存
工程层：库的制作、链接错误定位、部署依赖、GDB 调试、errno 排错
```

对你的服务器项目来说，这部分决定了：

```text
如何组织多文件 C/C++ 项目；
如何把线程池、网络模块、日志模块拆成库；
如何定位 undefined reference、找不到 .so、段错误；
如何用 gdb/objdump/readelf 观察程序行为。
```

---

## 1.3 静态库：链接时复制进可执行文件

静态库通常以 `.a` 为后缀，本质上是一组目标文件 `.o` 的归档。

制作流程：

```bash
gcc -c lib.c -o lib.o
ar -rcs libmylib.a lib.o
```

链接使用时，链接器会从静态库中取出需要的目标代码，合并进最终可执行文件。

### 优点

```text
部署简单：运行时不依赖外部 .so。
源码保护：调用方只需要头文件和库文件。
构建加速：库本身不必每次重新编译。
```

### 缺点

```text
不节省最终可执行文件体积。
多个程序静态链接同一库时，代码不能天然共享。
库升级麻烦：库更新后通常需要重新链接应用程序。
```

### 工程理解

头文件负责：

```text
告诉编译器函数/类型长什么样。
```

库文件负责：

```text
在链接阶段提供函数/变量的真正定义。
```

所以只 include 头文件但没有链接库，常见结果是：

```text
undefined reference to xxx
```

---

## 1.4 动态库：运行时由动态链接器装载

动态库通常以 `.so` 为后缀。

制作流程：

```bash
gcc -fPIC -c foo.c -o foo.o
gcc -shared -o libfoo.so foo.o
```

也可以合并写成：

```bash
gcc foo.c -shared -fPIC -o libfoo.so
```

其中：

```text
-fPIC：生成位置无关代码，便于共享库被映射到不同进程地址空间。
-shared：生成共享库，而不是普通可执行文件。
```

### 动态库的意义

```text
可执行文件中不直接复制库代码。
多个进程可以共享同一份库代码页。
库升级相对灵活。
程序运行时需要能找到对应 .so。
```

### 运行时查找动态库的方法

常见方式：

```text
1. 设置 LD_LIBRARY_PATH。
2. 编译链接时设置 rpath。
3. 将库放入系统库路径，例如 /usr/lib。
4. 配置 /etc/ld.so.conf 后运行 ldconfig。
```

排查依赖：

```bash
ldd ./main
```

### 常见误区

```text
动态库不是“编译时完全链接进去”。
编译链接阶段会记录符号和依赖关系，运行时由动态链接器完成装载、重定位和符号解析。
```

---

## 1.5 动态加载：dlopen / dlsym / dlclose

动态加载允许程序运行过程中主动打开某个 `.so`，再查找其中的函数。

头文件：

```c
#include <dlfcn.h>
```

典型流程：

```c
void *handle = dlopen("./libfoo.so", RTLD_NOW);
void *sym = dlsym(handle, "foo");
dlclose(handle);
```

常见模式：

```text
插件系统
可选功能模块
运行时加载算法/业务模块
```

服务器项目后续可以把它作为“了解即可”：

```text
现在不必实现插件系统；
但要知道 nginx/apache/数据库驱动/图形程序里经常有类似思想。
```

---

## 1.6 GDB 调试基础

编译时加调试信息：

```bash
gcc main.c -g -o main
# 或者
gcc main.c -ggdb -o main
```

常用命令：

| 命令 | 含义 |
|---|---|
| `l` / `list` | 列出源码 |
| `b` / `break` | 设置断点，可跟行号或函数名 |
| `r` / `run` | 启动程序 |
| `n` / `next` | 单步执行，不进入函数 |
| `s` / `step` | 单步执行，进入函数 |
| `p` / `print` | 打印变量 |
| `bt` | 查看调用栈 |
| `q` / `quit` | 退出 |

### 服务器项目中的用法

```text
1. 程序崩溃时看 backtrace。
2. fd 被误关时，在 close/remove_client 处打断点。
3. select 返回 EBADF 时，检查 client_fds_ 和 master_set_。
4. 线程池任务异常时，检查任务队列和 worker 状态。
```

---

## 1.7 errno 与系统调用错误处理

C/Linux 系统调用通常使用这种约定：

```text
成功：返回有效值。
失败：返回 -1，并设置 errno。
```

`errno` 保存最近一次错误码。

常用写法：

```c
if (ret == -1) {
    perror("open");
}
```

或：

```cpp
if (ret == -1) {
    std::cerr << "open failed: " << std::strerror(errno) << std::endl;
}
```

### 关键注意

```text
errno 只在函数明确失败时才有意义。
不要在成功返回后随便检查 errno。
errno 只保存最后一次错误，后续库函数调用可能覆盖它。
```

### 与服务器项目的关系

```text
accept/read/write/recv/send/select/poll/epoll_wait 都需要检查返回值和 errno。
EINTR：被信号中断，通常可重试或 continue。
EAGAIN/EWOULDBLOCK：非阻塞 I/O 暂时无数据。
EBADF：fd 无效，常见于 fd 生命周期管理错误。
```

---

## 1.8 readelf / objdump：观察编译产物

常见 ELF 段：

| 段 | 含义 |
|---|---|
| `.text` | 代码段 |
| `.rodata` | 只读数据，例如字符串常量、全局 const 数据 |
| `.data` | 已初始化的全局/静态变量 |
| `.bss` | 未初始化或初始化为 0 的全局/静态变量 |

查看 ELF 信息：

```bash
readelf -S ./main
readelf -s ./main
```

反汇编：

```bash
objdump -d ./main
```

工程意义：

```text
理解全局变量、静态变量、常量、函数代码最终放在哪里。
理解链接错误、符号查找、动态库依赖。
调试底层问题时，不只停留在源码层面。
```

---

## 1.9 当前阶段必须掌握 / 了解即可

### 必须掌握

```text
静态库和动态库的区别。
头文件声明和库文件定义的区别。
undefined reference 的本质。
LD_LIBRARY_PATH / ldd 的基本使用。
gdb 的断点、单步、打印变量、查看调用栈。
errno 的使用规则。
.text/.data/.bss/.rodata 与内存布局关系。
```

### 近期需要补

```text
Makefile 中如何链接静态库/动态库。
C++ 项目中 include 路径和 lib 路径如何组织。
write_all / read_loop 中如何处理 EINTR 和短写。
```

### 了解即可

```text
dlopen/dlsym 插件式加载。
ELF 重定位细节。
动态链接器内部实现。
objdump 反汇编深入分析。
```

## 1.10 实践任务

```text
1. 写 add.c/add.h/main.c，分别制作静态库和动态库。
2. 故意不链接库，观察 undefined reference。
3. 删除 LD_LIBRARY_PATH，观察运行时报错，再用 ldd 排查。
4. 用 gdb 调试一个数组越界或空指针程序，看 bt。
5. 用 readelf -S 查看 .text/.data/.bss/.rodata。
6. 在服务器项目中给 select/recv/send 的错误处理统一加 strerror(errno)。
```

---

# 2. 文件系统与文件描述符

## 2.1 知识层次

```text
层次：Linux 系统调用 / 文件系统 / fd 抽象 / I/O 模型
关键词：inode、硬链接、软链接、stat、DIR、open、read、write、lseek、dup、dup2
```

## 2.2 主线

文件不仅有内容，还有元数据。Linux 文件系统通过 inode 描述文件本体，通过目录项把“文件名”映射到 inode。

```text
路径名 -> 目录项 -> inode -> 文件元数据 + 数据块
```

进程并不是直接操作“文件名”，而是通过 `open()` 获得一个文件描述符：

```text
进程 fd 表中的整数 fd -> 打开的文件对象 -> inode / socket / pipe / device
```

因此，fd 是理解文件、socket、pipe、重定向和 I/O 多路复用的共同入口。

## 2.3 必须掌握

### 2.3.1 inode、硬链接、软链接

```text
inode：文件本体的元数据索引。
硬链接：多个目录项指向同一个 inode。
软链接：一个独立文件，内容是另一个路径。
```

常见理解误区：

```text
文件名不是文件本身。
文件名只是目录中的一条记录。
真正标识文件对象的是 inode。
```

### 2.3.2 stat

```c
int stat(const char *path, struct stat *buf);
```

用途：获取文件元数据，例如 inode、权限、类型、大小、时间、属主等。

工程意义：

```text
HTTP 静态文件服务需要判断路径是否存在、是否为普通文件、文件大小是多少。
日志系统可能需要检查日志文件状态。
目录遍历工具需要结合 stat 判断文件类型。
```

### 2.3.3 目录流

```c
DIR *opendir(const char *name);
struct dirent *readdir(DIR *dirp);
int closedir(DIR *dirp);
```

目录本质上也是一种特殊文件，保存文件名到 inode 的映射。`readdir()` 每次读取一个目录项。

工程意义：

```text
静态资源服务器需要遍历目录。
文件同步工具、ls 简化版、项目扫描工具都会用到目录流。
```

### 2.3.4 open / close / read / write / lseek

核心系统调用：

```c
int open(const char *pathname, int flags, ...);
int close(int fd);
ssize_t read(int fd, void *buf, size_t count);
ssize_t write(int fd, const void *buf, size_t count);
off_t lseek(int fd, off_t offset, int whence);
```

关键点：

```text
open 成功返回 fd，失败返回 -1 并设置 errno。
read 返回实际读到的字节数，返回 0 通常表示 EOF。
write 返回实际写入的字节数，不保证一次写完所有数据。
lseek 只适合可随机访问对象，不适合 pipe/socket 等流式对象。
```

## 2.4 易错点

### 易错点 1：把 fd 当成文件本身

fd 只是当前进程 fd 表中的整数索引。

```text
fd -> 打开的文件对象 -> inode / socket / pipe
```

关闭 fd 只是让当前进程不再通过这个索引访问对应对象。

### 易错点 2：忽略 read/write 的实际返回值

错误倾向：

```c
read(fd, buf, sizeof(buf));
write(fd, buf, sizeof(buf));
```

更稳妥思路：

```text
read 后只处理返回值 n 指示的字节数。
write 后检查实际写入了多少，必要时循环写。
```

### 易错点 3：目录权限和文件权限混淆

目录的权限含义不是“读写内容”这么简单：

```text
r：能读取目录项列表。
w：能在目录中创建/删除/重命名目录项，通常还需要 x 配合。
x：能进入目录、能通过该目录访问路径。
```

## 2.5 与服务器项目的关系

```text
socket fd 和普通文件 fd 一样，都进入进程 fd 表。
select/poll/epoll 监听的就是 fd。
HTTP 静态文件服务最终会用 open/read/write 或 mmap/sendfile。
日志模块会用 open/write/close。
配置文件读取会用 open/read 或 C++ fstream。
```

## 2.6 实践任务

```text
1. 写 my_ls：使用 opendir/readdir/stat 打印文件名、类型、大小。
2. 写 copy_file：使用 open/read/write 实现文件复制，处理短写。
3. 写 redirect_demo：使用 dup2 把 stdout 重定向到文件。
4. 在服务器项目中增加一个读取静态文件的小函数：read_file_to_string(path)。
```

---

# 3. 进程控制

## 3.1 知识层次

```text
层次：操作系统 / 进程生命周期 / 进程资源管理 / 程序装载
关键词：atexit、exit、wait、waitpid、fork、exec、环境变量、僵尸进程
```

## 3.2 主线

进程是操作系统分配和管理资源的基本单位。一个进程拥有：

```text
PID
虚拟地址空间
fd 表
信号处理方式
环境变量
当前工作目录
用户权限信息
```

进程生命周期可以简化为：

```text
创建 -> 执行 -> 退出 -> 父进程回收
```

在 Unix 编程中，常见组合是：

```text
fork() 复制当前进程
exec() 替换为新程序
wait/waitpid() 回收子进程
```

## 3.3 必须掌握

### 3.3.1 atexit / on_exit

```c
int atexit(void (*function)(void));
int on_exit(void (*function)(int, void *), void *arg);
```

用途：注册正常终止时执行的清理函数。

关键点：

```text
后注册先执行。
注册多次会执行多次。
fork 后子进程会继承已注册的退出处理函数。
```

工程意义：

```text
适合做简单资源清理。
但服务器项目中更推荐 RAII 或明确的 shutdown 流程。
```

### 3.3.2 wait / waitpid

```c
pid_t wait(int *wstatus);
pid_t waitpid(pid_t pid, int *wstatus, int options);
```

用途：父进程回收子进程资源，避免僵尸进程。

关键状态宏：

```text
WIFEXITED / WEXITSTATUS：正常退出与退出码。
WIFSIGNALED / WTERMSIG：被信号终止。
WIFSTOPPED / WSTOPSIG：被暂停。
WIFCONTINUED：从暂停恢复。
```

`waitpid` 比 `wait` 更灵活：

```text
可以等待指定 pid。
可以使用 WNOHANG 非阻塞回收。
适合 SIGCHLD handler 中循环回收子进程。
```

### 3.3.3 exec

```c
int execve(const char *pathname, char *const argv[], char *const envp[]);
```

核心含义：

```text
用新程序替换当前进程映像。
PID 不变。
原来的代码段、数据段、堆、栈会被新程序替换。
成功时不返回。
失败才返回 -1。
```

常见变体：

```text
execl / execlp / execle / execv / execvp / execvpe
```

常见组合：

```c
pid_t pid = fork();
if (pid == 0) {
    execlp("ls", "ls", "-l", NULL);
    _exit(127);
}
waitpid(pid, NULL, 0);
```

### 3.3.4 环境变量

```text
每个进程有自己的环境变量表。
子进程默认继承父进程环境变量。
exec 时可以选择继承或替换环境变量。
```

常用函数：

```c
getenv();
setenv();
unsetenv();
putenv();
clearenv();
```

工程意义：

```text
服务端配置可以通过环境变量传入，例如 PORT、DB_HOST、LOG_LEVEL。
部署时常用环境变量区分开发环境和生产环境。
```

## 3.4 易错点

### 易错点 1：exec 成功后还会继续执行原代码

不会。`exec` 成功后原程序映像被替换，只有失败时才返回。

### 易错点 2：fork 后忘记区分父子进程路径

```c
if (pid == 0) {
    // child
} else if (pid > 0) {
    // parent
} else {
    // error
}
```

子进程如果不 `exit/_exit`，可能继续执行父进程逻辑。

### 易错点 3：多进程服务器不处理 SIGCHLD

不回收子进程会产生僵尸进程。

## 3.5 与服务器项目的关系

```text
多进程服务器模型依赖 fork/wait/SIGCHLD。
守护进程化涉及 fork、setsid、chdir、umask、关闭 fd。
执行外部命令、CGI 或脚本时需要 fork + exec。
服务配置可从环境变量读取。
```

## 3.6 实践任务

```text
1. 写 fork_demo：打印父子进程 PID，观察执行路径。
2. 写 exec_demo：父进程 fork，子进程 exec ls，父进程 waitpid。
3. 写 zombie_demo：故意不 wait，观察 ps 中的僵尸进程。
4. 写 sigchld_reap_demo：使用 waitpid(-1, ..., WNOHANG) 回收多个子进程。
```

---

# 4. 虚拟内存与 mmap

## 4.1 知识层次

```text
层次：操作系统 / 内存管理 / 进程地址空间 / 系统调用
关键词：虚拟内存、MMU、页表、TLB、缺页中断、Swap、mmap、munmap
```

## 4.2 主线

裸机程序可以直接使用物理地址；有操作系统后，进程通常看到的是虚拟地址空间。

虚拟内存的核心是：

```text
虚拟地址空间 + 地址映射机制 + 页面置换策略
```

它解决的问题：

```text
进程隔离
内存保护
地址空间连续性
按需加载
扩展可用内存
```

## 4.3 必须掌握

### 4.3.1 地址翻译

典型过程：

```text
CPU 产生虚拟地址
MMU 查询页表 / TLB
命中则得到物理地址
未命中可能触发缺页中断
内核加载页面并更新页表
指令重新执行
```

核心概念：

| 概念 | 含义 |
|---|---|
| Page | 虚拟内存页，常见大小 4KB |
| Frame | 物理页框 |
| Page Table | 虚拟页到物理页的映射表 |
| TLB | 页表项缓存，加速地址翻译 |
| Page Fault | 访问尚未映射或权限不符的页面时触发 |

### 4.3.2 进程地址空间

典型布局：

```text
高地址
内核空间
共享库 / mmap 区
栈
堆
BSS 段
数据段
代码段
低地址
```

生命周期：

```text
局部变量 / 形参：位于栈帧，函数返回后失效。
全局变量 / static：位于数据段或 BSS，生命周期与进程相同。
动态分配内存：位于堆，生命周期由 malloc/free 或 new/delete 控制。
只读常量：可能位于只读数据段，通过页表权限保护。
```

### 4.3.3 mmap / munmap

```c
void *mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset);
int munmap(void *addr, size_t length);
```

用途：把文件或匿名内存映射到进程虚拟地址空间。

常见 flags：

```text
MAP_SHARED：修改对其他映射进程可见，并可同步回文件。
MAP_PRIVATE：写时复制，修改不影响原文件和其他进程。
MAP_ANONYMOUS：匿名映射，不关联具体文件。
```

注意：原笔记中的 `MAP_SHRED` 应修正为 `MAP_SHARED`。

## 4.4 易错点

### 易错点 1：以为 mmap 是“把物理地址映射到进程”

更准确地说，`mmap` 是在进程虚拟地址空间中建立一段映射区域。底层物理页何时分配、是否从文件加载，由内核和页表机制管理。

### 易错点 2：混淆 MAP_SHARED 和 MAP_PRIVATE

```text
MAP_SHARED：适合进程间共享、文件映射写回。
MAP_PRIVATE：适合只读加载或私有修改，常见写时复制。
```

### 易错点 3：忘记 munmap

`mmap` 得到的映射区不是 `free()` 释放，而是 `munmap()`。

## 4.5 与服务器项目的关系

```text
HTTP 静态文件服务可以用 mmap 加速文件读取。
进程地址空间帮助理解 fork 的写时复制。
线程共享同一进程地址空间，解释为什么线程池需要锁。
内存生命周期帮助理解 C++ RAII、智能指针、缓冲区管理。
```

## 4.6 实践任务

```text
1. 写 mmap_read_file：用 mmap 读取文件并输出。
2. 写 mmap_copy：用 mmap 实现文件复制。
3. 写 private_shared_demo：比较 MAP_SHARED 和 MAP_PRIVATE 的修改效果。
4. 写 fork_cow_demo：fork 后修改全局变量，观察父子进程值不同。
```

---


## 4.7 从编译产物到进程内存映像

编译链接阶段生成的 ELF 文件，在运行时会被装载到进程的虚拟地址空间中。

可以先建立这个对应关系：

```text
ELF .text    -> 进程代码段
ELF .rodata  -> 只读数据区域
ELF .data    -> 已初始化全局/静态变量区域
ELF .bss     -> 未初始化或零初始化全局/静态变量区域
运行时 malloc/new -> 堆
函数调用 -> 栈帧
mmap/动态库 -> mmap 区域
```

这能解释一个常见现象：

```text
同一个程序可以运行多次，并且看起来使用“相同地址”。
原因是每个进程都有独立虚拟地址空间，虚拟地址相同不代表物理地址相同。
```

## 4.8 malloc 与 brk / mmap 的关系

`malloc()` 不是系统调用，而是 C 标准库在用户态提供的内存分配器接口。glibc 常用实现是 ptmalloc 系列。

简化理解：

```text
小块内存：通常从堆 arena 中切分，arena 不够时可能通过 brk 扩展堆。
大块内存：可能直接通过 mmap 向内核申请独立映射。
```

关键点：

```text
malloc 返回的是用户可用区域的起始地址。
分配器会在用户区域附近维护元数据。
越界写不仅可能破坏你的数据，也可能破坏 malloc 的管理信息。
free 时可能因此崩溃。
```

所以 C/C++ 中这类写法非常危险：

```c
char *p = malloc(10);
p[10] = 'x';   // 越界，可能破坏分配器元数据
free(p);       // 可能在这里崩溃
```

与 C++ 的关系：

```text
new/delete 底层通常仍会使用运行库分配器。
RAII 和智能指针不是为了“让 malloc 消失”，而是减少手动释放和生命周期错误。
```

## 4.9 当前阶段对内存的掌握边界

必须掌握：

```text
虚拟地址空间不是物理内存。
栈、堆、全局区、代码段、mmap 区的大致位置和生命周期。
malloc/free 的生命周期由程序员控制，越界写可能破坏分配器元数据。
mmap 可以把文件或匿名内存映射到进程虚拟地址空间。
```

暂时不必深究：

```text
ptmalloc bin 结构。
tcache/fastbin/smallbin/largebin 细节。
ELF 装载器完整实现。
页表多级结构的所有位字段。
```

---

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

# 6. 信号、会话与守护进程

## 6.1 知识层次

```text
层次：异步事件 / 进程控制 / Unix 作业控制 / 守护进程
关键词：signal、sigaction、SIGCHLD、SIGPIPE、SIGALRM、sigprocmask、pending、reentrant、setsid、daemon
```

## 6.2 主线

信号是 Unix/Linux 提供的一种异步通知机制。

```text
内核或进程向目标进程发送信号。
目标进程可以默认处理、忽略或捕获处理。
```

常见来源：

```text
键盘：Ctrl+C 产生 SIGINT。
命令：kill -SIGNAL pid。
函数：kill、raise、alarm。
内核事件：子进程退出产生 SIGCHLD，管道读端关闭后写入可能产生 SIGPIPE。
```

## 6.3 必须掌握

### 6.3.1 signal

```c
typedef void (*sighandler_t)(int);
sighandler_t signal(int signum, sighandler_t handler);
```

handler 可以是：

```text
SIG_DFL：默认处理。
SIG_IGN：忽略。
自定义函数：捕获信号。
```

工程建议：

```text
教学阶段可以先用 signal。
严肃工程中更推荐 sigaction。
```

### 6.3.2 alarm / pause

```c
unsigned int alarm(unsigned int seconds);
int pause(void);
```

用途：

```text
alarm 设置定时 SIGALRM。
pause 挂起等待信号。
```

与网络服务器的关系：

```text
早期可用 alarm 理解定时事件。
现代服务器更常用 timerfd、epoll timeout、时间轮或最小堆定时器。
```

### 6.3.3 信号阻塞与未决

信号集 `sigset_t` 可以看作位图。

常用函数：

```c
sigemptyset();
sigfillset();
sigaddset();
sigdelset();
sigismember();
sigprocmask();
sigpending();
```

基本模型：

```text
信号产生。
如果未被阻塞，立即按默认/忽略/捕获处理。
如果被阻塞，进入未决集合。
解除阻塞后再处理。
```

### 6.3.4 可重入函数

信号处理函数可能打断普通代码执行，因此 handler 中只能调用异步信号安全的函数。

危险行为：

```text
调用 printf。
调用 malloc/free。
操作复杂全局状态。
加普通锁。
```

推荐原则：

```text
信号处理函数尽量只设置 sig_atomic_t 标志位。
真正逻辑放到主循环中处理。
```

## 6.4 会话与守护进程

会话 session 是一组进程组的集合，可能关联一个控制终端。终端关闭时，相关进程可能收到 `SIGHUP`。

守护进程的目标：

```text
脱离控制终端。
后台长期运行。
不受登录 shell 退出影响。
```

核心步骤通常包括：

```text
fork
父进程退出
setsid 创建新会话
chdir 改工作目录
umask 设置权限掩码
关闭或重定向标准 fd
进入主循环
```

## 6.5 易错点

```text
1. 在 signal handler 中做太多事情。
2. 不了解 EINTR，系统调用被信号打断后没有重试或正确处理。
3. 多进程服务器忽略 SIGCHLD，导致僵尸进程。
4. socket 写入对端已关闭连接时忽略 SIGPIPE 风险。
5. 把 Ctrl+D 当成信号；它通常表示终端输入 EOF，不是信号。
```

## 6.6 与服务器项目的关系

```text
select/recv/write 可能被信号打断，返回 -1 且 errno = EINTR。
多进程服务器需要处理 SIGCHLD。
写 socket 时可能遇到 SIGPIPE。
服务器优雅退出可用 SIGINT/SIGTERM 设置退出标志。
守护进程化会涉及 setsid、SIGHUP、关闭标准 fd。
```

## 6.7 实践任务

```text
1. sigint_flag_demo：Ctrl+C 后不立即退出，只设置退出标志。
2. alarm_demo：用 alarm + pause 理解异步信号。
3. sigmask_demo：阻塞 SIGINT，观察 pending，再解除阻塞。
4. sigchld_demo：子进程退出后父进程用 waitpid(WNOHANG) 回收。
5. sigpipe_demo：关闭管道读端后写入，观察 SIGPIPE。
```

---

# 7. 当前阶段优先级

## 7.1 必须掌握

```text
fd 是什么。
open/read/write/close 的返回值语义。
fork 后父子进程 fd 表和地址空间关系。
wait/waitpid 如何回收子进程。
exec 成功不返回。
虚拟地址空间的基本布局。
栈、堆、全局变量、static 的生命周期。
pipe 的 EOF 和阻塞规则。
信号会打断系统调用，产生 EINTR。
```

## 7.2 近期需要补

```text
dup2 重定向。
stat + 目录遍历。
mmap 文件映射。
SIGCHLD 正确回收子进程。
SIGPIPE 与 socket 写入错误处理。
守护进程基本流程。
```

## 7.3 了解即可

```text
System V IPC 的完整 API 细节。
复杂信号屏蔽场景。
页面置换算法细节。
传统 daemon 双 fork 细节。
```

---

# 8. 与 select + threadpool 服务器的连接

你的服务器主项目可以把这些知识串起来：

```text
fd：listen_fd、client_fd 都是文件描述符。
read/write：recv/send 是 socket 版本的 I/O。
进程：多进程服务器模型会遇到 fork/wait/SIGCHLD。
内存：线程池共享进程地址空间，所以任务队列要加锁。
IPC：pipe 可用于唤醒事件循环或日志进程通信。
信号：select/recv 需要处理 EINTR，优雅退出要处理 SIGINT/SIGTERM。
```

最重要的工程口诀：

```text
系统调用看返回值。
fd 生命周期要成组维护。
信号处理要克制。
共享数据必须考虑同步。
资源释放要有明确归属。
```

---

# 9. 后续整理计划

下一步可以继续从这份 `linux_system_notes.md` 拆出专题实验：

```text
linux_file_lab.md
linux_process_lab.md
linux_memory_lab.md
linux_ipc_lab.md
linux_signal_lab.md
```

也可以继续把每个主题补成：

```text
详细版：原理 + API + 实验
复习版：一页速查
行动版：本周要写的代码
```


---

# 10. 本次整合记录：`笔记1.docx` 的归档位置

`笔记1.docx` 中的内容不是单独开一份“杂项笔记”，而是按知识层次并入现有体系：

| 原始主题 | 并入位置 | 整理原则 |
|---|---|---|
| 静态库 `.a` | 第 1 章：程序构建、链接、调试与错误处理 | 从“命令记录”提升为“链接阶段复制目标代码” |
| 动态库 `.so` | 第 1 章：程序构建、链接、调试与错误处理 | 强调 `-fPIC`、运行时依赖、`ldd`、部署路径 |
| `dlopen/dlsym/dlclose` | 第 1 章：动态加载 | 作为插件机制的了解内容 |
| GDB 命令 | 第 1 章：GDB 调试基础 | 与服务器 fd/线程池调试结合 |
| `errno` | 第 1 章：错误处理 | 与系统调用返回值、`EINTR/EBADF` 联系 |
| `readelf/objdump` | 第 1 章 + 第 4 章 | 连接 ELF 段和进程内存布局 |
| `malloc`、arena、元数据 | 第 4 章：虚拟内存与 mmap | 强调越界写破坏分配器元数据 |
| 虚拟内存 32 位地址空间 | 第 4 章：虚拟内存与 mmap | 与“每个进程独立虚拟地址空间”合并 |
| 环境变量 | 第 3 章：进程控制 | 与 `exec`、shell、动态库查找联系 |

本次整合后的主线变为：

```text
源代码如何变成程序
程序如何装载成进程
进程如何拥有虚拟地址空间和 fd 表
进程如何通过系统调用操作文件、socket、IPC、信号
服务器项目如何在这些机制上构建
```
