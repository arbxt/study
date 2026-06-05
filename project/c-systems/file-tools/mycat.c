#include<stdio.h>
#include<fcntl.h>
#include<unistd.h>
#include<sys/types.h>
#include<sys/stat.h>

int main(int argc, char *argv[]) {
    int fd;
    int ret;
    char buffer[1024];
    fd = open(argv[1], O_RDONLY);
    if(fd == -1) {
        perror("Error opening file");
        return -1;
    }
    printf("File opened successfully with file descriptor: %d\n", fd);
    while ((ret = read(fd, buffer, sizeof(buffer))) > 0)
        write(STDOUT_FILENO, buffer, ret);

    if (ret == -1) {
        perror("Error reading file");
        close(fd);
        return -1;
    }
    printf("\n");
    close(fd);
    return 0;
}