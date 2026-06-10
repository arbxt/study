#pragma once

#include <string>


class TcpServer {
public:
   
    explicit TcpServer(unsigned short port);


    ~TcpServer();

    /*
     * 启动服务器。

     * 内部完成：
     *      socket()
     *      setsockopt()
     *      bind()
     *      listen()

     * 返回值：
     *      true：启动成功
     *      false：启动失败
     */
    bool start();

    /*
     * 接收一个客户端连接。
     *
     * 返回值：
     *      成功：client_fd
     *      失败：-1
     *
     * 注意：
     *      accept_client() 是阻塞函数。
     */
    int accept_client();

    int listen_fd() const;
    
    TcpServer(const TcpServer&) = delete;
    TcpServer& operator=(const TcpServer&) = delete;

private:
    unsigned short port_;
    int listen_fd_;
};
