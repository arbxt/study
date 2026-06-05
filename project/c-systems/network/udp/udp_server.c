#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

int main() {
    int fd;
    struct sockaddr_in server, cli;
    char buf[1024];

    fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd == -1) {
        perror("socket");
        return -1;
    }

    server.sin_family = AF_INET;
    server.sin_port = htons(8080);
    server.sin_addr.s_addr = INADDR_ANY;

    if (bind(fd, (struct sockaddr*)&server, sizeof(server)) == -1) {
        perror("bind");
        close(fd);
        return -1;
    }

    printf("UDP server running on port 8080...\n");

    while (1) {
        socklen_t len = sizeof(cli);
        memset(buf, 0, sizeof(buf));

        int n = recvfrom(fd, buf, sizeof(buf), 0,
                         (struct sockaddr*)&cli, &len);

        printf("Received from %s:%d -> %s\n",
               inet_ntoa(cli.sin_addr),
               ntohs(cli.sin_port),
               buf);

        // echo back
        sendto(fd, buf, n, 0,
               (struct sockaddr*)&cli, len);
    }

    close(fd);
    return 0;
}
