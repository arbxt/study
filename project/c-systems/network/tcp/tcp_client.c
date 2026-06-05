#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>

int main () {
    char buf[128];
    char msg[128];
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(8080);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if(fd == -1) {
        perror("socket");
        return -1;
    }
    int conn = connect(fd, (struct sockaddr *)&addr, sizeof(addr));
    if(conn == -1) {
        perror("connect");
        return -1;
    }

    while(fgets(msg, sizeof(msg), stdin)) {
        msg[strcspn(msg, "\n")] = 0; 
        write(fd, msg, strlen(msg));
        memset(buf, 0, sizeof(buf));
        int r = read(fd, buf, 128);
        if(strcmp(buf, "EXIT") == 0) {
            break;
        }
        write(1, buf, r);
        printf("\n");
    }
    close(fd);
    return 0;
}