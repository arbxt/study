#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

int main() {
    int fd;
    struct sockaddr_in addr;
    char buf[1024];

    fd = socket(AF_INET, SOCK_DGRAM, 0);

    addr.sin_family = AF_INET;
    addr.sin_port = htons(8080);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

    while (fgets(buf, sizeof(buf), stdin)) {
        sendto(fd, buf, strlen(buf), 0,
               (struct sockaddr*)&addr, sizeof(addr));

        int n = recvfrom(fd, buf, sizeof(buf), 0, NULL, NULL);
        write(1, buf, n);
    }
}
