#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <signal.h>
#include <sys/wait.h>

void reap_child(int sig) {
    while (waitpid(-1, NULL, WNOHANG) > 0);
}

void handle_http_request(int client_fd) {
    char buf[1024];
    read(client_fd, buf, sizeof(buf));
}

int main() {
    signal(SIGCHLD, reap_child);

    struct sockaddr_in sever, client;
    sever.sin_family = AF_INET;
    sever.sin_port = htons(8080);
    sever.sin_addr.s_addr = INADDR_ANY;

    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) {
        perror("socket");
        return -1;
    }

    bind(listen_fd, (struct sockaddr *)&sever, sizeof(sever));
    listen(listen_fd, 5);

    printf("Multi-process server listening on 8080\n");

    while (1) {
        socklen_t len = sizeof(client);
        int client_fd = accept(listen_fd, (struct sockaddr *)&client, &len);
        if (client_fd < 0) continue;

        pid_t pid = fork();

        if (pid == 0) { // child
            close(listen_fd);  // ⭐ 子进程不用监听

            char buf[512];
            int r = read(client_fd, buf, sizeof(buf) - 1);
            write(1, buf, r); // 输出到标准输出
            printf("\n");
            close(client_fd);
            _exit(0);
        }

        // parent
        close(client_fd);  // ⭐ 父进程不用 client_fd
    }
}
