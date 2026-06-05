/*
 * select_server.cpp
 *
 * Linux / Ubuntu 环境下的 select I/O 复用 TCP Echo Server。
 *
 * 功能：
 * 1. 单线程监听 8080 端口
 * 2. 使用 select 同时管理监听 socket 和多个客户端 socket
 * 3. 哪个客户端发数据，就处理哪个客户端
 * 4. 客户端发什么，服务器回什么
 *
 * 编译：
 *      g++ -Wall -Wextra -std=c++11 select_server.cpp -o select_server
 *
 * 运行：
 *      ./select_server
 *
 * 测试：
 *      nc 127.0.0.1 8080
 */

#include <iostream>
#include <cstring>
#include <vector>
#include <algorithm>

#include <unistd.h>       // close(), read(), write()
#include <sys/types.h>
#include <sys/socket.h>   // socket(), bind(), listen(), accept(), setsockopt()
#include <netinet/in.h>   // sockaddr_in
#include <arpa/inet.h>    // htons(), htonl(), inet_ntoa()
#include <sys/select.h>   // select(), fd_set
#include <errno.h>

static bool write_all(int fd, const char* buffer, ssize_t length)
{
    ssize_t total_written = 0;

    while (total_written < length) {
        ssize_t n = write(fd, buffer + total_written, length - total_written);

        if (n > 0) {
            total_written += n;
        } else if (n == -1) {
            if (errno == EINTR) {
                continue;
            }

            return false;
        }
    }

    return true;
}

