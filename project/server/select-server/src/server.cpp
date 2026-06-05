#include "server.h"

#include <iostream>
#include <cstring>

#include <unistd.h>       // close()
#include <sys/types.h>
#include <sys/socket.h>   // socket(), bind(), listen(), accept(), setsockopt()
#include <netinet/in.h>   // sockaddr_in
#include <arpa/inet.h>    // inet_ntoa(), htons(), htonl(), ntohs()
#include <errno.h>        // errno

TcpServer::TcpServer(unsigned short port)
    : port_(port),
      listen_fd_(-1)
{
}

TcpServer::~TcpServer()
{
    if (listen_fd_ != -1) {
        close(listen_fd_);
        listen_fd_ = -1;
    }
}

/*
 * 启动服务器。
 */
bool TcpServer::start()
{
    /*
     * 创建监听 socket。
     */
    listen_fd_ = socket(AF_INET, SOCK_STREAM, 0);

    if (listen_fd_ == -1) {
        std::cerr << "socket 创建失败: "
                  << std::strerror(errno)
                  << std::endl;
        return false;
    }

    /*
     * 设置端口复用。
     */
    int opt = 1;

    if (setsockopt(
            listen_fd_,
            SOL_SOCKET,
            SO_REUSEADDR,
            &opt,
            sizeof(opt)
        ) == -1) {
        std::cerr << "setsockopt 失败: "
                  << std::strerror(errno)
                  << std::endl;
        return false;
    }

    /*
     * 准备服务器地址。
     */
    sockaddr_in server_addr;
    std::memset(&server_addr, 0, sizeof(server_addr));

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(port_);

    /*
     * 绑定 IP 和端口。
     */
    if (bind(
            listen_fd_,
            reinterpret_cast<sockaddr*>(&server_addr),
            sizeof(server_addr)
        ) == -1) {
        std::cerr << "bind 失败: "
                  << std::strerror(errno)
                  << std::endl;
        return false;
    }

    /*
     * 进入监听状态。
     */
    if (listen(listen_fd_, 128) == -1) {
        std::cerr << "listen 失败: "
                  << std::strerror(errno)
                  << std::endl;
        return false;
    }

    std::cout << "服务器启动成功，监听端口 "
              << port_
              << std::endl;

    return true;
}

/*
 * 阻塞等待客户端连接。
 */
int TcpServer::accept_client()
{
    sockaddr_in client_addr;
    std::memset(&client_addr, 0, sizeof(client_addr));

    socklen_t client_addr_len = sizeof(client_addr);

    int client_fd = accept(
        listen_fd_,
        reinterpret_cast<sockaddr*>(&client_addr),
        &client_addr_len
    );

    if (client_fd == -1) {
        if (errno == EINTR) {
            return -1;
        }

        std::cerr << "accept 失败: "
                  << std::strerror(errno)
                  << std::endl;
        return -1;
    }

    std::cout << "客户端已连接: "
              << inet_ntoa(client_addr.sin_addr)
              << ":"
              << ntohs(client_addr.sin_port)
              << ", client_fd = "
              << client_fd
              << std::endl;

    return client_fd;
}

int TcpServer::listen_fd() const
{
    return listen_fd_;
}