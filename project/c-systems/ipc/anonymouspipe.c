#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>

int main() {
    int fd[2];
    char buf[1024];
    const char *msg = "Hello from parent process!";  // 无手动添加的 \0（字符串字面量自带，但 write 时可只传有效字节）

    if (pipe(fd) == -1) { perror("pipe failed"); return 1; }
    pid_t pid = fork();
    if (pid < 0) { perror("fork failed"); return 1; }

    if (pid == 0) {
        close(fd[1]);  // 子进程关闭写端
        ssize_t r = read(fd[0], buf, sizeof(buf));  // 读取原始字节流（不+1）
        if (r == -1) { perror("read failed"); exit(1); }

        // 写法1：直接 write 输出（无需 \0）
        printf("子进程（PID：%d）用 write 输出：\n", getpid());
        write(1, buf, r);  // 输出 r 个字节，正好是父进程写入的内容
        printf("\n");

        // 写法2：printf 输出（必须加 \0）
        buf[r] = '\0';  // 手动补结束符
        printf("子进程（PID：%d）用 printf 输出：%s\n", getpid(), buf);

        close(fd[0]);
        exit(0);
    } else {
        close(fd[0]);  // 父进程关闭读端
        write(fd[1], msg, strlen(msg));  // 只写有效字节（不含字符串自带的 \0）
        close(fd[1]);
        waitpid(pid, NULL, 0);
    }
    return 0;
}