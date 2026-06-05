# include <stdio.h>
 #include <sys/ipc.h>

int main() {
    __key_t key = ftok(".",257);
    if (key == -1)
    {
        perror("ftok");
        return -1;
    }
    printf("key=%d\n",key);
    return 0;
}