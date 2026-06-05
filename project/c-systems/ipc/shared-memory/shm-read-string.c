// 必须包含的头文件（核心：<sys/msg.h> 声明 msgrcv/msgget）
#include <sys/msg.h>    // msgrcv、msgget、msgbuf 结构体声明
#include <sys/shm.h>
#include <sys/ipc.h>    // IPC_CREAT、IPC_NOWAIT 等宏定义
#include <stdio.h>      // printf、perror
#include <string.h>     // memset
#include <errno.h>      // errno 错误码
#include <stddef.h>     // ssize_t 类型定义

// 定义消息结构体（msgrcv 必须使用该格式，首字段为 long mtype）
struct msgbuf {
    long mtype;         // 消息类型（必须 > 0）
    char mtext[128];    // 消息正文，与 msgrcv 的 128 对应
};

int main() {
    key_t key;
    int shmid;
    char *receive;  // 接收消息的缓冲区
    int flag;           // 修复：返回值类型改为 ssize_t

    // 1. 生成合法的 key（或使用 IPC_PRIVATE）
    key = ftok(".", 'A');    // 基于当前目录 + 项目ID 生成唯一 key
    if (key == -1) {
        perror("ftok failed");
        return 1;
    }

    // 2. 获取消息队列标识符（替代硬编码的 2）
    shmid = shmget(key, 4096, IPC_CREAT | 0644);  // 0666 是队列权限
    if (shmid == -1) {
        perror("shmget failed");
        return 1;
    }
    printf("msqid=%d\n",shmid);
    // 4. 调用 msgrcv（参数修正 + 类型匹配）
    // 参数说明：msqid, 缓冲区, mtext长度, 消息类型, 标志位
    receive = shmat(shmid, NULL, SHM_RDONLY);

    // 5. 打印接收结果
    printf("成功接收消息：%s\n",receive);
    flag = shmdt(receive);
    if(!flag) printf("detach success\n");
    return 0;
}