int main()
{
    /*
     * 第 1 步：创建监听 socket。
     */
    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);

    if (listen_fd == -1) {
        std::cerr << "socket 创建失败: "
                  << std::strerror(errno)
                  << std::endl;
        return 1;
    }

    /*
     * 第 2 步：设置端口复用。
     */
    int opt = 1;

    if (setsockopt(
            listen_fd,
            SOL_SOCKET,
            SO_REUSEADDR,
            &opt,
            sizeof(opt)
        ) == -1) {
        std::cerr << "setsockopt 失败: "
                  << std::strerror(errno)
                  << std::endl;
        close(listen_fd);
        return 1;
    }

    /*
     * 第 3 步：绑定地址。
     */
    sockaddr_in server_addr;
    std::memset(&server_addr, 0, sizeof(server_addr));

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(8080);

    if (bind(
            listen_fd,
            reinterpret_cast<sockaddr*>(&server_addr),
            sizeof(server_addr)
        ) == -1) {
        std::cerr << "bind 失败: "
                  << std::strerror(errno)
                  << std::endl;
        close(listen_fd);
        return 1;
    }

    /*
     * 第 4 步：监听。
     */
    if (listen(listen_fd, 128) == -1) {
        std::cerr << "listen 失败: "
                  << std::strerror(errno)
                  << std::endl;
        close(listen_fd);
        return 1;
    }

    std::cout << "select I/O 复用服务器启动，监听端口 8080..."
              << std::endl;















              
    /*
     * master_set：
     *      保存所有需要监听读事件的 fd。
     *
     * read_set：
     *      每次传给 select 的临时集合。
     *
     * 注意：
     *      select 会修改传入的 fd_set。
     *      所以每次循环都要把 master_set 复制给 read_set。
     */
    fd_set master_set;
    fd_set read_set;

    FD_ZERO(&master_set);
    FD_ZERO(&read_set);

    /*
     * 把监听 socket 加入 master_set。
     *
     * listen_fd 可读表示：
     *      有新的客户端连接可以 accept。
     */
    FD_SET(listen_fd, &master_set);

    /*
     * max_fd 记录当前监听集合中最大的 fd。
     * select 的第一个参数需要 max_fd + 1。
     */
    int max_fd = listen_fd;

    /*
     * 保存所有客户端 fd。
     *
     * 这不是 select 必需的，但方便我们遍历客户端。
     */
    std::vector<int> client_fds;

    while (true) {
        /*
         * 每次调用 select 前，都要重新复制一份。
         *
         * 因为 select 返回后，read_set 中只保留“就绪”的 fd，
         * 原始集合会被破坏。
         */
        read_set = master_set;

        /*
         * 第 5 步：select 等待事件。
         *
         * 参数说明：
         *
         * max_fd + 1：
         *      select 需要检查的 fd 范围是 [0, max_fd]。
         *
         * &read_set：
         *      关注读事件。
         *
         * nullptr：
         *      不关注写事件。
         *
         * nullptr：
         *      不关注异常事件。
         *
         * nullptr：
         *      没有超时时间，一直阻塞等待事件发生。
         */
        int ready_count = select(
            max_fd + 1,
            &read_set,
            nullptr,
            nullptr,
            nullptr
        );

        if (ready_count == -1) {
            if (errno == EINTR) {
                continue;
            }

            std::cerr << "select 失败: "
                      << std::strerror(errno)
                      << std::endl;
            break;
        }

        /*
         * 第 6 步：判断 listen_fd 是否可读。
         *
         * 如果 listen_fd 可读，说明有新客户端连接。
         */
        if (FD_ISSET(listen_fd, &read_set)) {
            sockaddr_in client_addr;
            std::memset(&client_addr, 0, sizeof(client_addr));

            socklen_t client_addr_len = sizeof(client_addr);

            int client_fd = accept(
                listen_fd,
                reinterpret_cast<sockaddr*>(&client_addr),
                &client_addr_len
            );

            if (client_fd == -1) {
                std::cerr << "accept 失败: "
                          << std::strerror(errno)
                          << std::endl;
            } else {
                /*
                 * select 有 FD_SETSIZE 限制。
                 * 常见 Linux 环境中 FD_SETSIZE 通常是 1024。
                 */
                if (client_fd >= FD_SETSIZE) {
                    std::cerr << "client_fd 超过 FD_SETSIZE 限制，关闭连接: "
                              << client_fd
                              << std::endl;
                    close(client_fd);
                } else {
                    /*
                     * 把新的客户端 fd 加入监听集合。
                     */
                    FD_SET(client_fd, &master_set);
                    client_fds.push_back(client_fd);

                    if (client_fd > max_fd) {
                        max_fd = client_fd;
                    }

                    std::cout << "客户端已连接: "
                              << inet_ntoa(client_addr.sin_addr)
                              << ":"
                              << ntohs(client_addr.sin_port)
                              << ", client_fd = "
                              << client_fd
                              << std::endl;
                }
            }

            /*
             * ready_count 表示本轮就绪 fd 数量。
             * 已经处理了 listen_fd，就减一。
             */
            --ready_count;

            if (ready_count <= 0) {
                continue;
            }
        }

        /*
         * 第 7 步：遍历客户端 fd，处理可读事件。
         *
         * 注意：
         *      这里不要对所有 0 ~ max_fd 都处理业务。
         *      我们用 client_fds 记录客户端 fd，遍历更清晰。
         */
        std::vector<int> closed_fds;

        for (int client_fd : client_fds) {
            /*
             * 如果当前 client_fd 本轮没有事件，跳过。
             */
            if (!FD_ISSET(client_fd, &read_set)) {
                continue;
            }

            char buffer[1024];
            std::memset(buffer, 0, sizeof(buffer));

            /*
             * 由于 select 已经告诉我们 client_fd 可读，
             * 所以这里 read 通常不会阻塞。
             *
             * 但是严格来说，如果 fd 被其他线程修改，或者出现特殊情况，
             * 阻塞 fd 上的 read 仍可能有边界问题。
             *
             * 本示例是单线程模型，逻辑上是安全的。
             */
            ssize_t bytes_read = read(client_fd, buffer, sizeof(buffer) - 1);

            if (bytes_read > 0) {
                std::cout << "收到 client_fd = "
                          << client_fd
                          << " 数据: "
                          << std::string(buffer, bytes_read)
                          << std::endl;

                /*
                 * Echo：原样写回客户端。
                 *
                 * 注意：
                 *      当前代码仍然使用阻塞 write。
                 *      如果客户端接收很慢，write 仍有可能阻塞。
                 *
                 * 更完整的 I/O 复用服务器应该也监听写事件，
                 * 并维护每个连接的发送缓冲区。
                 */
                if (!write_all(client_fd, buffer, bytes_read)) {
                    std::cerr << "write 失败，关闭 client_fd = "
                              << client_fd
                              << ", error = "
                              << std::strerror(errno)
                              << std::endl;

                    closed_fds.push_back(client_fd);
                }

            } else if (bytes_read == 0) {
                /*
                 * read 返回 0，表示客户端关闭连接。
                 */
                std::cout << "客户端关闭连接，client_fd = "
                          << client_fd
                          << std::endl;

                closed_fds.push_back(client_fd);

            } else {
                /*
                 * read 返回 -1，表示出错。
                 */
                if (errno == EINTR) {
                    continue;
                }

                std::cerr << "read 失败，关闭 client_fd = "
                          << client_fd
                          << ", error = "
                          << std::strerror(errno)
                          << std::endl;

                closed_fds.push_back(client_fd);
            }

            --ready_count;

            if (ready_count <= 0) {
                break;
            }
        }

        /*
         * 第 8 步：统一关闭失效客户端。
         *
         * 为什么不在遍历 client_fds 时直接 erase？
         *
         * 因为一边遍历 vector，一边删除元素容易导致迭代器失效。
         * 所以先记录到 closed_fds，最后统一删除。
         */
        for (int fd : closed_fds) {
            close(fd);
            FD_CLR(fd, &master_set);

            client_fds.erase(
                std::remove(client_fds.begin(), client_fds.end(), fd),
                client_fds.end()
            );

            /*
             * 如果关闭的是当前 max_fd，需要重新计算 max_fd。
             */
            if (fd == max_fd) {
                max_fd = listen_fd;

                for (int client_fd : client_fds) {
                    if (client_fd > max_fd) {
                        max_fd = client_fd;
                    }
                }
            }
        }
    }

    for (int fd : client_fds) {
        close(fd);
    }

    close(listen_fd);

    return 0;
}