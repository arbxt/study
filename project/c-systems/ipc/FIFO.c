#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

int main() {
    int m =  mkfifo("myfifo", 0644);
    if(m == -1) {
        perror("mkfifo failed");
        return 1;
    }
    return 0;
}