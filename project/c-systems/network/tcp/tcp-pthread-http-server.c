#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <arpa/inet.h>

int total_requests = 0; // 全局变量，记录总请求数
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER; // 初始化锁

void *handle(void *arg){
    int cli_fd = *(int *)arg;
    free(arg); // 释放动态分配的内存
    
    pthread_detach(pthread_self()); // 分离线程，自动回收资源

    while(1){
        char buf[1024];

        int r = read(cli_fd, buf, sizeof(buf) - 1);
        if(r <= 0) break;

        pthread_mutex_lock(&lock);
        int current_id = ++total_requests; 
        pthread_mutex_unlock(&lock);
        usleep(100000); // 睡眠 100ms，模拟复杂的逻辑计算
        printf("Handled request #%d (fd = %d)\n", current_id, cli_fd);

        char body[256];
        sprintf(body, "<html><body><h1>Request #%d</h1></body></html>", total_requests);
        int body_len = strlen(body);

        char response[512];
        sprintf(response, 
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: text/html\r\n"
                "Content-Length: %d\r\n" 
                "Connection: keep-alive\r\n" 
                "\r\n%s", body_len, body);
        write(cli_fd, response, strlen(response));
    }
    close(cli_fd); // 关闭客户端连接
    return NULL;
}

int main() {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd < 0){
        perror("socket");
        return -1;
    }
    int opt = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)); // 设置地址复用
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(8080);
    if(bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0){
        perror("bind");
        return -1;
    }
    if(listen(sockfd, 128) < 0){
        perror("listen");
        return -1;
    }
    printf("TCP server listening on port %d (fd = %d)\n", 8080, sockfd);
    while(1){
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int *cli_fd = malloc(sizeof(int)); // 动态分配内存存储客户端文件描述符
        if(cli_fd == NULL){
            perror("malloc");
            continue;
        }
        *cli_fd = accept(sockfd, (struct sockaddr *)&client_addr, &client_len);
        if(*cli_fd < 0){
            perror("accept");
            free(cli_fd); // 释放内存
            continue;
        }
        printf("Accepted connection from %s:%d (fd = %d)\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port), *cli_fd);
        
        pthread_t tid;
        if(pthread_create(&tid, NULL, handle, cli_fd) != 0){ // 创建线程处理客户端请求
            perror("pthread_create");
            close(*cli_fd); // 关闭客户端连接
            free(cli_fd); // 释放内存
            continue;
        }
    }
}