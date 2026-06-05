#include <stdio.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <string.h>
#include <sys/shm.h>

typedef struct msgbuf
{
    /* data */
    long mtype;
    char mtext[128];
}msgbuf_t;


int main() {
    int size = 4096;
    char *string = "test of shared memory";
    key_t key = ftok(".",'A');
    if(key == -1) {
        perror("ftok");
        return -1;
    }
    int shmid = shmget(key, size,IPC_CREAT|0644);
    printf("msgpid=%d\n",shmid);
    char *write = shmat(shmid, NULL,0);
    strcpy(write,string);
    int flag = shmdt(write);
    if(!flag) printf("detach success\n");
    return 0;
}